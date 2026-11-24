#include "test_blitter_suites.h"

#include "blitter.h"
#include "llc_file.h"
#include "llc_json.h"

GDEFINE_ENUM_TYPE(TEST_REASON_RESULT, ::llc::u0_t);
GDEFINE_ENUM_VALUED(TEST_REASON_RESULT, INPUT_WRITE	, 0, "The reason input fixture could not be written.");
GDEFINE_ENUM_VALUED(TEST_REASON_RESULT, PROCESS_LAUNCH	, 1, "The reason frontend could not be launched.");
GDEFINE_ENUM_VALUED(TEST_REASON_RESULT, PROCESS_EXIT	, 2, "The reason frontend rejected a valid JSON-array fixture.");
GDEFINE_ENUM_VALUED(TEST_REASON_RESULT, BLITTER_LOAD	, 3, "Blitter could not load the table written by reason.");
GDEFINE_ENUM_VALUED(TEST_REASON_RESULT, BLOCK_AVAILABLE	, 4, "Blitter did not retain the loaded reason table block.");
GDEFINE_ENUM_VALUED(TEST_REASON_RESULT, JSON_WRITE	, 5, "The JSON loaded by Blitter could not be serialized.");
GDEFINE_ENUM_VALUED(TEST_REASON_RESULT, JSON_CONTENT	, 6, "The JSON loaded by Blitter differs from the reason input.");

::llc::err_t test_reason(::llc::ATestError & errors, ::llc::vcst_t outputFolder) {
	stxp ::llc::vcst_t inputJSON = "[{\"id\":1},{\"id\":2},{\"id\":3}]";
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
		{ {"plain"              , "reason_plain"    , "reason_plain.json"    , "reason_plain.json 0"                         , ::blt::DATABASE_HOST_LOCAL  , {}}
		, {"deflated"           , "reason_deflated" , "reason_deflated.json" , "reason_deflated.json 1"                      , ::blt::DATABASE_HOST_DEFLATE, {}}
		, {"encrypted"          , "reason_encrypted", "reason_encrypted.json", "reason_encrypted.json 0 0123456789ABCDEF0123456789ABCDEF" , ::blt::DATABASE_HOST_LOCAL  , encryptionKey}
		, {"encrypted+deflated" , "reason_secure"   , "reason_secure.json"   , "reason_secure.json 1 0123456789ABCDEF0123456789ABCDEF"    , ::blt::DATABASE_HOST_DEFLATE, encryptionKey}
		};

	::llc::SJSONReader sourceReader;
	if_fail_fe(::llc::jsonParse(sourceReader, inputJSON));
	::llc::string expectedJSON;
	if_fail_fe(::llc::jsonWrite(sourceReader.Tree[0], sourceReader.View, expectedJSON));

	for(cnst SModeCase & mode : modes) {
		cnst ::llc::err_t writeResult = ::llc::fileFromMemory(outputFolder, mode.InputFile, inputJSON.cu8());
		LLC_TEST_CHECKF(errors, TEST_REASON_RESULT_INPUT_WRITE, ::llc::failed(writeResult), "Mode:%.*s, result:%i.", LLC_VIEW_ARGS(mode.Name), writeResult);
		if(::llc::failed(writeResult))
			continue;

		int exitCode = -1;
		cnst ::llc::err_t launchResult = ::testBlitterRunFrontend("reason", outputFolder, mode.Arguments, exitCode);
		LLC_TEST_CHECKF(errors, TEST_REASON_RESULT_PROCESS_LAUNCH, ::llc::failed(launchResult), "Mode:%.*s, result:%i.", LLC_VIEW_ARGS(mode.Name), launchResult);
		if(::llc::failed(launchResult))
			continue;
		LLC_TEST_CHECKF(errors, TEST_REASON_RESULT_PROCESS_EXIT, 0 != exitCode, "Mode:%.*s, exit code:%i.", LLC_VIEW_ARGS(mode.Name), exitCode);
		if(0 != exitCode)
			continue;

		::llc::SLoadCache loadCache;
		::blt::TNamedBlitterDB database = {};
		database.Key = mode.Database;
		database.Val.HostType = mode.Host;
		database.Val.EncryptionKey = mode.Key;
		cnst ::llc::err_t loadResult = ::blt::tableFileLoad(loadCache, database, outputFolder);
		LLC_TEST_CHECKF(errors, TEST_REASON_RESULT_BLITTER_LOAD, ::llc::failed(loadResult), "Mode:%.*s, result:%i.", LLC_VIEW_ARGS(mode.Name), loadResult);
		if(::llc::failed(loadResult))
			continue;

		LLC_TEST_CHECKF(errors, TEST_REASON_RESULT_BLOCK_AVAILABLE, 0 == database.Val.Blocks.size() || 0 == database.Val.Blocks[0], "Mode:%.*s, blocks:%u.", LLC_VIEW_ARGS(mode.Name), database.Val.Blocks.size());
		if(0 == database.Val.Blocks.size() || 0 == database.Val.Blocks[0])
			continue;

		::llc::string recoveredJSON;
		cnst ::llc::SJSONReader & recoveredReader = database.Val.Blocks[0]->Reader;
		cnst ::llc::err_t jsonResult = ::llc::jsonWrite(recoveredReader.Tree[0], recoveredReader.View, recoveredJSON);
		LLC_TEST_CHECKF(errors, TEST_REASON_RESULT_JSON_WRITE, ::llc::failed(jsonResult), "Mode:%.*s, result:%i.", LLC_VIEW_ARGS(mode.Name), jsonResult);
		if(::llc::failed(jsonResult))
			continue;
		LLC_TEST_CHECKF(errors, TEST_REASON_RESULT_JSON_CONTENT, recoveredJSON != expectedJSON, "Mode:%.*s, recovered:'%.*s', expected:'%.*s'.", LLC_VIEW_ARGS(mode.Name), LLC_VIEW_ARGS(recoveredJSON), LLC_VIEW_ARGS(expectedJSON));
	}
	rtrn 0;
}
