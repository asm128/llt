#include "lls_app.h"
#include "llc_process_broker.h"
#include "llc_args.h"

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
	rtrn GetLastError() == ERROR_BROKEN_PIPE ? 1 : -1;
#elif defined(LLC_LINUX)
	if(isatty(STDIN_FILENO)) rtrn 0;
	pollfd input = {STDIN_FILENO, POLLIN, 0};
	cnst int result = poll(&input, 1, 0);
	rtrn result < 0 ? -1 : result;
#else
	rtrn 0;
#endif
}

llc::err_t llsProcess(SApplication & app, llc::u2_t jobIndex, llc::vcst_t application, llc::vcst_t command, bool capture, llc::au0_t & output, llc::u2_t & exitCode, llc::vcu0_t input) {
	if_true_fe(!capture);
	{
		llc::lock_guard guard(app.Lock);
		if_fail_fe(app.Jobs[jobIndex].Command.append(command));
		++app.Revision;
	}
	llc::SProcessBrokerJob process;
	process.CommandLineApp = application;
	process.CommandLine = command;
	llc::as0_t environment;
	if_fail_fe(llc::environmentBlock(environment, app.Runtime.EntryPointArgs.Environment));
	if_fail_fe(process.Request.append((cnst llc::u0_t*)environment.begin(), environment.size()));
	if_fail_fe(process.Request.append(input));
	if_fail_fe(lcpMetricsRecord(COMMUNICATION_CHANNEL::Process, false, input));
	cnst llc::err_t result = llc::processBrokerRun(process);
	if_fail_fe(output.append(process.Response));
	if_fail_fe(output.append(process.ErrorOutput));
	if_fail_fe(lcpMetricsRecord(COMMUNICATION_CHANNEL::Process, true, output));
#ifdef LLC_WINDOWS
	exitCode = 0;
#elif defined(LLC_LINUX)
	exitCode = process.Process.ExitCode;
#endif
	{
		llc::lock_guard guard(app.Lock);
		SBuildJob & job = app.Jobs[jobIndex];
		job.ProcessStarted = llc::succeeded(result);
		job.ProcessFinished = true;
		job.Detached = false;
#ifdef LLC_WINDOWS
		job.PID = process.Process.ProcessInfo.dwProcessId;
#elif defined(LLC_LINUX)
		job.PID = 0;
#endif
		if_fail_fe(app.Jobs[jobIndex].Output.append({(cnst char*)output.begin(), output.size()}));
		++app.Revision;
	}
	rtrn result;
}
