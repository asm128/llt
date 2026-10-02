#include "lls-l.h"

#include "llc_runtime.h"
#include "llc_view_serialize.h"

#include <cstdio>

stct SLLSTerminal;

using FLLSTerminalCommand	= llc::FError<SLLSTerminal &>;
using FLLSTerminalResult	= llc::FError<const ::lls::SEViewResult &>;

stct SLLSTerminalCommand {
	::llc::vcst_t				Arguments			= {};
	::llc::u0_t					ArgumentCountMin	= 0;
	::llc::u0_t					ArgumentCountMax	= 0;
	FLLSTerminalCommand			Prepare				= {};
};

stct SLLSTerminal {
	::llc::SCommandLineArgs		CommandLineArgs		= {};
	::lls::LLS_COMMAND			Command				= ::lls::LLS_COMMAND_MAX;
	::llc::vcu0_t				Payload				= {};
	::llc::SEventSystem			Request				= {};
	::llc::SEventSystem			Response			= {};
};

sttc	::llc::err_t	llsTEntryPoint			(::llc::SRuntimeValues & runtimeValues);
sttc	::llc::err_t	commandPreparePing		(SLLSTerminal & appState);
sttc	::llc::err_t	resultHandleText		(const ::lls::SEViewResult & result);
stin	::llc::err_t	resultHandlePong		(const ::lls::SEViewResult & result) { always_printf("pong"); return resultHandleText(result); }
sttc	::llc::err_t	resultHandleStatus		(const ::lls::SEViewResult & result);
sttc	::llc::err_t	resultHandleFailure		(const ::lls::SEViewResult & result);

LLC_SYSTEM_OS_ENTRY_POINT(::llsTEntryPoint);

stxp SLLSTerminalCommand COMMANDS[] =
	{ {LLC_CXS("[text]")	, 0, 1, ::commandPreparePing}
	, {{}				, 0, 0, {}}
	, {{}				, 0, 0, {}}
	};

stxp FLLSTerminalResult RESULTS[] =
	{ ::resultHandleText
	, ::resultHandlePong
	, ::resultHandleStatus
	, ::resultHandleText
	, ::resultHandleFailure
	, ::resultHandleFailure
	};

static_assert(::llc::size(COMMANDS) == ::lls::LLS_COMMAND_MAX);
static_assert(::llc::size(RESULTS ) == ::lls::LLS_RESULT_MAX );

sttc	::llc::err_t	commandPreparePing		(SLLSTerminal & appState) {
	if(1 < appState.CommandLineArgs.Positionals.size())
		appState.Payload = appState.CommandLineArgs.Positionals[1].cu8();
	return 0;
}

sttc	::llc::err_t	commandSelect			(SLLSTerminal & appState) {
	const ::llc::vcst_t			commandName		= appState.CommandLineArgs.Positionals[0];
	const ::lls::LLS_COMMAND	commandType		= ::llc::get_value<::lls::LLS_COMMAND>(commandName);
	if_true_vef(::llc::OS_INVALID_PARAMETER, commandType >= ::lls::LLS_COMMAND_MAX, "Unknown lls command: %.*s. Use -help to list commands.", (int)commandName.size(), commandName.begin());

	const SLLSTerminalCommand	& command			= COMMANDS[commandType];
	const ::llc::u2_t			argumentCount		= appState.CommandLineArgs.Positionals.size() - 1;
	if_true_vef(::llc::OS_INVALID_PARAMETER, argumentCount < command.ArgumentCountMin || argumentCount > command.ArgumentCountMax, "Invalid argument count for '%s': %u supplied, expected %u..%u.", ::llc::get_value_namep(commandType), argumentCount, command.ArgumentCountMin, command.ArgumentCountMax);

	appState.Command	= commandType;
	return command.Prepare ? command.Prepare(appState) : 0;
}

sttc	::llc::err_t	displayHelp				(const SLLSTerminal & appState) {
	::printf
		( "Usage: %s <command> [arguments]\n"
		  "       %s -help\n"
		  "\n"
		  "Commands:\n"
		, appState.CommandLineArgs.ProgramName.begin()
		, appState.CommandLineArgs.ProgramName.begin()
		);
	for(::llc::u2_t iCommand = 0; iCommand < ::llc::size(COMMANDS); ++iCommand) {
		const ::lls::LLS_COMMAND		commandType		= (::lls::LLS_COMMAND)iCommand;
		const SLLSTerminalCommand	& command			= COMMANDS[iCommand];
		::printf("  %s", ::llc::get_value_namep(commandType));
		if(command.Arguments.size())
			::printf(" %.*s", (int)command.Arguments.size(), command.Arguments.begin());
		::printf("\n      %s\n", ::llc::get_value_descp(commandType));
	}
	return 0;
}

sttc	::llc::err_t	resultHandleText		(const ::lls::SEViewResult & result) {
	return (0 == result.Data.size()) ? 0 : always_printf("%.*s", (int)result.Data.size(), result.Data.begin());
}


sttc	::llc::err_t	resultHandleStatus		(const ::lls::SEViewResult & result) {
	if_true_vef(::llc::OS_INVALID_PARAMETER, szof(::lls::SServiceStatus) != result.Data.size(), "Invalid lls status payload: %u bytes received, %u expected.", result.Data.size(), (::llc::u2_t)szof(::lls::SServiceStatus));

	::llc::vcu0_t			statusBytes		= result.Data;
	::lls::SServiceStatus	status			= {};
	if_fail_fe(::llc::loadPOD(statusBytes, status));
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
	return 0;
}

sttc	::llc::err_t	resultHandleFailure		(const ::lls::SEViewResult & result) {
	::fprintf(stderr, "lls %s", ::llc::get_value_namep(result.Type));
	if(result.Data.size())
		::fprintf(stderr, ": %.*s", (int)result.Data.size(), (const char*)result.Data.begin());
	::fprintf(stderr, "\n");
	return ::llc::OS_ERROR;
}

sttc	::llc::err_t	responseHandle			(SLLSTerminal & appState) {
	::lls::SEViewResult			result				= {};
	if_fail_fef(::lls::eventExtractResult(appState.Response, result), "Unexpected response event type:%u.", (::llc::u2_t)appState.Response.Type);
	if_true_vef(::llc::OS_INVALID_PARAMETER, result.Type >= ::lls::LLS_RESULT_MAX, "Unknown lls result: %u.", (::llc::u2_t)result.Type);
	return RESULTS[result.Type](result);
}

sttc	::llc::err_t	requestExecute			(SLLSTerminal & appState) {
	if_fail_fef(::lls::eventMakeCommand(appState.Request, appState.Command, appState.Payload), "Failed to compose '%s' request.", ::llc::get_value_namep(appState.Command));
	if_fail_fef(::lls::pipeRequest(appState.Request, appState.Response), "Unable to contact lls at %s for '%s'.", ::lls::PIPE_NAME, ::llc::get_value_namep(appState.Command));
	return ::responseHandle(appState);
}

sttc	::llc::err_t	llsTEntryPoint			(::llc::SRuntimeValues & runtimeValues) {
	SLLSTerminal	appState	= {};
	appState.CommandLineArgs	= runtimeValues.EntryPointArgs;
	if(0 <= ::llc::argsOptionIndex(appState.CommandLineArgs, "help") || 0 == appState.CommandLineArgs.Positionals.size())
		return ::displayHelp(appState);

	::llc::err_t	commandSelectResult;
	if_fail_vef(commandSelectResult, commandSelectResult = ::commandSelect(appState)
		, "commandSelectResult:%i", commandSelectResult
		);
	return ::requestExecute(appState);
}
