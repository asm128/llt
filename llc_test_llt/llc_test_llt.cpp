#include "llc_test_core.h"

::llc::err_t test_nn(::llc::ATestError & results, ::llc::vcst_t outputFolder);
::llc::err_t test_nn_heatmap(::llc::ATestError & results, ::llc::vcst_t outputFolder);
::llc::err_t test_refactor_atlas(::llc::ATestError & results, ::llc::vcst_t outputFolder);

sttc ::llc::err_t test_llt_entry_point(::llc::SRuntimeValues & runtimeValues) {
	rtrn ::llc::runSuites(runtimeValues, LLC_TEST_SUITE(nn), LLC_TEST_SUITE(nn_heatmap), LLC_TEST_SUITE(refactor_atlas));
}
LLC_OS_ENTRY_POINT(::test_llt_entry_point);
