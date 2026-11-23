#include "test_blitter_suites.h"

#include "blitter.h"

GDEFINE_ENUM_TYPE(TEST_BLITTER_RESULT, ::llc::u0_t);
GDEFINE_ENUM_VALUED(TEST_BLITTER_RESULT, TABLE_NAME_RESULT	, 0, "tableFileName() rejected valid input.");
GDEFINE_ENUM_VALUED(TEST_BLITTER_RESULT, TABLE_NAME_VALUE	, 1, "tableFileName() selected the wrong extension.");
GDEFINE_ENUM_VALUED(TEST_BLITTER_RESULT, BLOCK_NAME_RESULT	, 2, "blockFileName() rejected valid input.");
GDEFINE_ENUM_VALUED(TEST_BLITTER_RESULT, BLOCK_NAME_VALUE	, 3, "blockFileName() selected the wrong extension.");
GDEFINE_ENUM_VALUED(TEST_BLITTER_RESULT, FOLDER_NAME_RESULT	, 4, "tableFolderName() rejected valid input.");
GDEFINE_ENUM_VALUED(TEST_BLITTER_RESULT, FOLDER_NAME_VALUE	, 5, "tableFolderName() produced the wrong folder name.");
GDEFINE_ENUM_VALUED(TEST_BLITTER_RESULT, FOLDER_INIT_RESULT	, 6, "tableFolderInit() rejected a writable test folder.");
GDEFINE_ENUM_VALUED(TEST_BLITTER_RESULT, FOLDER_INIT_VALUE	, 7, "tableFolderInit() produced the wrong database path.");

::llc::err_t test_blitter(::llc::ATestError & errors, ::llc::vcst_t outputFolder) {
	struct SNameCase {
		::blt::DATABASE_HOST Host;
		bool Encrypted;
		::llc::vcst_t Extension;
	};
	stxp SNameCase cases[] =
		{ {::blt::DATABASE_HOST_LOCAL	, false, "sson"}
		, {::blt::DATABASE_HOST_DEFLATE	, false, "zson"}
		, {::blt::DATABASE_HOST_LOCAL	, true , "cson"}
		, {::blt::DATABASE_HOST_DEFLATE	, true , "czon"}
		};

	for(cnst SNameCase & testCase : cases) {
		::llc::string expectedTable = ::llc::vcst_t{"catalog."};
		if_fail_fe(expectedTable.append(testCase.Extension));
		::llc::string tableName;
		cnst ::llc::err_t tableResult = ::blt::tableFileName(tableName, testCase.Host, testCase.Encrypted, "catalog");
		LLC_TEST_CHECKF(errors, TEST_BLITTER_RESULT_TABLE_NAME_RESULT, ::llc::failed(tableResult), "Host:%u, encrypted:%u, result:%i.", testCase.Host, testCase.Encrypted, tableResult);
		LLC_TEST_CHECKF(errors, TEST_BLITTER_RESULT_TABLE_NAME_VALUE, tableName != expectedTable, "Actual:'%.*s', expected:'%.*s'.", LLC_VIEW_ARGS(tableName), LLC_VIEW_ARGS(expectedTable));

		::llc::string expectedBlock = ::llc::vcst_t{"catalog.7."};
		if_fail_fe(expectedBlock.append(testCase.Extension));
		::llc::string blockName;
		cnst ::llc::err_t blockResult = ::blt::blockFileName(blockName, "catalog", testCase.Encrypted, testCase.Host, 7);
		LLC_TEST_CHECKF(errors, TEST_BLITTER_RESULT_BLOCK_NAME_RESULT, ::llc::failed(blockResult), "Host:%u, encrypted:%u, result:%i.", testCase.Host, testCase.Encrypted, blockResult);
		LLC_TEST_CHECKF(errors, TEST_BLITTER_RESULT_BLOCK_NAME_VALUE, blockName != expectedBlock, "Actual:'%.*s', expected:'%.*s'.", LLC_VIEW_ARGS(blockName), LLC_VIEW_ARGS(expectedBlock));
	}

	::llc::string folderName;
	cnst ::llc::err_t folderResult = ::blt::tableFolderName(folderName, "catalog", 64);
	LLC_TEST_CHECKF(errors, TEST_BLITTER_RESULT_FOLDER_NAME_RESULT, ::llc::failed(folderResult), "Result:%i.", folderResult);
	LLC_TEST_CHECKF(errors, TEST_BLITTER_RESULT_FOLDER_NAME_VALUE, folderName != ::llc::vcst_t{"catalog.64.db"}, "Actual:'%.*s'.", LLC_VIEW_ARGS(folderName));

	::llc::string expectedFolder = outputFolder;
	if(expectedFolder.size() && '/' != expectedFolder[expectedFolder.size() - 1] && '\\' != expectedFolder[expectedFolder.size() - 1])
		if_fail_fe(expectedFolder.push_back('/'));
	if_fail_fe(expectedFolder.append(::llc::vcst_t{"catalog.64.db/"}));
	::llc::string initializedFolder;
	cnst ::llc::err_t initResult = ::blt::tableFolderInit(initializedFolder, outputFolder, "catalog", 64);
	LLC_TEST_CHECKF(errors, TEST_BLITTER_RESULT_FOLDER_INIT_RESULT, ::llc::failed(initResult), "Result:%i.", initResult);
	LLC_TEST_CHECKF(errors, TEST_BLITTER_RESULT_FOLDER_INIT_VALUE, initializedFolder != expectedFolder, "Actual:'%.*s', expected:'%.*s'.", LLC_VIEW_ARGS(initializedFolder), LLC_VIEW_ARGS(expectedFolder));
	rtrn 0;
}
