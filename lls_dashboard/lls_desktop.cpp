#include "dashboard_app.h"
#include "../lls/lls_control.h"

#ifdef LLC_WINDOWS
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

namespace {

stct SDesktop {
	HANDLE Instance = 0, Activate = 0;
	WNDPROC Previous = 0;
	NOTIFYICONDATAA Icon = {sizeof(NOTIFYICONDATAA)};
	UINT TaskbarCreated = 0, Shutdown = 0;
	bool Added = false, Published = false;
	~SDesktop() { if(Activate) CloseHandle(Activate); if(Instance) CloseHandle(Instance); }
} Desktop;

cnst UINT TrayMessage = WM_APP + 42;
cnst UINT ToggleDashboard = 1001, Quit = 1002;
cnst char AppProperty[] = "LLS.Application";
#ifdef _DEBUG
cnst lls::SControlNames Control = {"Local\\LT4.Dashboard.Debug.Instance", "LT4.Dashboard.Debug.Control", ""};
cnst char ActivateName[] = "Local\\LT4.Dashboard.Debug.Activate";
cnst char WindowTitle[] = "LLS Dashboard Debug";
#else
cnst lls::SControlNames Control = {"Local\\LT4.Dashboard.Release.Instance", "LT4.Dashboard.Release.Control", ""};
cnst char ActivateName[] = "Local\\LT4.Dashboard.Release.Activate";
cnst char WindowTitle[] = "LLS Dashboard Release";
#endif

void showDashboard(HWND window) {
	ShowWindow(window, SW_RESTORE);
	SetForegroundWindow(window);
}

void requestShutdown(SApplication & app, HWND window) {
	app.Closing = true;
	SetWindowTextA(window, "LLS Dashboard - Closing");
}

LRESULT CALLBACK desktopWindow(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
	auto * app = (SApplication*)GetPropA(window, AppProperty);
	if(message == Desktop.Shutdown && Desktop.Shutdown && app) {
		requestShutdown(*app, window);
		rtrn 0;
	}
	if(message == WM_SHOWWINDOW && app) (*app->UI).Framework.RootWindow.NoDraw = !wparam;
	if(message == WM_GETMINMAXINFO) {
		auto & size = *(MINMAXINFO*)lparam;
		size.ptMinTrackSize = {720, 520};
		rtrn 0;
	}
	if(message == WM_CLOSE || (message == WM_SIZE && wparam == SIZE_MINIMIZED)) {
		ShowWindow(window, SW_HIDE);
		rtrn 0;
	}
	if(message == Desktop.TaskbarCreated) {
		Desktop.Added = Shell_NotifyIconA(NIM_ADD, &Desktop.Icon) != FALSE;
		rtrn 0;
	}
	if(message == WM_COMMAND && HIWORD(wparam) == 0) {
		if(LOWORD(wparam) == ToggleDashboard) {
			if(IsWindowVisible(window)) ShowWindow(window, SW_HIDE);
			else showDashboard(window);
			rtrn 0;
		}
		if(LOWORD(wparam) == Quit && app) {
			requestShutdown(*app, window);
			rtrn 0;
		}
	}
	if(message == TrayMessage) {
		if(lparam == WM_LBUTTONUP || lparam == WM_LBUTTONDBLCLK) showDashboard(window);
		if(lparam == WM_RBUTTONUP) {
			HMENU menu = CreatePopupMenu();
			if(!menu) rtrn 0;
			AppendMenuA(menu, MF_STRING, ToggleDashboard, IsWindowVisible(window) ? "Hide dashboard" : "Show dashboard");
			AppendMenuA(menu, MF_SEPARATOR, 0, 0);
			AppendMenuA(menu, MF_STRING, Quit, "Quit dashboard (service stays running)");
			POINT point = {};
			GetCursorPos(&point);
			SetForegroundWindow(window);
			cnst UINT selected = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, 0, window, 0);
			DestroyMenu(menu);
			PostMessageA(window, WM_NULL, 0, 0);
			if(selected) PostMessageA(window, WM_COMMAND, selected, 0);
		}
		rtrn 0;
	}
	rtrn CallWindowProcA(Desktop.Previous, window, message, wparam, lparam);
}
}

llc::err_t llsDesktopStart() {
	// One dashboard per build in this Windows session; service ownership is separate.
	// Access denied is an error, never permission to start a duplicate dashboard.
	Desktop.Activate = CreateEventA(0, FALSE, FALSE, ActivateName);
	if_null_ve(llc::OS_ERROR, Desktop.Activate);
	Desktop.Instance = CreateMutexA(0, FALSE, Control.Instance);
	cnst DWORD status = GetLastError();
	if_null_ve(llc::OS_ERROR, Desktop.Instance);
	if(status == ERROR_ALREADY_EXISTS) {
		if_zero_ve(llc::OS_ERROR, SetEvent(Desktop.Activate));
		rtrn 1;
	}
	rtrn 0;
}

llc::err_t llsDesktopAttach(SApplication & app) {
	HWND window = (*app.UI).Framework.RootWindow.PlatformDetail.WindowHandle;
	SetWindowTextA(window, WindowTitle);
	if_zero_ve(llc::OS_ERROR, SetPropA(window, AppProperty, &app));
	Desktop.Previous = (WNDPROC)SetWindowLongPtrA(window, GWLP_WNDPROC, (LONG_PTR)desktopWindow);
	if_null_ve(llc::OS_ERROR, Desktop.Previous);
	Desktop.TaskbarCreated = RegisterWindowMessageA("TaskbarCreated");
	Desktop.Icon.hWnd = window;
	Desktop.Icon.uID = 1;
	Desktop.Icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	Desktop.Icon.uCallbackMessage = TrayMessage;
	Desktop.Icon.hIcon = LoadIconA(0, IDI_APPLICATION);
	lstrcpynA(Desktop.Icon.szTip, WindowTitle, sizeof(Desktop.Icon.szTip));
	Desktop.Added = Shell_NotifyIconA(NIM_ADD, &Desktop.Icon) != FALSE;
	if_zero_ve(llc::OS_ERROR, Desktop.Added);
	Desktop.Shutdown = RegisterWindowMessageA("LT4.Dashboard.Quit.v1");
	if_zero_ve(llc::OS_ERROR, Desktop.Shutdown);
	rtrn 0;
}

llc::err_t llsDesktopUpdate(SApplication & app) {
	if(!Desktop.Published) {
		if_zero_ve(llc::OS_ERROR, SetPropA((*app.UI).Framework.RootWindow.PlatformDetail.WindowHandle, Control.Window, (HANDLE)1));
		Desktop.Published = true;
	}
	if(WaitForSingleObject(Desktop.Activate, 0) == WAIT_OBJECT_0)
		showDashboard((*app.UI).Framework.RootWindow.PlatformDetail.WindowHandle);
	rtrn 0;
}

llc::err_t llsDesktopClose(SApplication & app) {
	if(Desktop.Added) Shell_NotifyIconA(NIM_DELETE, &Desktop.Icon);
	Desktop.Added = false;
	if(app.UI) {
		HWND window = (*app.UI).Framework.RootWindow.PlatformDetail.WindowHandle;
		if(window && Desktop.Previous) SetWindowLongPtrA(window, GWLP_WNDPROC, (LONG_PTR)Desktop.Previous);
		if(window) RemovePropA(window, AppProperty);
		if(window) RemovePropA(window, Control.Window);
	}
	Desktop.Previous = 0;
	Desktop.Published = false;
	if(Desktop.Activate) { CloseHandle(Desktop.Activate); Desktop.Activate = 0; }
	if(Desktop.Instance) { CloseHandle(Desktop.Instance); Desktop.Instance = 0; }
	rtrn 0;
}
#else
llc::err_t llsDesktopStart() { rtrn -1; } // Notification-area host is Windows-only for now.
llc::err_t llsDesktopAttach(SApplication &) { rtrn -1; }
llc::err_t llsDesktopUpdate(SApplication &) { rtrn 0; }
llc::err_t llsDesktopClose(SApplication &) { rtrn 0; }
#endif
