#include "lls-l.h"

#include "llc_runtime.h"

#include <cstdio>
#include <cstring>

sttc	::llc::err_t	lls_t_entry_point		(::llc::SRuntimeValues & runtimeValues);
LLC_SYSTEM_OS_ENTRY_POINT(::lls_t_entry_point);

LLC_USING_TYPEINT();
LLC_USING_APOD();
LLC_USING_VIEW();

sttc	::llc::vcu0_t	textView			(const ::llc::vcst_t & text) {
	return {(const uint8_t*)text.begin(), text.size()};
}

sttc	::llc::err_t	printHelp			(const ::llc::SCommandLineArgs & args) {
	::printf
		( "Usage: %s <command>\n"
		  "\n"
		  "Commands:\n"
		  "  ping [text]  Check that lls is reachable and optionally echo text.\n"
		  "  status       Show protocol, process, uptime, and request count.\n"
		  "  shutdown     Ask lls to stop cleanly.\n"
		, args.ProgramName.begin()
		);
	return 0;
}

sttc	::llc::err_t	printText			(::llc::vcu0_t text) {
	if(text.size())
		::printf("%.*s", (int)text.size(), (const char*)text.begin());
	::printf("\n");
	return 0;
}

sttc	::llc::err_t	printResponse		(const ::llc::SEventSystem & response) {
	::lls::SEViewResult	result		= {};
	llc_necs(::lls::eventExtractResult(response, result));
	switch(result.Type) {
	case ::lls::LLS_RESULT_Pong:
		::printf("pong");
		if(result.Data.size()) {
			::printf(": ");
			return ::printText(result.Data);
		}
		::printf("\n");
		return 0;
	case ::lls::LLS_RESULT_Status:
		if_true_ve(::llc::OS_INVALID_PARAMETER, result.Data.size() != sizeof(::lls::SServiceStatus));
		{
			::lls::SServiceStatus	status	= {};
			::memcpy(&status, result.Data.begin(), sizeof(status));
			::printf
				( "protocol: %u\n"
				  "process:  %u\n"
				  "uptime:   %llu ms\n"
				  "requests: %llu\n"
				, status.ProtocolVersion
				, status.ProcessId
				, status.UptimeMilliseconds
				, status.RequestCount
				);
		}
		return 0;
	case ::lls::LLS_RESULT_Shutting_down:
		return ::printText(result.Data);
	case ::lls::LLS_RESULT_Ok:
		return ::printText(result.Data);
	case ::lls::LLS_RESULT_Invalid_request:
	case ::lls::LLS_RESULT_Error:
		if(result.Data.size())
			::fprintf(stderr, "lls: %.*s\n", (int)result.Data.size(), (const char*)result.Data.begin());
		else
			::fprintf(stderr, "lls: request failed.\n");
		return ::llc::OS_ERROR;
	default:
		::fprintf(stderr, "lls: unknown result type %u.\n", (unsigned)result.Type);
		return ::llc::OS_INVALID_PARAMETER;
	}
}

sttc	::llc::err_t	lls_t_entry_point		(::llc::SRuntimeValues & runtimeValues) {
	const ::llc::SCommandLineArgs	& args		= runtimeValues.EntryPointArgs;
	if(0 <= ::llc::argsOptionIndex(args, "help") || 0 == args.Positionals.size())
		return ::printHelp(args);

	const ::llc::vcst_t		commandName	= args.Positionals[0];
	::lls::LLS_COMMAND		command		= {};
	::llc::vcu0_t			payload		= {};
	if(commandName == LLC_CXS("ping")) {
		command			= ::lls::LLS_COMMAND_Ping;
		if(args.Positionals.size() > 1)
			payload		= ::textView(args.Positionals[1]);
	}
	else if(commandName == LLC_CXS("status"))
		command			= ::lls::LLS_COMMAND_Status;
	else if(commandName == LLC_CXS("shutdown"))
		command			= ::lls::LLS_COMMAND_Shutdown;
	else {
		::fprintf(stderr, "Unknown command: %.*s\n\n", (int)commandName.size(), commandName.begin());
		::printHelp(args);
		return ::llc::OS_INVALID_PARAMETER;
	}

	::llc::SEventSystem	request		= {};
	::llc::SEventSystem	response	= {};
	llc_necs(::lls::eventMakeCommand(request, command, payload));
	const ::llc::err_t	requestResult	= ::lls::pipeRequest(request, response);
	if(::llc::failed(requestResult)) {
		::fprintf(stderr, "Unable to contact lls at %s (error %i). Start lls and try again.\n", ::lls::PIPE_NAME, requestResult);
		return requestResult;
	}
	return ::printResponse(response);
}
