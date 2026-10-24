#include "llc_app_impl.h"
#include "dashboard_app.h"
#include "../lcp/lcp_help.h"
#include "../lcp/lcp_tools.h"
#include "../lcp/lcp_identity.h"
#include "../lcp/lcp_text.h"
#include "llc_json_expression.h"
#include "llc_chrono.h"
#include "llc_process.h"

#define lls_dashboard_info_printf(...) do {} while(0) // info_printf

#if defined(LLC_WINDOWS) && !defined(LLC_AVOID_LOCAL_APPLICATION_MODEL_EXECUTABLE_RUNTIME)
#include <process.h>
LLC_DEFINE_APPLICATION_ENTRY_POINT_MT(SApplication, "LLS dashboard");
#else
LLC_DEFINE_APPLICATION_ENTRY_POINT(SApplication, "LLS dashboard");
#endif

sttc llc::err_t dashboardString(cnst llc::SJSONReader & reader, llc::u2_t node, llc::vcst_t key, llc::string & output) {
	llc::vcst_t value;
	llc::string decoded;
	if_fail_fe(llc::jsonObjectGetAsString(reader, node, key, value));
	if_fail_fe(lcpStringDecode(value, decoded));
	rtrn output.append(value);
}

sttc llc::err_t dashboardSnapshot(SApplication & app, llc::vcst_t response) {
	llc::SJSONReader reader;
	if_fail_fe(llc::jsonParse(reader, response, llc::JSON_PARSE_STRICT));
	cnst llc::err_t root = llc::jsonExpressionResolve("result.structuredContent", reader, 0);
	if_fail_fe(root);
	llc::u3_t session = 0, elapsed = 0;
	llc::u2_t revision = 0, requests = 0;
	bool changed = false;
	llc::s2_t logError = 0, historyError = 0;
	if_fail_fe(llc::jsonObjectGetAsInteger(reader, root, "session", session));
	if_fail_fe(llc::jsonObjectGetAsInteger(reader, root, "elapsedMs", elapsed));
	if_fail_fe(llc::jsonObjectGetAsInteger(reader, root, "revision", revision));
	if_fail_fe(llc::jsonObjectGetAsInteger(reader, root, "requests", requests));
	if_fail_fe(llc::jsonObjectGetAsBoolean(reader, root, "changed", changed));
	if_fail_fe(llc::jsonObjectGetAsInteger(reader, root, "logError", logError));
	if_fail_fe(llc::jsonObjectGetAsInteger(reader, root, "historyError", historyError));
	llc::aobj<SDashboardJob> jobs;
	if(changed) {
		cnst llc::err_t array = llc::jsonObjectValueGet(reader, root, "jobs");
		if_fail_fe(array);
		cnst llc::err_t count = llc::jsonArraySize(reader, array);
		if_fail_fe(count);
		if_fail_fe(jobs.resize((llc::u2_t)count));
		for(llc::u2_t i = 0; i < jobs.size(); ++i) {
			cnst llc::err_t node = llc::jsonArrayValueGet(reader, array, i);
			if_fail_fe(node);
			auto & job = jobs[i];
			if_fail_fe(dashboardString(reader, node, "id", job.Id));
			if_fail_fe(dashboardString(reader, node, "state", job.State));
			if_fail_fe(dashboardString(reader, node, "summary", job.Summary));
			if_fail_fe(llc::jsonObjectGetAsInteger(reader, node, "pid", job.PID));
			if_fail_fe(llc::jsonObjectGetAsInteger(reader, node, "elapsedMs", job.ElapsedMs));
			if_fail_fe(llc::jsonObjectGetAsInteger(reader, node, "session", job.OriginSession));
			if_fail_fe(llc::jsonObjectGetAsBoolean(reader, node, "historical", job.Historical));
			if_fail_fe(llc::jsonObjectGetAsInteger(reader, node, "status", job.Result.Status));
			if_fail_fe(llc::jsonObjectGetAsInteger(reader, node, "exitCode", job.Result.ExitCode));
			if_fail_fe(llc::jsonObjectGetAsBoolean(reader, node, "exitKnown", job.Result.ExitKnown));
			if_fail_fe(llc::jsonObjectGetAsBoolean(reader, node, "started", job.ProcessStarted));
			if_fail_fe(llc::jsonObjectGetAsBoolean(reader, node, "finished", job.ProcessFinished));
			if_fail_fe(llc::jsonObjectGetAsBoolean(reader, node, "detached", job.Detached));
		}
	}
	llc::lock_guard guard(app.Lock);
	if(changed) app.Jobs = std::move(jobs);
	app.LogError = logError;
	app.HistoryError = historyError;
	app.SessionId = session;
	app.ServiceRevision = revision;
	app.Requests = requests;
	app.ElapsedMs = elapsed;
	app.Connected = true;
	app.Connection = llc::vcst_t{"ONLINE"};
	app.LastSnapshot = llc::timeCurrentInUs();
	++app.Revision;
	rtrn 0;
}

sttc llc::err_t dashboardTemplateSanitize(llc::string & text) {
	llc::u2_t write = 0;
	for(llc::u2_t read = 0; read < text.size(); ++read) {
		if(text[read] == '\\' && read + 1 < text.size() && (text[read + 1] == '{' || text[read + 1] == '}')) continue;
		text[write++] = text[read];
	}
	rtrn text.resize(write);
}

sttc llc::err_t dashboardCommandRequest(cnst llc::SJSONReader & layout, llc::vcst_t command, llc::string & request) {
	llc::SJSONReader input;
	if_fail_fe(llc::jsonParse(input, command, llc::JSON_PARSE_STRICT));
	if_true_fe(!input.Token.size() || input.Token[0].Type != llc::JSON_TYPE_OBJECT);
	if(llc::jsonObjectValueGet(input, 0, "method") >= 0) {
		llc::vcst_t method, name;
		llc::string decodedMethod;
		if_fail_fe(llc::jsonObjectGetAsString(input, 0, "method", method));
		if_fail_fe(lcpStringDecode(method, decodedMethod));
		if_true_fe(method != "tools/call");
		if_fail_fe(llc::jsonExpressionResolve("params.name", input, 0, name));
		cnst llc::err_t argumentNode = llc::jsonExpressionResolve("params.arguments", input, 0);
		if_fail_fe(argumentNode);
		if_true_fe(input.Token[(llc::u2_t)argumentNode].Type != llc::JSON_TYPE_OBJECT);
		if_true_fe(!lcpToolTitle(name).size());
		rtrn request.append(command);
	}
	llc::vcst_t name, format;
	if_fail_fe(llc::jsonExpressionResolve("name", input, 0, name));
	cnst llc::err_t argumentNode = llc::jsonExpressionResolve("arguments", input, 0);
	if_fail_fe(argumentNode);
	if_true_fe(input.Token[(llc::u2_t)argumentNode].Type != llc::JSON_TYPE_OBJECT);
	if_true_fe(!lcpToolTitle(name).size());
	if_fail_fe(llc::jsonExpressionResolve("mcp.request", layout, 0, format));
	llc::string decodedFormat;
	if_fail_fe(lcpStringDecode(format, decodedFormat));
	if_fail_fe(llc::jsonStringFormat(format, input, 0, request));
	if_fail_fe(dashboardTemplateSanitize(request));
	llc::SJSONReader check;
	rtrn llc::jsonParse(check, request, llc::JSON_PARSE_STRICT);
}

llc::err_t llsCommandQueue(SApplication & app, llc::vcst_t command) {
	llc::string request;
	if_fail_fe(dashboardCommandRequest((*app.UI).Layout.Reader, command, request));
	llc::lock_guard guard(app.Lock);
	if_true_fe(app.CommandPending || app.CommandBusy);
	if_fail_fe(app.CommandRequest.clear());
	if_fail_fe(app.CommandRequest.append(request));
	if_fail_fe(app.CommandResponse.clear());
	if_fail_fe(app.CommandResponse.append_string("Sending..."));
	app.CommandPending = true;
	++app.Revision;
	rtrn 0;
}

sttc llc::err_t dashboardCommandResult(llc::err_t called, llc::vcst_t response, llc::string & output) {
	if(llc::failed(called)) rtrn output.append_string("Command transport failed.");
	llc::SJSONReader reader;
	if_fail_fe(llc::jsonParse(reader, response, llc::JSON_PARSE_STRICT));
	rtrn output.append(response);
}

#ifdef _DEBUG
sttc llc::err_t dashboardCommandTest(SApplication & app) {
	llc::string request, fullRequest, response;
	cnst llc::vcst_t responseJSON = "{\"jsonrpc\":\"2.0\",\"id\":2,\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"pong\"}]}}";
	if_fail_fe(dashboardCommandRequest((*app.UI).Layout.Reader, "{\"name\":\"help\",\"arguments\":{\"path\":\"\"}}", request));
	if_true_fe(request != "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"help\",\"arguments\":{\"path\":\"\"}}}");
	if_fail_fe(dashboardCommandRequest((*app.UI).Layout.Reader, request, fullRequest));
	if_true_fe(fullRequest != request);
	if_fail_fe(dashboardCommandResult(0, responseJSON, response));
	if_true_fe(response != responseJSON);
	for(llc::vcst_t command : (*app.UI).CommandTemplateJSON) {
		if_fail_fe(request.clear());
		if_fail_fe(dashboardCommandRequest((*app.UI).Layout.Reader, command, request));
	}
	rtrn 0;
}
#endif

llc::err_t setup(SApplication & app) {
#if defined(LLC_WINDOWS) && defined(_DEBUG)
	// Match LRT: track allocations/leaks without walking the entire heap on every allocation.
	_CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF | _CRTDBG_ALLOC_MEM_DF);
#endif
	if_fail_fe(llc::setupDefaultLogCallbacks());
	cnst llc::err_t instance = llsDesktopStart();
	if_fail_fe(instance);
	if(instance) { app.Secondary = true; rtrn 0; }
#ifdef _DEBUG
	app.Service = llc::vcst_t{"Debug"};
#else
	app.Service = llc::vcst_t{"Release"};
#endif
	llc::vcst_t service;
	llc::argsOptionValue(app.Runtime.EntryPointArgs, "service", service);
	if_true_fe(service.size() && service != "Debug" && service != "Release");
	if(service.size()) app.Service = service;
	if_fail_fe(lcpIdentity(app.Identity));
	cnst llc::vcst_t identity = app.Identity;
	cnst llc::err_t end = identity.find('\n');
	if_fail_fe(llc::append_strings(app.Title, "LLS Dashboard | ", llc::vcst_t{identity.begin(), end >= 0 ? (llc::u2_t)end : identity.size()}, " | Service: ", app.Service));
	if_fail_fe(helpLoad());
	if_fail_fe(lcpToolsLoad("lls_tools.json"));
	if_fail_fe(llc::tcpipInitialize());
	app.NetworkStarted = true;
	if_fail_fe(llsGUISetup(app));
#ifdef _DEBUG
	if_fail_fe(dashboardCommandTest(app));
#endif
	if_fail_fe(llsDesktopAttach(app));
	rtrn 0;
}

llc::err_t cleanup(SApplication & app) {
	if_fail_e(relayClose(app.Peer));
	if(app.NetworkStarted) { if_fail_e(llc::tcpipShutdown()); }
	if_fail_e(llsDesktopClose(app));
	if(app.UI) { if_fail_e(llc::mainWindowDestroy((*app.UI).Framework.RootWindow)); }
#if defined(LLC_WINDOWS) && !defined(LLC_AVOID_LOCAL_APPLICATION_MODEL_EXECUTABLE_RUNTIME)
	if(app.Restart) {
		char executable[32768] = {};
		cnst DWORD length = GetModuleFileNameA(0, executable, (DWORD)llc::size(executable));
		if_true_fe(!length || length >= llc::size(executable));
		llc::string command = llc::vcst_t{GetCommandLineA(), (llc::u2_t)-1};
		llc::SProcessManager<> processes;
		cnst llc::err_t process = processes.Create({executable, length}, command, {}, CREATE_NO_WINDOW);
		if_fail_fe(process);
		if_fail_fe(processes.Launch((llc::u2_t)process));
		if_fail_fe(processes.Close((llc::u2_t)process));
	}
#endif
	rtrn 0;
}

llc::err_t update(SApplication & app, bool systemRequestedExit) {
	lls_dashboard_info_printf("dashboard update: enter");
	if(app.Secondary) {
		error_printf("Dashboard stopped: another instance owns the Debug dashboard.");
		rtrn llc::APPLICATION_STATE_EXIT;
	}
	if(systemRequestedExit) app.Closing = true;
	if(app.Closing) {
		error_printf("Dashboard stopping%s.", systemRequestedExit ? ": system exit requested" : "");
#ifdef LLC_AVOID_LOCAL_APPLICATION_MODEL_EXECUTABLE_RUNTIME
		rtrn app.Restart ? llc::APPLICATION_STATE_RESTART : llc::APPLICATION_STATE_EXIT;
#else
		rtrn llc::APPLICATION_STATE_EXIT;
#endif
	}
	if_fail_fe(llsDesktopUpdate(app));
	lls_dashboard_info_printf("dashboard update: desktop");
	cnst llc::err_t result = llsGUIUpdate(app);
	lls_dashboard_info_printf("dashboard update: gui=%i", result);
	if(result) {
		if(llc::failed(result)) error_printf("Dashboard GUI failed: %i.", result);
		app.Closing = true;
	}
#ifdef LLC_AVOID_LOCAL_APPLICATION_MODEL_EXECUTABLE_RUNTIME
	if(app.Restart) rtrn llc::APPLICATION_STATE_RESTART;
#endif
	rtrn result;
}

llc::err_t draw(SApplication & app) {
	if(app.Closing || app.Secondary) rtrn 0;
	llc::string command;
	{
		llc::lock_guard guard(app.Lock);
		if(app.CommandPending) {
			if_fail_fe(command.append(app.CommandRequest));
			app.CommandPending = false;
			app.CommandBusy = true;
		}
	}
	if(command.size()) {
		llc::string response, result;
		cnst llc::err_t called = relayCall(app.Peer, app.Service == "Debug", command, response, 1800000);
		cnst llc::err_t parsed = dashboardCommandResult(called, response, result);
		if(llc::failed(called)) { if_fail_e(relayClose(app.Peer)); }
		if(llc::failed(parsed) && !result.size()) { if_fail_fe(result.append_string("Invalid command response.")); }
		{
			llc::lock_guard guard(app.Lock);
			if_fail_fe(app.CommandResponse.clear());
			if_fail_fe(app.CommandResponse.append(result));
			app.CommandBusy = false;
			++app.Revision;
		}
		app.NextPoll = 0;
		rtrn 0;
	}
	if((llc::u3_t)llc::timeCurrentInUs() < app.NextPoll) { llc::sleep(10); rtrn 0; }
	llc::string request, response;
	if_fail_fe(llc::append_strings(request, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"session\",\"arguments\":{\"session\":",
		llc::str(app.SessionId), ",\"revision\":", llc::str(app.ServiceRevision), "}}}"));
	cnst llc::err_t called = relayCall(app.Peer, app.Service == "Debug", request, response, 3000);
	cnst llc::err_t parsed = llc::succeeded(called) ? dashboardSnapshot(app, response) : called;
	if(llc::failed(parsed)) {
		if_fail_e(relayClose(app.Peer));
		llc::lock_guard guard(app.Lock);
		app.Connected = false;
		app.Connection = llc::failed(called) ? "OFFLINE - last snapshot is stale" : "INVALID SNAPSHOT - last data is stale";
		++app.Revision;
	}
	app.NextPoll = llc::timeCurrentInUs() + 1000000;
	rtrn 0;
}
