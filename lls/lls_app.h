#include "../lcp/lcp_relay.h"
#include "../lcp/lcp_metrics.h"
#include "llc_runtime.h"
#include "llc_json.h"
#include "llc_mutex.h"
#include "llc_timer.h"
#include "lls_log.h"

#ifndef LLS_APP_H
#define LLS_APP_H

stct SBuildResult {
	llc::err_t			Status = {};
	llc::u2_t			ExitCode = {};
	bool				ExitKnown = {};
};
stct SBuildJob {
	llc::pobj<SRelayPeer>	Peer;
	llc::string			Id, Request, Command, Output, Response, OutputPath;
	llc::err_t OutputLogStatus = 0;
	llc::u3_t OriginSession = 0, SavedElapsedMs = 0;
	llc::u2_t Sequence = 0;
	bool Historical = false;
	llc::vcst_t			State = "queued";
	llc::STimer			Timer;
	SBuildResult		Result;
	llc::u2_t			PID = 0;
	bool				ProcessStarted = false, ProcessFinished = false, Detached = false;
};
stct SApplication {
	llc::SRuntimeValues	& Runtime;
	llc::SJSONReader		Reader;
	SRelayServer		Relay;
	llc::u3_t SessionId = 0;
	llc::string Identity;
	SServiceLog Log;
	SCommunicationMetrics Metrics;
	llc::string HistoryRoot;
	llc::err_t HistoryError = 0;
	bool				NetworkStarted = false;
	llc::string			Request;
	llc::aobj<SBuildJob>	Jobs;
	llc::u2_t			NextJob = {}, Revision = {}, Requests = {};
	llc::mutex			Lock;
	llc::err_t			WorkerResult = {};
	llc::STimer			Session;
	bool				Started = {}, InputClosed = {}, Busy = {}, Closing = {}, Secondary = {};
	explicit SApplication(llc::SRuntimeValues & runtime) : Runtime(runtime) {}
};

llc::err_t llsInputAvailable();
llc::err_t llsDesktopStart();
llc::err_t llsDesktopAttach(SApplication & app);
llc::err_t llsDesktopUpdate(SApplication & app);
llc::err_t llsDesktopClose(SApplication & app);
llc::err_t llsProcess(SApplication & app, llc::u2_t job, llc::vcst_t application, llc::vcst_t command, bool capture, llc::au0_t & output, llc::u2_t & exitCode, llc::vcu0_t input = {});

llc::err_t llsSession(cnst llc::SJSONReader & reader, SApplication & app, llc::string & body);
llc::err_t llsFailures(cnst llc::SJSONReader & reader, SApplication & app, llc::string & body);
llc::err_t llsJobSummary(cnst SBuildJob & job, llc::string & summary);
#ifdef _DEBUG
llc::err_t llsJobSummaryTest();
#endif
llc::err_t llsJobJSON(cnst SBuildJob & job, llc::string & json);
llc::err_t llsHistoryLoad(SApplication & app);
llc::err_t llsHistorySave(SApplication & app, cnst SBuildJob & job);
#endif
