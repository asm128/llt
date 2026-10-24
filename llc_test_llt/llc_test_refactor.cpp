#include "llc_refactor.h"
#include "llc_test_core.h"

GDEFINE_ENUM_TYPE(REFACTOR_TEST_RESULT, ::llc::u0_t);
GDEFINE_ENUM_VALUED(REFACTOR_TEST_RESULT, DIFF, 0, "Unified diff lost a before/after hunk or its provenance coordinates.");
GDEFINE_ENUM_VALUED(REFACTOR_TEST_RESULT, NORMALIZE, 1, "Code normalization retained layout, comments or literal values.");
GDEFINE_ENUM_VALUED(REFACTOR_TEST_RESULT, SIMILARITY, 2, "Refactor similarity did not rank equivalent token structure correctly.");
GDEFINE_ENUM_VALUED(REFACTOR_TEST_RESULT, RETRIEVAL, 3, "Historical before-code retrieval did not preserve the matching after hunk and provenance.");
GDEFINE_ENUM_VALUED(REFACTOR_TEST_RESULT, STRING, 4, "String literal assignment counted bytes after the first null terminator.");

::llc::err_t testRefactorAtlas(::llc::ATestError & results, ::llc::vcst_t) {
	cnst ::llc::vcst_t patch =
		"diff --git a/llc/llc_ref.h b/llc/llc_ref.h\n"
		"--- a/llc/llc_ref.h\n"
		"+++ b/llc/llc_ref.h\n"
		"@@ -6,4 +6,4 @@ namespace llc\n"
		" {\n"
		"- T * Instance = 0;\n"
		"+ T * Instance = {};\n"
		" }\n"
		"@@ -42 +42 @@ namespace llc\n"
		"- if_true_ve(0, 0 == ref_allocate(&newRef));\n"
		"+ if_null_ve(0, ref_allocate(&newRef));\n";
	::llc::SRefactorPatch parsed;
	if_fail_fe(::llc::refactorDiffParse(patch, parsed));
	LLC_TEST_REQUIRE(results, REFACTOR_TEST_RESULT_DIFF, parsed.Hunks.size() != 2);
	LLC_TEST_REQUIRE(results, REFACTOR_TEST_RESULT_DIFF, parsed.Hunks[0].FileBefore != "llc/llc_ref.h" || parsed.Hunks[0].FileAfter != "llc/llc_ref.h");
	LLC_TEST_REQUIRE(results, REFACTOR_TEST_RESULT_DIFF, parsed.Hunks[0].LineBefore != 6 || parsed.Hunks[0].CountBefore != 4 || parsed.Hunks[0].LineAfter != 6 || parsed.Hunks[0].CountAfter != 4);
	LLC_TEST_REQUIRE(results, REFACTOR_TEST_RESULT_DIFF, parsed.Hunks[1].LineBefore != 42 || parsed.Hunks[1].CountBefore != 1 || parsed.Hunks[1].LineAfter != 42 || parsed.Hunks[1].CountAfter != 1);
	LLC_TEST_REQUIRE(results, REFACTOR_TEST_RESULT_DIFF, parsed.Hunks[0].Before == parsed.Hunks[0].After);

	::llc::string first = {}, second = {};
	if_fail_fe(::llc::refactorCodeNormalize("int value = 42; // old value\nreturn value;", first));
	if_fail_fe(::llc::refactorCodeNormalize(" int  value=7; /* replacement */ return value ;", second));
	LLC_TEST_REQUIRE(results, REFACTOR_TEST_RESULT_NORMALIZE, first != second);

	::llc::f3_t same = 0, different = 0;
	if_fail_fe(::llc::refactorCodeSimilarity("if(0 == pointer) return -1;", "if (0==pointer) return -2;", same));
	if_fail_fe(::llc::refactorCodeSimilarity("if(0 == pointer) return -1;", "grid.resize(width, height);", different));
	LLC_TEST_REQUIRE(results, REFACTOR_TEST_RESULT_SIMILARITY, same != 1 || different >= same);

	::llc::string assigned;
	cnst ::llc::sc_t embedded[] = {'a', 't', 'l', 'a', 's', 0, 'x', 0};
	assigned = embedded;
	LLC_TEST_REQUIRE(results, REFACTOR_TEST_RESULT_STRING, assigned.size() != 5);
	LLC_TEST_REQUIRE(results, REFACTOR_TEST_RESULT_STRING, assigned != "atlas");
	LLC_TEST_REQUIRE(results, REFACTOR_TEST_RESULT_STRING, assigned.begin()[assigned.size()] != 0);

	parsed.Source = {
		"llc",
		"5183dcb1c84eae6a4a53c5b3ae01a025c45175b2",
		"2026-10-23T13:18:13-03:00",
		"refactor(ref): align initialization and allocation checks with LLC idioms",
	};
	::llc::SRefactorMatch match;
	if_fail_fe(::llc::refactorFindBest("if_true_ve(0, 0 == ref_allocate(&reference));", {&parsed, 1}, match));
	LLC_TEST_REQUIRE(results, REFACTOR_TEST_RESULT_RETRIEVAL, match.Patch || match.Hunk != 1 || match.Score <= 0.5);
	LLC_TEST_REQUIRE(results, REFACTOR_TEST_RESULT_RETRIEVAL, parsed.Source.Commit != "5183dcb1c84eae6a4a53c5b3ae01a025c45175b2");
	LLC_TEST_REQUIRE(results, REFACTOR_TEST_RESULT_RETRIEVAL, ::llc::find_string("if_null_ve", parsed.Hunks[match.Hunk].After) < 0);
	rtrn 0;
}
