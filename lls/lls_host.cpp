#include "lls_app.h"
#include "lls_control.h"

#ifdef LLC_WINDOWS
#pragma comment(lib, "user32.lib")
namespace {
HANDLE Instance = {};
HWND Window = {};
UINT Shutdown = {};
bool Published = false;
cnst char ClassName[] = "LT4.LLS.ServiceControl";
#ifdef _DEBUG
cnst auto & Control = lls::ControlDebug;
#else
cnst auto & Control = lls::ControlRelease;
#endif
LRESULT CALLBACK controlWindow(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
	auto * app = (SApplication*)GetWindowLongPtrA(window, GWLP_USERDATA);
	if(message == Shutdown && Shutdown && app) {
		app->Closing = app->InputClosed = true;
		rtrn 0;
	}
	rtrn DefWindowProcA(window, message, wparam, lparam);
}
}
llc::err_t llsDesktopStart() {
	Instance = CreateMutexA(0, FALSE, Control.Instance);
	cnst DWORD status = GetLastError();
	if_null_fe(Instance);
	rtrn status == ERROR_ALREADY_EXISTS ? 1 : 0;
}
llc::err_t llsDesktopAttach(SApplication & app) {
	WNDCLASSA type = {};
	type.lpfnWndProc = controlWindow;
	type.hInstance = GetModuleHandleA(nullptr);
	type.lpszClassName = ClassName;
	if_zero_fe(RegisterClassA(&type));
	// Invisible control endpoint only. No SGUI, tray, render target or dashboard.
	Window = CreateWindowExA(0, ClassName, "LLS service control", 0, 0, 0, 0, 0, 0, 0, type.hInstance, 0);
	if_null_fe(Window);
	SetWindowLongPtrA(Window, GWLP_USERDATA, (LONG_PTR)&app);
	Shutdown = RegisterWindowMessageA(lls::ShutdownMessage);
	if_zero_fe(Shutdown);
	rtrn 0;
}
llc::err_t llsDesktopUpdate(SApplication &) {
	if(!Published) {
		if_zero_fe(SetPropA(Window, Control.Window, (HANDLE)1));
		Published = true;
	}
	MSG message = {};
	while(PeekMessageA(&message, Window, 0, 0, PM_REMOVE)) DispatchMessageA(&message);
	rtrn 0;
}
llc::err_t llsDesktopClose(SApplication &) {
	if(Window) {
		RemovePropA(Window, Control.Window);
		DestroyWindow(Window);
		Window = {};
		UnregisterClassA(ClassName, GetModuleHandleA(nullptr));
	}
	if(Instance) { CloseHandle(Instance); Instance = {}; }
	Published = false;
	rtrn 0;
}
#else
llc::err_t llsDesktopStart() { rtrn 0; }
llc::err_t llsDesktopAttach(SApplication &) { rtrn 0; }
llc::err_t llsDesktopUpdate(SApplication &) { rtrn 0; }
llc::err_t llsDesktopClose(SApplication &) { rtrn 0; }
#endif
