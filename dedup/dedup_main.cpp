#include "llc_path.h"
#include "llc_file.h"
#include "llc_string_compose.h"
#include "llc_string.h"
#include "llc_timer.h"
#include "llc_minmax.h"
#include "llc_runtime.h"

sttc	::llc::err_t	dedup_entry_point		(::llc::SRuntimeValues & runtimeValues);
LLC_SYSTEM_OS_ENTRY_POINT(::dedup_entry_point);

LLC_USING_TYPEINT();
LLC_USING_APOD();
LLC_USING_VIEW();

stxp llc::vcst_t                DEFAULT_TARGET_FOLDER       = LLC_CXS("./Duplicated");
stxp llc::minmax<u3_t>          DEFAULT_FILE_SIZE_RANGE     = {0x400 * 0x400 * 5, (u3_t)-1};

struct SFileInfo {
	llc::string    Path;
	llc::string    Name;
	uint64_t       Size;
	uint64_t       TimestampCreation;
	uint64_t       TimestampLastAccess;
};
struct SFileInfoPair {
    SFileInfo       A;
    SFileInfo       B;
};

struct SFileInfoMove {
    s2_t            FileInfoIndex;
    llc::string     SourcePath;
    llc::string     FolderPath;
    llc::string     FileName;
};

struct SDedupApp {
    llc::SCommandLineArgs           CommandLineArgs;
    llc::minmax<u3_t>               FileSizeRangeInBytes    = DEFAULT_FILE_SIZE_RANGE;
    llc::vcst_t                     TargetFolder            = DEFAULT_TARGET_FOLDER;
    llc::aobj<llc::string>          PathsToProcess;
    llc::aobj<SFileInfoPair>        ExactMatches;
    llc::aobj<SFileInfo>            FilesMoved;

};

stxp u2_c       COMPARISON_CHUNK_SIZE_MAX   = 0x400 * 0x400 * 0x100; 

// FILE_ATTRIBUTE_READONLY (0x1): The file is read-only. Applications can read the file but cannot write to it or delete it.
// FILE_ATTRIBUTE_HIDDEN (0x2): The file is hidden and not included in an ordinary directory listing.
// FILE_ATTRIBUTE_SYSTEM (0x4): The file is part of, or is used exclusively by, the operating system.
// FILE_ATTRIBUTE_DIRECTORY (0x10): The item is a directory (folder) rather than a file.
// FILE_ATTRIBUTE_ARCHIVE (0x20): The file should be archived. Windows sets this flag whenever a file is created or modified.
// FILE_ATTRIBUTE_NORMAL (0x80): The file has no other attributes set. This flag is only valid if it is used alone.
// FILE_ATTRIBUTE_REPARSE_POINT (0x400): The file or directory has an associated reparse point, or is a symbolic link/junction point.

sttc llc::err_t listFolder   (llc::vcst_t path, bool recursiveWalk, llc::function<llc::err_t(const WIN32_FIND_DATAA&, llc::vcst_c)> callback) {
	llc::rtrim(path, path, "/\\");
	llc::string 		 pathStr;
    if_fail_fe(llc::append_strings(pathStr, path, "/*"));

    WIN32_FIND_DATAA    data    = {};
    HANDLE              hFind   = FindFirstFileExA(pathStr, FindExInfoBasic, &data, FindExSearchNameMatch, 0, 0);
    if_true_vif(0, hFind == INVALID_HANDLE_VALUE, "No files found in \"%s\"", path.begin());

    uint32_t            filesFound = 0;
    do {
        verbose_printf(
            "\ndwFileAttributes   (DWORD)    : %u"
            "\nftCreationTime     (FILETIME) : %llu"
            "\nftLastAccessTime   (FILETIME) : %llu"
            "\nftLastWriteTime    (FILETIME) : %llu"
            "\nnFileSizeHigh      (DWORD)    : %u"
            "\nnFileSizeLow       (DWORD)    : %u"
            "\ndwReserved0        (DWORD)    : %u"
            "\ndwReserved1        (DWORD)    : %u"
            "\ncFileName          (CHAR[%u]) : \"%s\""
            "\ncAlternateFileName (CHAR[14]) : \"%s\"" 
            , data.dwFileAttributes
            , *(const uint64_t*)&data.ftCreationTime
            , *(const uint64_t*)&data.ftLastAccessTime
            , *(const uint64_t*)&data.ftLastWriteTime
            , data.nFileSizeHigh
            , data.nFileSizeLow
            , data.dwReserved0
            , data.dwReserved1
            , MAX_PATH, data.cFileName
            , data.cAlternateFileName
            );
        SYSTEMTIME st = {};
        FileTimeToSystemTime(&data.ftCreationTime   , &st); verbose_printf("ftCreationTime   : %04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        FileTimeToSystemTime(&data.ftLastAccessTime , &st); verbose_printf("ftLastAccessTime : %04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        FileTimeToSystemTime(&data.ftLastWriteTime  , &st); verbose_printf("ftLastWriteTime  : %04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
		if(callback) {
			if_fail_ef(callback(data, path), "Callback failed for file \"%s\".", data.cFileName);
		}
        if(recursiveWalk && (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && '.' != data.cFileName[0]) {
			pathStr.clear();
			if_fail_fe(llc::append_strings(pathStr, path, "/", data.cFileName));
			if_fail_bef(listFolder(pathStr, recursiveWalk, callback), "Failed to list folder \"%s\".", pathStr.begin());
        }
        ++filesFound;
    }
    while(FindNextFileA(hFind, &data));
    FindClose(hFind);

    rtrn filesFound;
}

sttc llc::err_t compareFileContents(const SFileInfoPair& pair) {
	u2_c            comparisonChunkSize         = (pair.A.Size > COMPARISON_CHUNK_SIZE_MAX) ? COMPARISON_CHUNK_SIZE_MAX : (u2_c)pair.A.Size;
	info_printf("\nComparing file contents for: " 
        "\n%s/%s" 
        "\n%s/%s"
        "\nFile  size: %llu bytes."
        "\nChunk size: %llu bytes."
        , pair.A.Path.begin(), pair.A.Name.begin(), pair.B.Path.begin(), pair.B.Name.begin()
        , pair.A.Size, (uint64_t)comparisonChunkSize
        );
	sttc llc::au0_t      bufferA, bufferB;
    if_fail_fef(llc::resize(comparisonChunkSize, bufferA, bufferB), "comparisonChunkSize:(%u)", comparisonChunkSize);
	struct SAutoCloseFile {
		FILE* Handle = nullptr;
		~SAutoCloseFile() { if(Handle) fclose(Handle); }
	};
    SAutoCloseFile  A = {}, B = {};
    llc::string     filePathA, filePathB;
    if_fail_fe(llc::append_strings(filePathA, pair.A.Path, "/", pair.A.Name));
    if_fail_fe(llc::append_strings(filePathB, pair.B.Path, "/", pair.B.Name));
	if_true_fwf(0 != fopen_s(&A.Handle, filePathA, "rb"), "Failed to open file: \"%s\"", filePathA.begin());
	if_true_fwf(0 != fopen_s(&B.Handle, filePathB, "rb"), "Failed to open file: \"%s\"", filePathB.begin());
    llc::STimer     timer           = {};
	u3_t            remainingBytes  = pair.A.Size;
	while(remainingBytes) {
		u3_c            bytesToRead = (remainingBytes > comparisonChunkSize) ? comparisonChunkSize : remainingBytes;
		if_true_fe(bytesToRead != fread(bufferA.begin(), 1, bytesToRead, A.Handle));
		if_true_fe(bytesToRead != fread(bufferB.begin(), 1, bytesToRead, B.Handle));
		if_true_vi(1, memcmp(bufferA.begin(), bufferB.begin(), bytesToRead)); // Files are different
		remainingBytes -= bytesToRead;
        timer.Frame();
        info_printf("Chunk comparison execution time: %f seconds.", timer.LastTimeMicroseconds * 0.000001);
	}
    return 0; // Files are identical
}

sttc llc::err_t movePaths(llc::string & oldPath, llc::string & destinationFolder, llc::vcst_t sourcePath, llc::vcst_t sourceName, llc::vcst_t destinationRoot) {
	if_fail_fe(llc::append_strings(oldPath, sourcePath, "/", sourceName));

    llc::vcst_t         oldPathView     = sourcePath;
	llc::ltrim(oldPathView, oldPathView, "\\/");
    s2_t                colonOffset     = oldPathView.find(':', 0);
    llc::string         oldPathDriveLetterFix;
    if(colonOffset >= 0) {
        if_fail_fe(llc::append_strings(oldPathDriveLetterFix, oldPathView));
		oldPathDriveLetterFix[colonOffset] = '_';
        oldPathView = oldPathDriveLetterFix;
    }
	if_fail_fe(llc::append_strings(destinationFolder, destinationRoot, '/', oldPathView.cc(), '/'));
    return 0;
}

sttc llc::err_t moveFile(const SFileInfo & fileToMove, llc::vcst_c & destinationRoot) {
    llc::string         oldPath         = {};
    llc::string         newPath         = {};
    if_fail_fe(movePaths(oldPath, newPath, fileToMove.Path, fileToMove.Name, destinationRoot));
	if_fail_wf(llc::pathCreate(newPath, '/'), "Failed to create directory \"%s\"", newPath.begin());
	if_fail_fe(llc::append_strings(newPath, fileToMove.Name));
	if_zero_fwf(MoveFileExA(oldPath.begin(), newPath.begin(), MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH), "Failed to move duplicated file \"%s\" to \"%s\"", oldPath.begin(), newPath.begin());
    info_printf("moved: \"%s\" -> \"%s\"", oldPath.begin(), newPath.begin());
    return 0;
}

sttc llc::err_t collectExactMatches(llc::view<const SFileInfoPair> potentiallyDuplicatedFiles, llc::aobj<SFileInfoPair> & exactMatches) {
    llc::STimer                 timer;
    for(const auto & pair : potentiallyDuplicatedFiles) {
        verbose_printf("Processing potentially duplicated files with size %llu: %s/%s and %s/%s"
            , pair.A.Size
            , pair.A.Path.begin(), pair.A.Name.begin()
            , pair.B.Path.begin(), pair.B.Name.begin()
            );
        
        llc::err_t                  comparisonResult = 0;
		if_fail_ce(comparisonResult = compareFileContents(pair))
	    else if(comparisonResult)
			verbose_printf("Files are different: %s/%s and %s/%s"
				, pair.A.Path.begin(), pair.A.Name.begin()
				, pair.B.Path.begin(), pair.B.Name.begin()
			);
        else {
			info_printf("Files are identical: %s/%s and %s/%s"
				, pair.A.Path.begin(), pair.A.Name.begin()
				, pair.B.Path.begin(), pair.B.Name.begin()
			);
			if_fail_fe(exactMatches.push_back(pair));
		}
        timer.Frame();
        info_printf("compareFileContents execution time: %f seconds.", timer.LastTimeMicroseconds * 0.000001);
    }    
    return 0;
}

sttc llc::err_t collectLargeFiles(llc::vcst_t pathToProcess, llc::minmax<u3_t> fileSizeRangeInBytes, llc::aobj<SFileInfo> & largeFiles) {
    info_printf("Scanning path: \"%.*s\"", pathToProcess.size(), pathToProcess.begin());
    if_fail_fe(::listFolder(pathToProcess, true, [&largeFiles, fileSizeRangeInBytes](const WIN32_FIND_DATAA & entryData, llc::vcst_t folderPath) { 
        SFileInfo               newInfo         = {};
        newInfo.Size        = ((uint64_t)entryData.nFileSizeHigh << 32) | entryData.nFileSizeLow;
		if( newInfo.Size < fileSizeRangeInBytes.Min 
         || newInfo.Size > fileSizeRangeInBytes.Max
         ) // if the file is smaller than the minimum Size or larger than the maximum Size, skip it
            return 0; 
        memcpy(&newInfo.TimestampCreation   , &entryData.ftCreationTime     , sizeof(uint64_t));
        memcpy(&newInfo.TimestampLastAccess , &entryData.ftLastAccessTime   , sizeof(uint64_t));
        verbose_printf("Large file found: %s. Size: %llu. Creation Time: %llu", entryData.cFileName, newInfo.Size, newInfo.TimestampCreation);
        if_fail_fe(llc::append_strings(newInfo.Path, folderPath));
        if_fail_fe(llc::append_strings(newInfo.Name, entryData.cFileName));
        if_fail_fe(largeFiles.push_back(newInfo));
        return 1;
        }));
    return 0;
}

sttc llc::err_t collectPotentialDuplicates(llc::view<const SFileInfo> largeFiles, llc::aobj<SFileInfoPair> & potentiallyDuplicatedFiles) {
	info_printf("Comparing large files...");
    for(uint32_t iFile0 = 0; iFile0 < largeFiles.size() - 1; ++iFile0) 
    for(uint32_t iFile1 = iFile0 + 1; iFile1 < largeFiles.size(); ++iFile1) {
        if(largeFiles[iFile0].Size != largeFiles[iFile1].Size) 
            continue; 

        verbose_printf("Found possibly duplicated large files: %s/%s and %s/%s"
            , largeFiles[iFile0].Path.begin(), largeFiles[iFile0].Name.begin()
            , largeFiles[iFile1].Path.begin(), largeFiles[iFile1].Name.begin()
            );
        SFileInfoPair pair = { largeFiles[iFile0], largeFiles[iFile1] };
        if_fail_fe(potentiallyDuplicatedFiles.push_back(pair));
    }
    return 0;
}

sttc bool discardPath(llc::view<vcst_t> inputs, u2_t index, llc::vcst_c & path) {
    for(u2_t iOther = 0; iOther < inputs.size(); ++iOther) {
        if(iOther == index)
            continue;

        const auto & other = inputs[iOther];
        if(other == path) {
            if(iOther < index)
                return true;
            continue;
        }

        if(other.size() == 0 || other.size() >= path.size())
            continue;
        if(memcmp(path.begin(), other.begin(), other.size()))
            continue;

        if(other[other.size() - 1] == '/' || path[other.size()] == '/')
            return true;
    }
    return false;
}

sttc llc::err_t filterPathRoots(
    llc::aobj<llc::string> & output,
    llc::view<llc::vcst_t> inputs
) {
    if_fail_fe(inputs.enumerate([&](u2_t index, llc::vcst_c & path) {
        if_true_vif(1, discardPath(inputs, index, path), "Path discarded: \"%.*s\"", path.size(), path.begin());
        if_fail_fe(output.push_back(path));
        return 0;
    }));
    return 0;
}
sttc llc::err_t filterPathRoots(llc::aobj<llc::string> & output, llc::view<llc::string> inputs ) {
    llc::aobj<vcst_t> views;
    if_fail_fe(inputs.for_each([&](llc::string & path) { return views.push_back(path); }));
    return filterPathRoots(output, views);
}

#include <filesystem>
#include <string>

sttc llc::err_t pathAbsolute
    ( llc::aobj<llc::string>  & outputPaths
    , llc::vcst_c             & inputPath
    , llc::sc_c               separatorChar = '/'
    ) {
    if_zero_vw(1, inputPath.size());

    std::string     text            (inputPath.begin(), inputPath.size());
    sc_t            charToReplace   = ('/' == separatorChar) ? '\\' : '/';
    for(char & character : text) {
        if(character == charToReplace) 
            character = separatorChar; 
    }
    try {
        std::filesystem::path   absolutePath    = std::filesystem::absolute(text).lexically_normal();
        u2_c                    rootLength      = (u2_t)absolutePath.root_path().generic_string().size();
        text                = absolutePath.generic_string();
        while(text.size() > rootLength && text.back() == separatorChar)
            text.pop_back();
    }
    catch(const std::filesystem::filesystem_error & e) { 
        error_printf("Failed to resolve path for \"%.*s\":\"%s\"", (int)inputPath.size(), inputPath.begin(), e.what());
        return 1;
    }
    llc::err_t      index;
    if_fail_fe(index = outputPaths.push_back({}));
    llc::string     & normalized    = outputPaths[index];
    if_fail_fe(llc::append_strings(normalized, llc::vcst_t{text.c_str(), (u2_t)text.size()}));
    info_printf("Normalized path \"%.*s\" -> \"%.*s\"", (int)inputPath.size(), inputPath.begin(), (int)normalized.size(), normalized.begin());
    return 0;
}

stin llc::err_t pathListAbsolute(llc::aobj<llc::string> & outputPaths, llc::view<const llc::vcst_t> inputs) {
    return inputs.for_each([&outputPaths, inputs](const llc::vcst_t & input) { return pathAbsolute(outputPaths, input); });
}

sttc llc::err_t executeDedup(SDedupApp & appState) {
    llc::aobj<llc::string> scanPaths;
    if_fail_fe(pathListAbsolute(scanPaths, appState.CommandLineArgs.Positionals));
    if_fail_fe(filterPathRoots(appState.PathsToProcess, scanPaths));

    info_printf("\nComparing files of sizes between %llu and %llu"
        , appState.FileSizeRangeInBytes.Min
        , appState.FileSizeRangeInBytes.Max
        );
    appState.PathsToProcess.for_each([](llc::vcst_c & path) { info_printf("Path to process: \"%s\"", path.begin()); return 0; });

    llc::aobj<SFileInfo> largeFiles;
    for(const auto & pathToProcess : appState.PathsToProcess)
        if_fail_fe(collectLargeFiles(pathToProcess, appState.FileSizeRangeInBytes, largeFiles));
    info_printf("Total large files found: %u", largeFiles.size());
    
    llc::aobj<SFileInfoPair>    potentiallyDuplicatedFiles;
	if(largeFiles.size() > 1)
		if_fail_fe(collectPotentialDuplicates(largeFiles, potentiallyDuplicatedFiles));
    info_printf("Total potentially duplicated large files found: %u", potentiallyDuplicatedFiles.size());
	
    if_fail_fe(collectExactMatches(potentiallyDuplicatedFiles, appState.ExactMatches));
    info_printf("Duplicated files found: %u", appState.ExactMatches.size());
    //llc::aobj<SFileInfoMove>    filesToMove;
    //appState.ExactMatches.for_each([&filesToMove](const SFileInfoPair & filePair) { 
    //    filePair.B;
    //    filesToMove.;
    //    return 0; 
    //    });

    if_fail_vi(1, llc::argsOptionValue(appState.CommandLineArgs, "move", appState.TargetFolder));
    if_zero_fwf(appState.TargetFolder.size(), "-move requires a destination folder.");
	info_printf("\nTarget folder    : \"%s\"", appState.TargetFolder.begin());

    for(const auto & pair : appState.ExactMatches) {
	    const SFileInfo     & fileToMove        = pair.B;
        //llc::string         destinationFolder   = {};
	    if_fail_wf(moveFile(fileToMove, appState.TargetFolder), "Failed to move file \"%s\".", fileToMove.Name.begin());
		if_fail_fe(appState.FilesMoved.push_back(fileToMove));
    }
    return 0;
}

sttc llc::err_t displayHelp(SDedupApp & appState) {
    if(appState.CommandLineArgs.Options.size() <= 1) {
	    info_printf(
            "\nUsage: %s [-move=<target-folder>] [--] <source-folder> [<source-folder> ...] "
            "\nHelp: %s -help [<command> [<command> ...]]"
		    "\nThis program searches for large files in the specified paths and identifies potentially duplicated files based on their size."
		    "\nIf duplicates are found, it compares their contents and moves one of the duplicates to a target folder."
		    , appState.CommandLineArgs.ProgramName.begin()
		    , appState.CommandLineArgs.ProgramName.begin()
            );
        return 0;
    }
    info_printf("\nHelp for command line options:");
    for(const auto & option : appState.CommandLineArgs.Options) {
        if(option.Key == LLC_CXS("help"))
			continue;

             if(option.Key == LLC_CXS("move")) info_printf("\n-%s: %s", option.Key.begin(), "[add -move instructions here");
		else if(option.Key == LLC_CXS("file")) info_printf("\n-%s: %s", option.Key.begin(), "[add -file instructions here");
		else if(option.Key == LLC_CXS("dump")) info_printf("\n-%s: %s", option.Key.begin(), "[add -dump instructions here");
		else if(option.Key == LLC_CXS("scan")) info_printf("\n-%s: %s", option.Key.begin(), "[add -scan instructions here");
		else if(option.Key == LLC_CXS("show")) info_printf("\n-%s: %s", option.Key.begin(), "[add -show instructions here");
		else if(option.Key == LLC_CXS("wipe")) info_printf("\n-%s: %s", option.Key.begin(), "[add -wipe instructions here");
    }
    return 0;
}

sttc	::llc::err_t	dedup_entry_point		(::llc::SRuntimeValues & runtimeValues) {
    SDedupApp       appState    = {};
    appState.CommandLineArgs = runtimeValues.EntryPointArgs;
    if(0 <= llc::argsOptionIndex(appState.CommandLineArgs, "help"))
		return ::displayHelp(appState);

	if(appState.CommandLineArgs.Positionals.size() < 1)
		return ::displayHelp(appState);

	if_fail_ve(EXIT_FAILURE, ::executeDedup(appState));

    return 0;
}
