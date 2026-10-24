#include "lls_app.h"
#include "llc_process.h"

#ifdef LLC_LINUX
#include <poll.h>
#include <unistd.h>
#endif

// Do not stall the window thread while waiting for another request.
llc::err_t llsInputAvailable() {
#ifdef LLC_WINDOWS
	HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
	cnst DWORD type = GetFileType(input);
	if(type == FILE_TYPE_CHAR || type == FILE_TYPE_UNKNOWN) rtrn 0;
	if(type == FILE_TYPE_DISK) rtrn 1;
	DWORD bytes = 0;
	if(PeekNamedPipe(input, 0, 0, 0, &bytes, 0)) rtrn bytes ? 1 : 0;
	rtrn GetLastError() == ERROR_BROKEN_PIPE ? 1 : llc::OS_ERROR;
#elif defined(LLC_LINUX)
	if(isatty(STDIN_FILENO)) rtrn 0;
	pollfd input = {STDIN_FILENO, POLLIN, 0};
	cnst int result = poll(&input, 1, 0);
	rtrn result < 0 ? llc::OS_ERROR : result;
#else
	rtrn 0;
#endif
}

llc::err_t llsProcess(SApplication & app, llc::u2_t jobIndex, llc::vcst_t application, llc::vcst_t command, bool capture, llc::au0_t & output, llc::u2_t & exitCode, llc::vcu0_t input) {
	{
		llc::lock_guard guard(app.Lock);
		if_fail_fe(app.Jobs[jobIndex].Command.append(command));
		++app.Revision;
	}
	llc::SProcessManager processes;
	llc::u2_t flags = 0;
#ifdef LLC_WINDOWS
	if(capture) flags = CREATE_NO_WINDOW;
#endif
	cnst llc::err_t id = processes.Create(application, command, {}, flags);
	if_fail_fe(id);
	llc::SProcessManager<>::TProcessScope scope{processes, (llc::u2_t)id};
	if(capture) {
		if_fail_fe(processes.Redirect(id));
	}
	if_fail_fe(processes.Launch(id));
	{
		llc::lock_guard guard(app.Lock);
		SBuildJob & job = app.Jobs[jobIndex];
		job.ProcessStarted = true;
		job.Detached = !capture;
#ifdef LLC_WINDOWS
		job.PID = processes.PlatformState[id].Info.Id;
#elif defined(LLC_LINUX)
		job.PID = processes.PlatformState[id].PID;
#endif
		++app.Revision;
	}
	if(capture) {
		// Drain output concurrently with supplied input, preserving the published child PID.
		if_fail_fe(lcpMetricsRecord(COMMUNICATION_CHANNEL::Process, false, input));
		if_fail_fe(processes.Communicate(id, input, output));
		if_fail_fe(lcpMetricsRecord(COMMUNICATION_CHANNEL::Process, true, output));
		if_fail_fe(processes.Wait(id, exitCode));
		llc::lock_guard guard(app.Lock);
		if_fail_fe(app.Jobs[jobIndex].Output.append({(cnst char*)output.begin(), output.size()}));
		app.Jobs[jobIndex].ProcessFinished = true;
		++app.Revision;
	}
	rtrn scope.Close();
}
