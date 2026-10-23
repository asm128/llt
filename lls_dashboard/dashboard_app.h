#include "../lcp/lcp_relay.h"
#include "lls_gui.h"
#include "llc_mutex.h"
#include "llc_timer.h"
#include <atomic>

#ifndef DASHBOARD_APP_H
#define DASHBOARD_APP_H
stct SDashboardJob {
	llc::string Id, State, Command, Request, Output, Response;
	llc::u2_t PID = 0;
	llc::u3_t ElapsedMs = 0, OriginSession = 0;
	bool Historical = false;
	stct { llc::err_t Status = 0; llc::u2_t ExitCode = 0; bool ExitKnown = false; } Result;
	bool ProcessStarted = false, ProcessFinished = false, Detached = false;
};
stct SApplication {
	llc::SRuntimeValues & Runtime;
	llc::pobj<SLLSGUI> UI;
	llc::aobj<SDashboardJob> Jobs;
	llc::mutex Lock;
	llc::string Identity, Title, ServiceIdentity, Diagnostics, LogPath, Connection = "Connecting", Service = "Debug";
	llc::u3_t SessionId = 0, ElapsedMs = 0, LastSnapshot = 0, NextPoll = 0;
	int LogError = 0, HistoryError = 0;
	llc::u2_t Requests = 0, Revision = 0, ServiceRevision = (llc::u2_t)-1;
	SRelayPeer Peer;
	bool Connected = false, Secondary = false, NetworkStarted = false, Restart = false;
	std::atomic<bool> Closing = false;
	explicit SApplication(llc::SRuntimeValues & runtime) : Runtime(runtime) {}
};
llc::err_t llsDesktopStart();
llc::err_t llsDesktopAttach(SApplication & app);
llc::err_t llsDesktopUpdate(SApplication & app);
llc::err_t llsDesktopClose(SApplication & app);
llc::err_t llsSearchTest();
#endif
