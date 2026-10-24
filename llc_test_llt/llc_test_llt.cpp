#include "llc_test_core.h"

::llc::err_t testNN(::llc::ATestError & results, ::llc::vcst_t outputFolder);
::llc::err_t testNNHeatmap(::llc::ATestError & results, ::llc::vcst_t outputFolder);
::llc::err_t testRefactorAtlas(::llc::ATestError & results, ::llc::vcst_t outputFolder);

sttc ::llc::err_t test_llt_entry_point(::llc::SRuntimeValues & runtimeValues) {
	rtrn ::llc::runSuites(runtimeValues, LLC_TEST_SUITE(testNN, "nn"), LLC_TEST_SUITE(testNNHeatmap, "nn_heatmap"), LLC_TEST_SUITE(testRefactorAtlas, "refactor_atlas"));
}
LLC_OS_ENTRY_POINT(::test_llt_entry_point);
