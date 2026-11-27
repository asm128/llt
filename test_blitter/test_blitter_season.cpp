#include "test_blitter_suites.h"

#include "blitter.h"
#include "llc_file.h"
#include "llc_json.h"

GDEFINE_ENUM_TYPE(TEST_SEASON_RESULT, ::llc::u0_t);
GDEFINE_ENUM_VALUED(TEST_SEASON_RESULT, INPUT_WRITE	, 0, "The season input fixture could not be written.");
GDEFINE_ENUM_VALUED(TEST_SEASON_RESULT, PROCESS_LAUNCH	, 1, "The season frontend could not be launched.");
GDEFINE_ENUM_VALUED(TEST_SEASON_RESULT, PROCESS_EXIT	, 2, "The season frontend rejected a valid JSON-array fixture.");
GDEFINE_ENUM_VALUED(TEST_SEASON_RESULT, BLITTER_LOAD	, 3, "Blitter could not load a block written by season.");
GDEFINE_ENUM_VALUED(TEST_SEASON_RESULT, BLOCK_AVAILABLE	, 4, "Blitter did not retain a loaded season block.");
GDEFINE_ENUM_VALUED(TEST_SEASON_RESULT, JSON_WRITE	, 5, "A season block loaded by Blitter could not be serialized.");
GDEFINE_ENUM_VALUED(TEST_SEASON_RESULT, JSON_CONTENT	, 6, "A season block loaded by Blitter differs from the expected source records.");

::llc::err_t test_season(::llc::ATestError & errors, ::llc::vcst_t outputFolder) {
	stxp ::llc::vcst_t inputJSON = "[{\"id\":1},{\"id\":2},{\"id\":3},{\"id\":4},{\"id\":5}]";
	stxp ::llc::vcst_t encryptionKey = "0123456789ABCDEF0123456789ABCDEF";
	struct SModeCase {
		::llc::vcst_t Name;
		::llc::vcst_t Database;
		::llc::vcst_t InputFile;
		::llc::vcst_t Arguments;
		::blt::DATABASE_HOST Host;
		::llc::vcst_t Key;
	};
	stxp SModeCase modes[] =
		{ {"plain"              , "season_plain"    , "season_plain.json"    , "season_plain.json 2 0"                      , ::blt::DATABASE_HOST_LOCAL  , {}}
		, {"deflated"           , "season_deflated" , "season_deflated.json" , "season_deflated.json 2 1"                   , ::blt::DATABASE_HOST_DEFLATE, {}}
		, {"encrypted"          , "season_encrypted", "season_encrypted.json", "season_encrypted.json 2 0 0123456789ABCDEF0123456789ABCDEF" , ::blt::DATABASE_HOST_LOCAL  , encryptionKey}
		, {"encrypted+deflated" , "season_secure"   , "season_secure.json"   , "season_secure.json 2 1 0123456789ABCDEF0123456789ABCDEF"    , ::blt::DATABASE_HOST_DEFLATE, encryptionKey}
		};

	::llc::SJSONReader sourceReader;
	if_fail_fe(::llc::jsonParse(sourceReader, inputJSON));
	::llc::aobj<::llc::string> expectedBlocks;
	if_fail_fe(::llc::jsonArraySplit(*sourceReader.Tree[0], sourceReader.View, 2, expectedBlocks));

	for(cnst SModeCase & mode : modes) {
		cnst ::llc::err_t writeResult = ::llc::fileFromMemory(outputFolder, mode.InputFile, inputJSON.as<u0_c>());
		LLC_TEST_CHECKF(errors, TEST_SEASON_RESULT_INPUT_WRITE, ::llc::failed(writeResult), "Mode:%.*s, result:%i.", LLC_VIEW_ARGS(mode.Name), writeResult);
		if(::llc::failed(writeResult))
			continue;

		int exitCode = -1;
		cnst ::llc::err_t launchResult = ::testBlitterRunFrontend("season", outputFolder, mode.Arguments, exitCode);
		LLC_TEST_CHECKF(errors, TEST_SEASON_RESULT_PROCESS_LAUNCH, ::llc::failed(launchResult), "Mode:%.*s, result:%i.", LLC_VIEW_ARGS(mode.Name), launchResult);
		if(::llc::failed(launchResult))
			continue;
		LLC_TEST_CHECKF(errors, TEST_SEASON_RESULT_PROCESS_EXIT, 0 != exitCode, "Mode:%.*s, exit code:%i.", LLC_VIEW_ARGS(mode.Name), exitCode);
		if(0 != exitCode)
			continue;

		::llc::SLoadCache loadCache;
		::blt::TNamedBlitterDB database = {};
		database.Key = mode.Database;
		database.Val.BlockSize = 2;
		database.Val.HostType = mode.Host;
		database.Val.EncryptionKey = mode.Key;
		for(::llc::u2_t iBlock = 0; iBlock < expectedBlocks.size(); ++iBlock)
			if_fail_fe(database.Val.BlocksOnDisk.push_back(iBlock));
		database.Val.MaxBlockOnDisk = expectedBlocks.size() - 1;

		for(::llc::u2_t iBlock = 0; iBlock < expectedBlocks.size(); ++iBlock) {
			cnst ::llc::err_t loadResult = ::blt::blockFileLoad(loadCache, database, outputFolder, iBlock);
			LLC_TEST_CHECKF(errors, TEST_SEASON_RESULT_BLITTER_LOAD, ::llc::failed(loadResult), "Mode:%.*s, block:%u, result:%i.", LLC_VIEW_ARGS(mode.Name), iBlock, loadResult);
			if(::llc::failed(loadResult))
				continue;

			cnst ::llc::u2_t loadedIndex = (::llc::u2_t)loadResult;
			LLC_TEST_CHECKF(errors, TEST_SEASON_RESULT_BLOCK_AVAILABLE, loadedIndex >= database.Val.Blocks.size() || 0 == database.Val.Blocks[loadedIndex], "Mode:%.*s, block:%u, loaded index:%u, blocks:%u.", LLC_VIEW_ARGS(mode.Name), iBlock, loadedIndex, database.Val.Blocks.size());
			if(loadedIndex >= database.Val.Blocks.size() || 0 == database.Val.Blocks[loadedIndex])
				continue;

			::llc::string recoveredJSON;
			cnst ::llc::SJSONReader & recoveredReader = database.Val.Blocks[loadedIndex]->Reader;
			cnst ::llc::err_t jsonResult = ::llc::jsonWrite(recoveredReader.Tree[0], recoveredReader.View, recoveredJSON);
			LLC_TEST_CHECKF(errors, TEST_SEASON_RESULT_JSON_WRITE, ::llc::failed(jsonResult), "Mode:%.*s, block:%u, result:%i.", LLC_VIEW_ARGS(mode.Name), iBlock, jsonResult);
			if(::llc::failed(jsonResult))
				continue;
			LLC_TEST_CHECKF(errors, TEST_SEASON_RESULT_JSON_CONTENT, recoveredJSON != expectedBlocks[iBlock], "Mode:%.*s, block:%u, recovered:'%.*s', expected:'%.*s'.", LLC_VIEW_ARGS(mode.Name), iBlock, LLC_VIEW_ARGS(recoveredJSON), LLC_VIEW_ARGS(expectedBlocks[iBlock]));
		}
	}
	rtrn 0;
}
