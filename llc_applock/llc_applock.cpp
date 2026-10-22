#include "llc_runtime.h"
#include "llc_string_compose.h"

LLC_USING_TYPEINT();

stxp llc::vcst_c	ROOT			= "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\";
stxp llc::vcst_c	BLOCKED[]		= { "node.exe", "nodejs.exe", "python.exe", "python3.exe", "pythonw.exe", "python3w.exe", "py.exe", "pyw.exe", "pip.exe", "pip3.exe"};

sttc llc::err_t		uninstallKeys	(llc::vcst_t keyPath, llc::vcst_t debugger) {
	HKEY					key				= {};
    LSTATUS					result;
	if_true_fef((result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyPath, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &key)) && result != ERROR_FILE_NOT_FOUND, "0x%X", result);
	if_true_vif(1, ERROR_FILE_NOT_FOUND == result, "keyPath:\"%s\" dbugger:\"%s\" doesn't exist", keyPath.begin(), debugger.begin());

	llc::astsc_t<4096>		existing		= {};
	DWORD					bytes			= existing.size();
	result				= RegGetValue(key, 0, "Debugger", RRF_RT_REG_SZ, 0, &existing[0], &bytes);
	if(ERROR_SUCCESS == result && debugger == llc::vcst_t{&existing[0], (u2_t)-1})
		result				= RegDeleteValue(key, "Debugger");
	RegCloseKey(key);
	if_true_fef(result && result != ERROR_FILE_NOT_FOUND, "0x%X", result);
	return 0;
}
sttc llc::err_t		installKeys		(llc::vcst_t keyPath, llc::vcst_t debugger) {
    HKEY					key				= {};
	llc::astsc_t<4096>		existing		= {};
	DWORD					bytes			= existing.size();
    LSTATUS					result;
	if_true_fef(result = RegCreateKeyEx(HKEY_LOCAL_MACHINE, &keyPath[0], 0, 0, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, 0, &key, 0), "0x%X-0x%X", result, GetLastError());
    result				= RegGetValue(key, 0, "Debugger", RRF_RT_REG_SZ, 0, &existing[0], &bytes);
	if(ERROR_SUCCESS == result && debugger != llc::vcst_t{&existing[0], (u2_t)-1}) {
		RegCloseKey(key);
		error_printf("Refusing to replace the existing IFEO Debugger for %s.", keyPath.begin());
		return ERROR_ALREADY_EXISTS;
	}
	if_true_fef(result && result != ERROR_FILE_NOT_FOUND, "0x%X", result);
	result				= RegSetValueEx(key, "Debugger", 0, REG_SZ, debugger.cu8().begin(), debugger.size() + 1);
	RegCloseKey(key);
	if_true_fef(result, "RegSetValueEx() --> 0x%X", result);
	return 0;
}
sttc llc::err_t configure(bool installLocker) {
	always_printf("%s Gatewatch's App Locker", installLocker ? "installing" : "uninstalling");

	llc::astsc_t<4096>	executable = {};
    if_zero_fef(FALSE == GetModuleFileName(nullptr, &executable[0], executable.size() - 1), "%i", GetLastError());

    llc::string			keyPath, debugger;
    for (const auto name : BLOCKED) {
		keyPath = debugger = {};
		if_fail_fe(llc::append_strings(keyPath, ROOT, name));
		if_fail_fe(llc::append_strings(debugger, '"', executable, "\" --blocked"));

        if_fail_fe(installLocker ? installKeys(keyPath, debugger) : uninstallKeys(keyPath, debugger)); 
    }
    return 0;
}
//
int WINAPI app_blocker_entry_point(llc::SRuntimeValues & runtimeValues) {
	if_true_fef(-1 != llc::argsOptionIndex(runtimeValues.EntryPointArgs, "blocked"), "%s", "This runtime is prohibited by the computer's security policy.");

	if(-1 != llc::argsOptionIndex(runtimeValues.EntryPointArgs, "install"))
        return configure(true);

	if(-1 != llc::argsOptionIndex(runtimeValues.EntryPointArgs, "uninstall"))
        return configure(false);

    error_printf("-- Gatewatch Runtime Guard -- Run as administrator with --install or --uninstall.");
    return -1;
}

LLC_OS_ENTRY_POINT(app_blocker_entry_point);



git clone https://github.com/asm128/gpk
git clone https://github.com/asm128/gpk_data
git clone https://github.com/asm128/gpk_games
git clone https://github.com/asm128/zlib
