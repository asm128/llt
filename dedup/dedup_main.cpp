#include "llc_path.h"
#include "llc_file.h"
#include "llc_string_compose.h"
#include "llc_string.h"
#include "llc_timer.h"
#include "llc_minmax.h"


LLC_USING_TYPEINT();
LLC_USING_APOD();
LLC_USING_VIEW();

#pragma pack(push, 1)
struct FileAttributes {
    uint32_t ReadOnly           : 1;  // 0x00000001  Prevents modification through normal file operations
    uint32_t Hidden             : 1;  // 0x00000002  Normally omitted from directory listings
    uint32_t System             : 1;  // 0x00000004  Identifies a file used by the operating system
    uint32_t Reserved0          : 1;  // 0x00000008  Reserved by Windows
    uint32_t Directory          : 1;  // 0x00000010  Entry represents a directory rather than a file
    uint32_t Archive            : 1;  // 0x00000020  Marks the file as changed since last backup
    uint32_t Device             : 1;  // 0x00000040  Reserved; historically used to identify device files
    uint32_t Normal             : 1;  // 0x00000080  No other attributes are set
    uint32_t Temporary          : 1;  // 0x00000100  Indicates temporary data; filesystem may avoid writing it to permanent storage
    uint32_t SparseFile         : 1;  // 0x00000200  File may contain large zero-filled regions that consume little/no disk space
    uint32_t ReparsePoint       : 1;  // 0x00000400  File has filesystem-specific metadata that changes how Windows accesses it
    uint32_t Compressed         : 1;  // 0x00000800  File data is transparently compressed by the filesystem
    uint32_t Offline            : 1;  // 0x00001000  File data is not immediately available locally and may need retrieval
    uint32_t NotContentIndexed  : 1;  // 0x00002000  Windows Search should not index the file's contents
    uint32_t Encrypted          : 1;  // 0x00004000  File data is transparently encrypted by the filesystem
    uint32_t IntegrityStream    : 1;  // 0x00008000  Filesystem maintains integrity information to detect/correct corruption
    uint32_t Virtual            : 1;  // 0x00010000  Reserved for system use
    uint32_t NoScrubData        : 1;  // 0x00020000  Storage integrity scrubber should not validate this file's data
    uint32_t ExtendedAttribute  : 1;  // 0x00040000  File has extended attributes associated with it
    uint32_t Pinned             : 1;  // 0x00080000  Cloud/storage provider should keep the file locally available
    uint32_t Unpinned           : 1;  // 0x00100000  Cloud/storage provider should not guarantee local availability
    uint32_t Reserved1          : 1;  // 0x00200000  Reserved by Windows
    uint32_t RecallOnOpen       : 1;  // 0x00400000  File data may be recalled from remote storage when the file is opened
    uint32_t RecallOnDataAccess : 1;  // 0x00800000  File data may be recalled from remote storage when its contents are accessed
    uint32_t Padding            : 8;  // Remaining unused bits
};
#pragma pack(pop)

// FILE_ATTRIBUTE_READONLY (0x1): The file is read-only. Applications can read the file but cannot write to it or delete it.
// FILE_ATTRIBUTE_HIDDEN (0x2): The file is hidden and not included in an ordinary directory listing.
// FILE_ATTRIBUTE_SYSTEM (0x4): The file is part of, or is used exclusively by, the operating system.
// FILE_ATTRIBUTE_DIRECTORY (0x10): The item is a directory (folder) rather than a file.
// FILE_ATTRIBUTE_ARCHIVE (0x20): The file should be archived. Windows sets this flag whenever a file is created or modified.
// FILE_ATTRIBUTE_NORMAL (0x80): The file has no other attributes set. This flag is only valid if it is used alone.
// FILE_ATTRIBUTE_REPARSE_POINT (0x400): The file or directory has an associated reparse point, or is a symbolic link/junction point.

llc::err_t listFolder   (llc::vcst_t path, bool recursiveWalk, llc::function<llc::err_t(const WIN32_FIND_DATAA&, llc::vcst_c)> callback) {
    WIN32_FIND_DATAA    data    = {};
	llc::rtrim(path, path, "/\\");
	llc::string 		 pathStr;
    if_fail_fe(llc::append_strings(pathStr, path, "/*"));
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

struct SFileInfo {
	llc::string    Path;
	llc::string    Name;
	uint64_t       Size;
	uint64_t       TimestampCreation;
	uint64_t       TimestampLastAccess;
};
    struct SFileInfoPair {
        SFileInfo fileA;
        SFileInfo fileB;
    };

int    compareFileContents(const SFileInfoPair& pair) {
	struct SAutoCloseFile {
		FILE* Handle = nullptr;
		~SAutoCloseFile() { if(Handle) fclose(Handle); }
	};
	stxp u2_c       COMPARISON_CHUNK_SIZE_MAX   = 0x400 * 0x400 * 0x100; 
	u2_c            comparisonChunkSize         = (pair.fileA.Size > COMPARISON_CHUNK_SIZE_MAX) ? COMPARISON_CHUNK_SIZE_MAX : (u2_c)pair.fileA.Size;
	info_printf("\nComparing file contents for: " 
        "\n%s/%s" 
        "\n%s/%s"
        "\nFile  size: %llu bytes."
        "\nChunk size: %llu bytes."
        , pair.fileA.Path.begin(), pair.fileA.Name.begin(), pair.fileB.Path.begin(), pair.fileB.Name.begin()
        , pair.fileA.Size, (uint64_t)comparisonChunkSize);
	static llc::au0_t      bufferA, bufferB;
    llc::resize(comparisonChunkSize, bufferA, bufferB);
    SAutoCloseFile  fileA = {}, fileB = {};
    llc::string     filePathA, filePathB;
    llc::append_strings(filePathA, pair.fileA.Path, "/", pair.fileA.Name);
    llc::append_strings(filePathB, pair.fileB.Path, "/", pair.fileB.Name);
	if_true_fwf(0 != fopen_s(&fileA.Handle, filePathA, "rb"), "Failed to open file: \"%s\"", filePathA.begin());
	if_true_fwf(0 != fopen_s(&fileB.Handle, filePathB, "rb"), "Failed to open file: \"%s\"", filePathB.begin());
    llc::STimer     timer           = {};
	u3_t            remainingBytes  = pair.fileA.Size;
	while(remainingBytes) {
		u3_c            bytesToRead = (remainingBytes > comparisonChunkSize) ? comparisonChunkSize : remainingBytes;
		if_true_fe(bytesToRead != fread(bufferA.begin(), 1, bytesToRead, fileA.Handle));
		if_true_fe(bytesToRead != fread(bufferB.begin(), 1, bytesToRead, fileB.Handle));
		if_true_vi(1, memcmp(bufferA.begin(), bufferB.begin(), bytesToRead)); // Files are different
		remainingBytes -= bytesToRead;
        timer.Frame();
        info_printf("Chunk comparison execution time: %f seconds.", timer.LastTimeMicroseconds * 0.000001);
	}
    return 0; // Files are identical
}

llc::err_t moveFile(const SFileInfo & fileToMove, llc::vcst_c & targetFolder) {
    llc::string         oldPath         = {}
        ,               newPath         = {};
	if_fail_fe(llc::append_strings(oldPath, fileToMove.Path, "/", fileToMove.Name));
    llc::vstr_t         oldPathView     = fileToMove.Path; 
	llc::ltrim(oldPathView, oldPathView, "\\/");
    s2_t                colonOffset     = oldPathView.find(':', 0);
    if(colonOffset >= 0)
		oldPathView[colonOffset] = '_';
	if_fail_fe(llc::append_strings(newPath, targetFolder, '/', oldPathView.cc(), '/'));
	if_fail_wf(llc::pathCreate(newPath, '/'), "Failed to create directory \"%s\"", newPath.begin());
	if_fail_fe(llc::append_strings(newPath, fileToMove.Name));
    info_printf("Moving duplicated file: \"%s\" to \"%s\"", oldPath.begin(), newPath.begin());
	if_zero_fwf(MoveFileExA(oldPath.begin(), newPath.begin(), MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH)
        , "Failed to move duplicated file \"%s\" to \"%s\"", oldPath.begin(), newPath.begin());
    return 0;
}

#if defined(LLC_WINDOWS)
static	::llc::error_t	test_base_log_write	(const char * text, uint32_t /*textLen*/) {	OutputDebugStringA(text); return (::llc::error_t)printf("%s", text); }
#elif defined(LLC_ANDROID)
static	::llc::error_t	test_base_log_write	(const char * text, uint32_t textLen) {	LOGI("%s", text); return (::llc::error_t)textLen; }
#elif defined(LLC_ARDUINO)
static	::llc::error_t	test_base_log_write	(const char * text, uint32_t textLen) {	return ::llc::error_t(Serial ? (::llc::error_t)Serial.write(text, textLen) : (::llc::error_t)textLen); }
#else
static	::llc::error_t	test_base_log_write	(const char * text, uint32_t textLen) {	(void)textLen; return (::llc::error_t)printf_s("%s", text, textLen); }
#endif

#if defined(LLC_WINDOWS)
static	::llc::error_t	test_base_log_print	(const char * text) {	OutputDebugStringA(text); return (::llc::error_t)printf("%s", text); }
#elif defined(LLC_ANDROID)z
static	::llc::error_t	test_base_log_print	(const char * text) {	LOGI("%s", text); return (::llc::error_t)strlen(text); }
#elif defined(LLC_ARDUINO)
static	::llc::error_t	test_base_log_print	(const char * text) {	return Serial ? (::llc::error_t)Serial.print(text) : (::llc::error_t)strlen(text); }
#else
static	::llc::error_t	test_base_log_print	(const char * text) {	return (::llc::error_t)printf("%s", text); }
#endif

llc::err_t collectExactMatches(llc::view<const SFileInfoPair> potentiallyDuplicatedFiles, llc::aobj<SFileInfoPair> & exactMatches) {
    llc::STimer                 timer;
    for(const auto & pair : potentiallyDuplicatedFiles) {
        verbose_printf("Processing potentially duplicated files with size %llu: %s/%s and %s/%s"
            , pair.fileA.Size
            , pair.fileA.Path.begin(), pair.fileA.Name.begin()
            , pair.fileB.Path.begin(), pair.fileB.Name.begin()
        );
        
        llc::err_t                  comparisonResult = 0;
		if_fail_ce(comparisonResult = compareFileContents(pair))
	    else if(comparisonResult)
			verbose_printf("Files are different: %s/%s and %s/%s"
				, pair.fileA.Path.begin(), pair.fileA.Name.begin()
				, pair.fileB.Path.begin(), pair.fileB.Name.begin()
			);
        else {
			info_printf("Files are identical: %s/%s and %s/%s"
				, pair.fileA.Path.begin(), pair.fileA.Name.begin()
				, pair.fileB.Path.begin(), pair.fileB.Name.begin()
			);
			if_fail_fe(exactMatches.push_back(pair));
		}
        timer.Frame();
        info_printf("compareFileContents execution time: %f seconds.", timer.LastTimeMicroseconds * 0.000001);
    }    
    return 0;
}

llc::err_t collectPotentialDuplicates(llc::view<const SFileInfo> largeFiles, llc::aobj<SFileInfoPair> & potentiallyDuplicatedFiles) {
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

stxp llc::vcst_t  DEFAULT_PATH_TO_PROCESS   = LLC_CXS("./");
stxp llc::vcst_t  DEFAULT_TARGET_FOLDER     = LLC_CXS("./Duplicated");

int main(int argc, char * argv[]) {
    static_assert(sizeof(FileAttributes) == 4, "Must be exactly 4 bytes");
	llc::setupLogCallbacks(test_base_log_print, test_base_log_write);

    llc::vcst_t  pathToProcess     = DEFAULT_PATH_TO_PROCESS;
    llc::vcst_t  targetFolder      = DEFAULT_TARGET_FOLDER;

	if(argc < 3) {
		info_printf(
            "\nUsage: %s"
		    "\nThis program searches for large files in the specified path and identifies potentially duplicated files based on their size."
		    "\nIf duplicates are found, it compares their contents and moves one of the duplicates to a target folder."
            , argv[0]
            );
		return 0;
	} 
	else  {
        pathToProcess   = {argv[1], (u2_t)-1};
		targetFolder    = {argv[2], (u2_t)-1};
	}
    stxp llc::minmax<u3_t>  fileSizeRangeInBytes  = {50*1024*1024, (u3_t)-1};
	info_printf(
        "\nPath to process  : \"%s\""
	    "\nTarget folder    : \"%s\""
        "\nComparing files of sizes between %llu and %llu"
        , pathToProcess.begin()
        , targetFolder .begin()
        , fileSizeRangeInBytes.Min
        , fileSizeRangeInBytes.Max
        );

	llc::aobj<SFileInfo>    largeFiles; // list of large files found in the specified path
    if_fail_fe(::listFolder(pathToProcess, true, [&largeFiles](const WIN32_FIND_DATAA & entryData, llc::vcst_t folderPath) { 
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

    info_printf("Total large files found: %u", largeFiles.size());
    llc::aobj<SFileInfoPair>    potentiallyDuplicatedFiles;
	if(largeFiles.size() > 1)
		if_fail_fe(collectPotentialDuplicates(largeFiles, potentiallyDuplicatedFiles));

    info_printf("Total potentially duplicated large files found: %u", potentiallyDuplicatedFiles.size());
    llc::aobj<SFileInfoPair>    exactMatches;
	if_fail_fe(collectExactMatches(potentiallyDuplicatedFiles, exactMatches));

    for(const auto & pair : exactMatches) {
        const b8_t          fileToMoveIsFileB 
            = (pair.fileA.Name.size() < pair.fileB.Name.size())
            //|| ((pair.fileB.Name.Size() == 13) && (0 == memcmp(pair.fileB.Name.begin(), "177", 3))) 
            ;
	    const SFileInfo     & fileToMove    = fileToMoveIsFileB ? pair.fileB : pair.fileA;
	    if_fail_wf(moveFile(fileToMove, targetFolder), "Failed to move file \"%s\".", fileToMove.Name.begin());
    }
    return 0;
}

