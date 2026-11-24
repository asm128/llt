#include "test_blitter_suites.h"

#include "llc_path.h"

#include <cstdlib>

sttc ::llc::string g_ExecutableFolder;

::llc::err_t testBlitterRunFrontend(::llc::vcst_t executableName, ::llc::vcst_t outputFolder, ::llc::vcst_t arguments, int & exitCode) {
	::llc::string executablePath = g_ExecutableFolder;
	if_fail_fe(executablePath.append(executableName));
#if defined(LLC_WINDOWS)
	if_fail_fe(executablePath.append(::llc::vcst_t{".exe"}));
#endif

	::llc::string command = ::llc::vcst_t{"\""};
	if_fail_fe(command.append(executablePath));
	if_fail_fe(command.append(::llc::vcst_t{"\" "}));
	if_fail_fe(command.append(arguments));
	if_fail_fe(command.push_back(0));

#if defined(LLC_WINDOWS)
	::llc::as0_t previousFolder;
	if_fail_fe(previousFolder.resize(32768));
	cnst ::DWORD previousLength = ::GetCurrentDirectoryA(previousFolder.size(), previousFolder.begin());
	if_true_fe(0 == previousLength || previousLength >= previousFolder.size());
	if_fail_fe(previousFolder.resize(previousLength));
	if_fail_fe(previousFolder.push_back(0));

	::llc::string suiteFolder = outputFolder;
	if_fail_fe(suiteFolder.push_back(0));
	if_zero_fe(::SetCurrentDirectoryA(suiteFolder.begin()));
	exitCode = ::system(command.begin());
	if_zero_fe(::SetCurrentDirectoryA(previousFolder.begin()));
#else
	(void)outputFolder;
	exitCode = ::system(command.begin());
#endif
	rtrn 0;
}

sttc ::llc::err_t test_blitter_entry_point(::llc::SRuntimeValues & runtimeValues);
LLC_OS_ENTRY_POINT(::test_blitter_entry_point);

sttc ::llc::err_t test_blitter_entry_point(::llc::SRuntimeValues & runtimeValues) {
	::llc::string executablePath;
	if_fail_fe(::llc::pathAbsolute(runtimeValues.EntryPointArgs.ProgramName, executablePath));
	::llc::vcst_t executableFolder;
	if_fail_fe(::llc::pathDirectory(executablePath, executableFolder));
	g_ExecutableFolder = executableFolder;

	rtrn ::llc::runSuites(runtimeValues
		, LLC_TEST_SUITE(reason)
		, LLC_TEST_SUITE(season)
		, LLC_TEST_SUITE(blitter)
		);
}
