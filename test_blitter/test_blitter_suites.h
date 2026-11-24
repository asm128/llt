#include "llc_test_core.h"

#ifndef TEST_BLITTER_SUITES_H
#define TEST_BLITTER_SUITES_H

usns llc;

::llc::err_t test_reason	(::llc::ATestError & errors, ::llc::vcst_t outputFolder);
::llc::err_t test_season	(::llc::ATestError & errors, ::llc::vcst_t outputFolder);
::llc::err_t test_blitter	(::llc::ATestError & errors, ::llc::vcst_t outputFolder);

::llc::err_t testBlitterRunFrontend(::llc::vcst_t executableName, ::llc::vcst_t outputFolder, ::llc::vcst_t arguments, int & exitCode);

#endif // TEST_BLITTER_SUITES_H
