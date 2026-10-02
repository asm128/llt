#include "lls-l.h"

#include "llc_runtime.h"


LLC_USING_TYPEINT();
LLC_USING_APOD();
LLC_USING_VIEW();

sttc	llc::err_t	lls_entry_point		(llc::SRuntimeValues & runtimeValues);
LLC_SYSTEM_OS_ENTRY_POINT(::lls_entry_point);

sttc	llc::err_t	llsProcessRequest	(const llc::SEventSystem & request, llc::SEventSystem & response, ::lls::SServiceStatus & status, bool & stopRequested) {
	::lls::SEViewCommand	command		= {};
	if_fail_ve(::lls::eventMakeResult(response, ::lls::LLS_RESULT_Invalid_request, vcst_t{"Expected a system command event."}), ::lls::eventExtractCommand(request, command)));

	info_printf("Request #%llu: %s", status.RequestCount, llc::get_value_namep(command.Type));
	switch(command.Type) {
	default							: return ::lls::eventMakeResult(response, ::lls::LLS_RESULT_Invalid_request, vcst_t{"Unknown lls command."});
	case ::lls::LLS_COMMAND_Ping	: return ::lls::eventMakeResult(response, ::lls::LLS_RESULT_Pong, command.Data);
	case ::lls::LLS_COMMAND_Status	: return ::lls::eventMakeResult(response, ::lls::LLS_RESULT_Status, vcu0_t((u0_c*)&status, sizeof(status)));
	case ::lls::LLS_COMMAND_Shutdown:
		stopRequested	= true;
		return ::lls::eventMakeResult(response, ::lls::LLS_RESULT_Shutting_down, vcst_t{"lls is shutting down."});
	}
}

sttc	llc::err_t	lls_entry_point		(llc::SRuntimeValues & runtimeValues) {
	(void)runtimeValues;
	::lls::pipe_t			serverPipe		= 0;
	llc_necs(::lls::pipeServerCreate(serverPipe));

	const u3_t		startedAt		= ::GetTickCount64();
	::lls::SServiceStatus	status			= {};
	status.ProcessId						= ::GetCurrentProcessId();
	info_printf("lls protocol %u listening at %s (pid %u).", status.ProtocolVersion, ::lls::PIPE_NAME, status.ProcessId);

	bool			stopRequested	= false;
	llc::err_t		waitResult;
	while(false == stopRequested) {
		if_fail_bef(waitResult = ::lls::pipeServerWait(serverPipe), "%s", "Failed while waiting for an lls client.");

		llc::SEventSystem		request			= {};
		llc::SEventSystem		response		= {};
		{
			llc::err_t			readResult;
			if_true_block_logf(error_printf, llc::failed(readResult = ::lls::pipeReadEvent(serverPipe, request)), {
				if_fail_e(::lls::pipeServerDisconnect(serverPipe));
				continue;
				}, "%s", "Failed to read an lls request.");
		}
		++status.RequestCount;
		status.UptimeMilliseconds = ::GetTickCount64() - startedAt; // Use llc::STimer instead of platform-specific GetTickCount64()
		if_fail_ce(::llsProcessRequest(request, response, status, stopRequested), "%s", "Failed to process request. Skip to next");
		{

			llc::err_t	writeResult;
			if_fail_e(writeResult = ::lls::pipeWriteEvent(serverPipe, response),"Failed to write an lls response.");
			if_fail_e(::lls::pipeServerDisconnect(serverPipe));
		}
	}
	if_fail_e(::lls::pipeClose(serverPipe));
	info_printf("lls stopped after %llu requests.", status.RequestCount);
	return waitResult;
}
