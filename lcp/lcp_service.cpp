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
err_t argument(cnst SJSONReader & reader, vcst_t expression, string & value) {
	vcst_t text;
	if_fail_fe(jsonExpressionResolve(expression, reader, 0, text));
	if_fail_fe(lcpStringDecode(text, value));
	if(!value.size()) { if_fail_fe(value.append(text)); }
	rtrn 0;
}
err_t executable(cnst SJSONReader & reader, string & path, string & directory, string & command) {
	if_fail_fe(argument(reader, "params.arguments.executable", path));
	for(u2_t i = 0; i < path.size(); ++i) if(path[i] == '/') path[i] = '\\';
	// Use a full path, never PATH lookup or the connector's working directory.
	if_true_ve(OS_INVALID_PARAMETER, path.size() < 4 || !((path[1] == ':' && path[2] == '\\') || (path[0] == '\\' && path[1] == '\\')));
	cnst char * basename = strrchr(path.begin(), '\\');
	if_null_ve(OS_INVALID_PARAMETER, basename);
	++basename;
	cnst bool module = _stricmp(basename, "lrt.exe") == 0;
	if_true_ve(OS_INVALID_PARAMETER, !module && _stricmp(basename, "lls.exe") != 0);
	if_true_ve(OS_INVALID_PARAMETER, path.find('"') >= 0 || path.find('\0') >= 0);
	cnst DWORD attributes = GetFileAttributesA(path.begin());
	if_true_ve(OS_NOT_FOUND, attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY));
	if_fail_fe(directory.append(path.begin(), (u2_t)(basename - path.begin())));
	if_fail_fe(append_strings(command, '"', path, '"'));
	if(module) { if_fail_fe(command.append_string(" lls")); }
	string resource;
	if_fail_fe(append_strings(resource, directory, "lls_tools.json"));
	if_true_ve(OS_NOT_FOUND, GetFileAttributesA(resource.begin()) == INVALID_FILE_ATTRIBUTES);
	if_fail_fe(resource.clear());
	if_fail_fe(append_strings(resource, directory, "lcp_help.json"));
	if_true_ve(OS_NOT_FOUND, GetFileAttributesA(resource.begin()) == INVALID_FILE_ATTRIBUTES);
	if(module) {
		if_fail_fe(resource.clear());
		if_fail_fe(append_strings(resource, directory, "lls.dll"));
		if_true_ve(OS_NOT_FOUND, GetFileAttributesA(resource.begin()) == INVALID_FILE_ATTRIBUTES);
	}
	rtrn 0;
}
err_t control(cnst SJSONReader & reader, string & text) {
	vcst_t action, configuration;
	if_fail_fe(jsonExpressionResolve("params.arguments.action", reader, 0, action));
	if_fail_fe(jsonExpressionResolve("params.arguments.configuration", reader, 0, configuration));
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
	if(action == "launch" || action == "restart") { if_fail_fe(executable(reader, path, directory, command)); }
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

llc::err_t lcpServiceCall(cnst llc::SJSONReader & reader, llc::string & body) {
	llc::string text;
#ifdef LLC_WINDOWS
	cnst llc::err_t result = control(reader, text);
	if(llc::failed(result) && !text.size()) {
		if_fail_fe(llc::append_strings(text, "LLS control failed (", llc::str(result), "). Open help: lls. No forced termination is performed."));
	}
	rtrn lcpTextBody(body, text, llc::failed(result));
#else
	(void)reader;
	rtrn lcpTextBody(body, "LLS desktop control is Windows-only.", true);
#endif
}
