#include "llc_runtime.h"
#include "llc_string_compose.h"

#ifndef LCP_IDENTITY_H
#define LCP_IDENTITY_H
stin llc::err_t lcpIdentity(llc::string & output) {
#ifdef _DEBUG
	llc::vcst_t configuration = "Debug";
#else
	llc::vcst_t configuration = "Release";
#endif
#ifdef LLC_AVOID_LOCAL_APPLICATION_MODEL_EXECUTABLE_RUNTIME
	llc::vcst_t mode = "-MT (LRT module)";
#else
	llc::vcst_t mode = " (standalone)";
#endif
	if_fail_fe(llc::append_strings(output, "Configuration: ", configuration, mode, "\nArchitecture: ", llc::str((llc::u2_t)sizeof(void*) * 8), "-bit\nBuilt: ", __DATE__, " ", __TIME__, "\n"));
#ifdef LLC_WINDOWS
	char path[32768] = {};
	DWORD count = GetModuleFileNameA(nullptr, path, (DWORD)llc::size(path));
	if_true_fe(!count || count >= llc::size(path));
	if_fail_fe(llc::append_strings(output, "PID: ", llc::str((llc::u2_t)GetCurrentProcessId()), "\nHost EXE: ", llc::vcst_t{path, count}, '\n'));
	HMODULE module = {};
	if_zero_fe(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&lcpIdentity, &module));
	count = GetModuleFileNameA(module, path, (DWORD)llc::size(path));
	if_true_fe(!count || count >= llc::size(path));
	if_fail_fe(llc::append_strings(output, "Code module: ", llc::vcst_t{path, count}, '\n'));
	count = GetCurrentDirectoryA((DWORD)llc::size(path), path);
	if_true_fe(!count || count >= llc::size(path));
	if_fail_fe(llc::append_strings(output, "Working directory: ", llc::vcst_t{path, count}, "\nCommand line: ", llc::vcst_t{GetCommandLineA(), (llc::u2_t)-1}, '\n'));
#endif
	rtrn 0;
}
#endif
