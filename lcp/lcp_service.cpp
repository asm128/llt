#include "lcp_service.h"
#include "lcp_text.h"
#include "../lls/lls_control.h"
#include "llc_json_expression.h"

#ifdef LLC_WINDOWS
#pragma comment(lib, "user32.lib")

namespace {
using namespace llc;

stct SHandle {
	HANDLE Handle = 0;
	SHandle() = default;
	SHandle(cnst SHandle &) = delete;
	SHandle & oper=(cnst SHandle &) = delete;
	~SHandle() { if(Handle) CloseHandle(Handle); }
};
stct SOperation : SHandle {
	bool Owned = false;
	~SOperation() { if(Owned) ReleaseMutex(Handle); }
};
stct SWindow {
	cnst char * Property;
	HWND Window = 0;
};
BOOL CALLBACK findWindow(HWND window, LPARAM parameter) {
	SWindow & found = *(SWindow*)parameter;
	if(GetPropA(window, found.Property) != (HANDLE)1) rtrn TRUE;
	found.Window = window;
	rtrn FALSE;
}
err_t locate(cnst lls::SControlNames & names, HWND & window, DWORD & pid) {
	SWindow found{names.Window};
	EnumWindows(findWindow, (LPARAM)&found);
	window = found.Window;
	pid = 0;
	if(window) {
		if_zero_ve(OS_ERROR, GetWindowThreadProcessId(window, &pid));
		rtrn 0;
	}
	SHandle instance;
	instance.Handle = OpenMutexA(SYNCHRONIZE, FALSE, names.Instance);
	if(instance.Handle) rtrn OS_BUSY; // Starting, older binary, or another desktop/session.
	rtrn GetLastError() == ERROR_FILE_NOT_FOUND ? 0 : OS_ERROR;
}
err_t executable(vcst_t executablePath, string & text, string & path, string & directory, string & command) {
	if_fail_fe(path.append(executablePath));
	for(u2_t i = 0; i < path.size(); ++i) if(path[i] == '/') path[i] = '\\';
	// Use a full path, never PATH lookup or the connector's working directory.
	if(path.size() < 4 || !((path[1] == ':' && path[2] == '\\') || (path[0] == '\\' && path[1] == '\\'))) {
		if_fail_fe(text.append_string("Select lls.exe or lrt.exe using its absolute path."));
		rtrn OS_INVALID_PARAMETER;
	}
	cnst char * basename = strrchr(path.begin(), '\\');
	if(!basename) { if_fail_fe(text.append_string("The selected executable has no directory.")); rtrn OS_INVALID_PARAMETER; }
	++basename;
	cnst bool module = _stricmp(basename, "lrt.exe") == 0;
	if(!module && _stricmp(basename, "lls.exe") != 0) {
		if_fail_fe(text.append_string("Only lls.exe or lrt.exe can start the LLS service."));
		rtrn OS_INVALID_PARAMETER;
	}
	if(path.find('"') >= 0 || path.find('\0') >= 0) { if_fail_fe(text.append_string("The selected executable path is invalid.")); rtrn OS_INVALID_PARAMETER; }
	cnst DWORD attributes = GetFileAttributesA(path.begin());
	if(attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
		if_fail_fe(append_strings(text, "Executable not found: ", path));
		rtrn OS_NOT_FOUND;
	}
	if_fail_fe(directory.append(path.begin(), (u2_t)(basename - path.begin())));
	if_fail_fe(append_strings(command, '"', path, '"'));
	if(module) { if_fail_fe(command.append_string(" lls")); }
	string resource;
	if_fail_fe(append_strings(resource, directory, "lls_tools.json"));
	if(GetFileAttributesA(resource.begin()) == INVALID_FILE_ATTRIBUTES) { if_fail_fe(append_strings(text, "Missing service resource: ", resource)); rtrn OS_NOT_FOUND; }
	if_fail_fe(resource.clear());
	if_fail_fe(append_strings(resource, directory, "lcp_help.json"));
	if(GetFileAttributesA(resource.begin()) == INVALID_FILE_ATTRIBUTES) { if_fail_fe(append_strings(text, "Missing service resource: ", resource)); rtrn OS_NOT_FOUND; }
	if(module) {
		if_fail_fe(resource.clear());
		if_fail_fe(append_strings(resource, directory, "lls.dll"));
		if(GetFileAttributesA(resource.begin()) == INVALID_FILE_ATTRIBUTES) { if_fail_fe(append_strings(text, "Missing service module: ", resource)); rtrn OS_NOT_FOUND; }
	}
	rtrn 0;
}
err_t control(vcst_t action, vcst_t configuration, vcst_t executablePath, string & text) {
	if_true_ve(OS_INVALID_PARAMETER, configuration != "Debug" && configuration != "Release");
	if_true_ve(OS_INVALID_PARAMETER, action != "launch" && action != "shutdown" && action != "restart" && action != "status");
	cnst auto & names = configuration == "Debug" ? lls::ControlDebug : lls::ControlRelease;
	SOperation operation;
	if(action != "status") {
		operation.Handle = CreateMutexA(0, FALSE, names.Operation);
		if_null_ve(OS_ERROR, operation.Handle);
		cnst DWORD waited = WaitForSingleObject(operation.Handle, 0);
		operation.Owned = waited == WAIT_OBJECT_0 || waited == WAIT_ABANDONED;
		if_true_ve(OS_BUSY, !operation.Owned);
	}
	HWND window = 0;
	DWORD pid = 0;
	cnst err_t located = locate(names, window, pid);
	if(failed(located)) {
		if_fail_fe(text.append_string("LLS is starting, inaccessible, or lacks the control interface. No process was launched or stopped."));
		rtrn located;
	}
	if(action == "status")
		rtrn append_strings(text, "{\"running\":", b82vcs(window != 0), ",\"pid\":", str((u2_t)pid), '}');
	if(action == "launch" && window)
		rtrn append_strings(text, "LLS already running; pid ", str((u2_t)pid), ". No second instance launched.");
	if(action == "shutdown" && !window) rtrn text.append_string("LLS is already stopped.");
	string path, directory, command;
	if(action == "launch" || action == "restart") { if_fail_fe(executable(executablePath, text, path, directory, command)); }
	if(window) {
		SHandle process;
		process.Handle = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
		if_null_ve(OS_ERROR, process.Handle);
		if(action == "restart") {
			char actual[32768] = {};
			DWORD size = sizeof(actual);
			if_zero_ve(OS_ERROR, QueryFullProcessImageNameA(process.Handle, 0, actual, &size));
			if_true_ve(OS_INVALID_PARAMETER, _stricmp(actual, path.begin()) != 0);
		}
		DWORD current = 0;
		if_zero_ve(OS_ERROR, GetWindowThreadProcessId(window, &current));
		if_true_ve(OS_BUSY, current != pid || GetPropA(window, names.Window) != (HANDLE)1);
		cnst UINT message = RegisterWindowMessageA(lls::ShutdownMessage);
		if_zero_ve(OS_ERROR, message);
		if_zero_ve(OS_ERROR, PostMessageA(window, message, 0, 0));
		cnst DWORD waited = WaitForSingleObject(process.Handle, 30000);
		if(waited == WAIT_TIMEOUT) {
			if_fail_fe(text.append_string("Shutdown requested; still finishing work after 30 seconds. No replacement launched. Use lls status before retrying."));
			rtrn action == "restart" ? OS_BUSY : 0;
		}
		if_true_ve(OS_ERROR, waited != WAIT_OBJECT_0);
		if(action == "shutdown") rtrn text.append_string("LLS stopped; process exit confirmed.");
	}
	STARTUPINFOA startup = {sizeof(STARTUPINFOA)};
	PROCESS_INFORMATION info = {};
	// Service owns its lifetime and GUI; never inherit MCP stdin/stdout handles.
	if_zero_ve(OS_ERROR, CreateProcessA(path.begin(), command.Data, 0, 0, FALSE, CREATE_NO_WINDOW, 0, directory.begin(), &startup, &info));
	SHandle process, thread;
	process.Handle = info.hProcess;
	thread.Handle = info.hThread;
	for(u2_t attempt = 0; attempt < 100; ++attempt) {
		cnst DWORD waited = WaitForSingleObject(process.Handle, 100);
		if_true_ve(OS_ERROR, waited == WAIT_FAILED);
		if(waited == WAIT_OBJECT_0) {
			DWORD code = 0;
			if_zero_ve(OS_ERROR, GetExitCodeProcess(process.Handle, &code));
			if_fail_fe(append_strings(text, "LLS host exited before readiness; exit code ", str((u2_t)code), '.'));
			rtrn OS_ERROR;
		}
		HWND ready = 0;
		DWORD readyPID = 0;
		if(succeeded(locate(names, ready, readyPID)) && ready && readyPID == info.dwProcessId)
			rtrn append_strings(text, "LLS ready; pid ", str((u2_t)readyPID), ". Running independently of LCP.");
	}
	if_fail_fe(append_strings(text, "Host started; pid ", str((u2_t)info.dwProcessId), ", but requested LLS configuration did not become ready within 10 seconds. Process left running; inspect before retrying."));
	rtrn OS_BUSY;
}
} // namespace
#endif

llc::err_t lcpServiceControl(llc::vcst_t action, llc::vcst_t configuration, llc::vcst_t executable, llc::string & text) {
#ifdef LLC_WINDOWS
	cnst llc::err_t result = control(action, configuration, executable, text);
	if(llc::failed(result) && !text.size()) {
		if_fail_fe(llc::append_strings(text, "LLS control failed (", llc::str(result), "). No forced termination is performed."));
	}
	rtrn result;
#else
	(void)action; (void)configuration; (void)executable;
	if_fail_fe(text.append_string("LLS desktop control is Windows-only."));
	rtrn -1;
#endif
}

llc::err_t lcpServiceCall(cnst llc::SJSONReader & reader, llc::string & body) {
	llc::vcst_t action, configuration;
	if_fail_fe(llc::jsonExpressionResolve("params.arguments.action", reader, 0, action));
	if_fail_fe(llc::jsonExpressionResolve("params.arguments.configuration", reader, 0, configuration));
	llc::string executable;
	if(action == "launch" || action == "restart") {
		llc::vcst_t encoded;
		if_fail_fe(llc::jsonExpressionResolve("params.arguments.executable", reader, 0, encoded));
		if_fail_fe(lcpStringDecode(encoded, executable));
		if(!executable.size()) { if_fail_fe(executable.append(encoded)); }
	}
	llc::string text;
	cnst llc::err_t result = lcpServiceControl(action, configuration, executable, text);
	rtrn lcpTextBody(body, text, llc::failed(result));
}
