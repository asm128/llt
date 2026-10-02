#include "lls-l.h"

#include "llc_runtime.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstring>

sttc	::llc::err_t	lls_entry_point		(::llc::SRuntimeValues & runtimeValues);
LLC_SYSTEM_OS_ENTRY_POINT(::lls_entry_point);

LLC_USING_TYPEINT();
LLC_USING_APOD();
LLC_USING_VIEW();

sttc	::llc::vcu0_t	bytesView			(const void * data, ::llc::u2_t byteCount) {
	return {(const uint8_t*)data, byteCount};
}

sttc	::llc::vcu0_t	textView			(const char * text) {
	return ::bytesView(text, (::llc::u2_t)::strlen(text));
}

sttc	::llc::err_t	llsProcessRequest	(const ::llc::SEventSystem & request, ::llc::SEventSystem & response, ::lls::SServiceStatus & status, bool & stopRequested) {
	::lls::SEViewCommand	command		= {};
	if_fail(::lls::eventExtractCommand(request, command))
		return ::lls::eventMakeResult(response, ::lls::LLS_RESULT_Invalid_request, ::textView("Expected a system command event."));

	info_printf("Request #%llu: %s", status.RequestCount, ::llc::get_value_namep(command.Type));
	switch(command.Type) {
	case ::lls::LLS_COMMAND_Ping:
		return ::lls::eventMakeResult(response, ::lls::LLS_RESULT_Pong, command.Data);
	case ::lls::LLS_COMMAND_Status:
		return ::lls::eventMakeResult(response, ::lls::LLS_RESULT_Status, ::bytesView(&status, sizeof(status)));
	case ::lls::LLS_COMMAND_Shutdown:
		stopRequested	= true;
		return ::lls::eventMakeResult(response, ::lls::LLS_RESULT_Shutting_down, ::textView("lls is shutting down."));
	default:
		return ::lls::eventMakeResult(response, ::lls::LLS_RESULT_Invalid_request, ::textView("Unknown lls command."));
	}
}

sttc	::llc::err_t	lls_entry_point		(::llc::SRuntimeValues & runtimeValues) {
	(void)runtimeValues;
	::lls::pipe_t			serverPipe		= 0;
	llc_necs(::lls::pipeServerCreate(serverPipe));

	const ::llc::u3_t		startedAt		= ::GetTickCount64();
	::lls::SServiceStatus	status			= {};
	status.ProcessId						= ::GetCurrentProcessId();
	info_printf("lls protocol %u listening at %s (pid %u).", status.ProtocolVersion, ::lls::PIPE_NAME, status.ProcessId);

	bool					stopRequested	= false;
	while(false == stopRequested) {
		const ::llc::err_t	waitResult		= ::lls::pipeServerWait(serverPipe);
		if(::llc::failed(waitResult)) {
			error_printf("Failed while waiting for an lls client.");
			::lls::pipeClose(serverPipe);
			return waitResult;
		}

		::llc::SEventSystem	request			= {};
		::llc::SEventSystem	response		= {};
		const ::llc::err_t	readResult		= ::lls::pipeReadEvent(serverPipe, request);
		if(::llc::failed(readResult)) {
			error_printf("Failed to read an lls request.");
			::lls::pipeServerDisconnect(serverPipe);
			continue;
		}

		++status.RequestCount;
		status.UptimeMilliseconds			= ::GetTickCount64() - startedAt;
		llc_necs(::llsProcessRequest(request, response, status, stopRequested));
		const ::llc::err_t	writeResult		= ::lls::pipeWriteEvent(serverPipe, response);
		if(::llc::failed(writeResult))
			error_printf("Failed to write an lls response.");
		::lls::pipeServerDisconnect(serverPipe);
	}

	::lls::pipeClose(serverPipe);
	info_printf("lls stopped after %llu requests.", status.RequestCount);
	return 0;
}
