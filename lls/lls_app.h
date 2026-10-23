#include "lls_gui.h"
#include "llc_mutex.h"
#include "llc_timer.h"

#ifndef LLS_APP_H
#define LLS_APP_H

stct SBuildResult {
	llc::err_t			Status = {};
	llc::u2_t			ExitCode = {};
	bool				ExitKnown = {};
};
stct SBuildJob {
	llc::string			Id, Request, Command, Output, Response;
	llc::vcst_t			State = "queued";
	llc::STimer			Timer;
	SBuildResult		Result;
	llc::u2_t			PID = 0;
	bool				ProcessStarted = false, ProcessFinished = false, Detached = false;
};
stct SApplication {
	llc::SRuntimeValues	& Runtime;
	llc::SJSONReader		Reader;
	llc::string			Request;
	llc::aobj<SBuildJob>	Jobs;
	llc::u2_t			NextJob = {}, Revision = {}, Requests = {};
	llc::mutex			Lock;
	llc::err_t			WorkerResult = {};
	llc::STimer			Session;
	llc::pobj<SLLSGUI>	UI;
	bool				Started = {}, InputClosed = {}, Busy = {}, Closing = {}, Secondary = {};
	explicit SApplication(llc::SRuntimeValues & runtime) : Runtime(runtime) {}
};

llc::err_t llsInputAvailable();
llc::err_t llsDesktopStart();
llc::err_t llsDesktopAttach(SApplication & app);
llc::err_t llsDesktopUpdate(SApplication & app);
llc::err_t llsDesktopClose(SApplication & app);
llc::err_t llsProcess(SApplication & app, llc::u2_t job, llc::vcst_t application, llc::vcst_t command, bool build, llc::au0_t & output, llc::u2_t & exitCode);

#endif
