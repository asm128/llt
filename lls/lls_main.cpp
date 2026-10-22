#include "llc_app_impl.h"
#include "llc_cstdio.h"
#include "llc_json_expression.h"
#include "llc_mutex.h"
#include "llc_string_compose.h"

stct SApplication {
	::llc::SJSONReader		Reader;
	::llc::string			Request;
	::llc::aobj<::llc::string> Pending;
	::llc::mutex			Lock;
	::llc::err_t			WorkerResult	= {};
	bool					Started			= {}, InputClosed = {}, Busy = {};
	explicit SApplication(::llc::SRuntimeValues &) {}
};
LLC_DEFINE_APPLICATION_ENTRY_POINT(SApplication, "LT4 Limb System");

sttc	::llc::err_t	llsWrite			(::llc::vcst_t message, FILE * stream = stdout) {
	if_zero_vw(0, message.size());
	sttc ::llc::mutex lock;
	::llc::lock_guard guard(lock);
	if_true_ve(::llc::OS_ERROR, message.size() != fwrite(message.begin(), 1, message.size(), stream));
	if_true_ve(::llc::OS_ERROR, fflush(stream));
	rtrn message.size();
}

sttc	::llc::err_t	llsRespond			(cnst ::llc::SJSONReader & reader, ::llc::err_t id, ::llc::vcst_t body) {
	::llc::string response = "{\"jsonrpc\":\"2.0\",\"id\":";
	if_fail_fe(::llc::jsonWrite(reader.Tree[(::llc::u2_t)id], reader.View, response));
	if_fail_fe(::llc::append_strings(response, ',', body, "}\n"));
	rtrn ::llsWrite(response);
}

sttc	::llc::err_t	llsToolCall			(cnst ::llc::SJSONReader & reader, ::llc::err_t id) {
	::llc::vcst_t action = {}, msbuild = {};
	if_fail_fe(::llc::jsonExpressionResolve("params.arguments.action", reader, 0, action));
	cnst bool build = "build" == action;
	::llc::string command = {}, result = {};
	if_fail_fe(::llc::jsonStringFormat(build
		? ::llc::vcst_t{"MSBuild.exe \"{params.arguments.solution}\" /t:\"{params.arguments.project}\""}
		: ::llc::vcst_t{"devenv.exe /Run \"{params.arguments.solution}\""}, reader, 0, command));
	if(build) {
		if_fail_fe(::llc::jsonExpressionResolve("params.arguments.msbuild", reader, 0, msbuild));
		::llc::vcst_t configuration = {}, platform = {};
		if_fail_fe(::llc::jsonExpressionResolve("params.arguments.configuration", reader, 0, configuration));
		if_fail_fe(::llc::split<::llc::sc_c>('|', configuration, configuration, platform));
		if_fail_fe(::llc::append_strings(command, " /p:Configuration=\"", configuration, "\" /p:Platform=\"", platform, '"'));
	}
	::llc::au0_t output = {};
	::llc::u2_t exitCode = 0;
	cnst ::llc::err_t status = build
		? ::llc::SProcessManager{}.Execute(msbuild, command, {}, output, exitCode, {})
		: ::llc::SProcessManager{}.Launch({}, command, {});
	if_fail_fe(::llsWrite({(cnst ::llc::sc_t*)output.begin(), output.size()}, stderr));
	if_fail_fe(::llc::append_strings(result, "\"result\":{\"resultType\":\"complete\",\"content\":[{\"type\":\"text\",\"text\":\"Visual Studio ", action, ": status ", ::llc::str(status), ", exit code ", ::llc::str(exitCode), ". Output is on stderr.\"}],\"isError\":", ::llc::b82vcs(::llc::failed(status) || 0 != exitCode), '}'));
	rtrn ::llsRespond(reader, id, result);
}

sttc	::llc::err_t	llsRequest			(::llc::SJSONReader & reader, ::llc::vcst_t request, SApplication * app = 0) {
	if_fail_fe(reader.Reset());
	if(::llc::failed(::llc::jsonParse(reader, request, ::llc::JSON_PARSE_STRICT)))
		rtrn ::llsWrite("{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32700,\"message\":\"Parse error\"}}\n");
	cnst ::llc::err_t id = ::llc::jsonObjectValueGet(reader, 0, "id");
	if(0 > id) rtrn 0;
	::llc::vcst_t method = {};
	if_fail_fe(::llc::jsonExpressionResolve("method", reader, 0, method));
	if("server/discover" == method)
		rtrn ::llsRespond(reader, id, "\"result\":{\"resultType\":\"complete\",\"supportedVersions\":[\"2026-07-28\"],\"capabilities\":{\"tools\":{}},\"_meta\":{\"io.modelcontextprotocol/serverInfo\":{\"name\":\"lls\",\"version\":\"0.1.0\"}},\"instructions\":\"Build a selected solution target with MSBuild or run a solution through devenv. Use forward slashes in paths.\",\"ttlMs\":3600000,\"cacheScope\":\"public\"}");
	if("initialize" == method)
		rtrn ::llsRespond(reader, id, "\"result\":{\"protocolVersion\":\"2025-11-25\",\"capabilities\":{\"tools\":{}},\"serverInfo\":{\"name\":\"lls\",\"version\":\"0.1.0\"},\"instructions\":\"Supply the MSBuild executable path for builds; running through devenv requires it on PATH.\"}");
	if("ping" == method)
		rtrn ::llsRespond(reader, id, "\"result\":{\"resultType\":\"complete\"}");
	if("tools/list" == method)
		rtrn ::llsRespond(reader, id, "\"result\":{\"resultType\":\"complete\",\"tools\":[{\"name\":\"vs\",\"title\":\"Visual Studio\",\"description\":\"Build one solution target with MSBuild or run the solution through devenv.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"action\":{\"type\":\"string\",\"enum\":[\"build\",\"run\"]},\"solution\":{\"type\":\"string\",\"description\":\"Solution path using forward slashes.\"},\"msbuild\":{\"type\":\"string\",\"description\":\"Full MSBuild executable path using forward slashes; required for build.\"},\"project\":{\"type\":\"string\",\"description\":\"MSBuild solution target name, including solution folders when applicable; required for build.\"},\"configuration\":{\"type\":\"string\",\"description\":\"Exact solution configuration such as Debug|x64; required for build.\"}},\"required\":[\"action\",\"solution\"],\"additionalProperties\":false}}],\"ttlMs\":3600000,\"cacheScope\":\"public\"}");
	if("tools/call" == method) {
		if(app) {
			::llc::lock_guard guard(app->Lock);
			rtrn app->Pending.emplace_back(request);
		}
		rtrn ::llsToolCall(reader, id);
	}
	rtrn ::llsRespond(reader, id, "\"error\":{\"code\":-32601,\"message\":\"Method not found\"}");
}

::llc::err_t setup(SApplication &) { rtrn ::llc::setupDefaultLogCallbacks(); }
::llc::err_t cleanup(SApplication & app) { rtrn app.WorkerResult; }

::llc::err_t update(SApplication & app, bool systemRequestedExit) {
	if(!app.Started) { app.Started = true; rtrn 0; } // Let lrt start draw before reading stdin.
	app.InputClosed |= systemRequestedExit;
	{
		::llc::lock_guard guard(app.Lock);
		if_fail_fe(app.WorkerResult);
		if(app.InputClosed && !app.Busy && !app.Pending.size())
			rtrn ::llc::APPLICATION_STATE_EXIT;
	}
	if(app.InputClosed) { ::llc::sleep(1); rtrn 0; }
	for(int character; EOF != (character = fgetc(stdin));) {
		if('\n' != character) {
			if_fail_fe(app.Request.push_back((::llc::sc_t)character));
			continue;
		}
		if_fail_fe(::llsRequest(app.Reader, app.Request, &app));
		rtrn app.Request.clear();
	}
	if_true_ve(::llc::OS_ERROR, ferror(stdin));
	app.InputClosed = true;
	if(app.Request.size()) {
		if_fail_fe(::llsRequest(app.Reader, app.Request, &app));
	}
	rtrn app.Request.clear();
}

::llc::err_t draw(SApplication & app) {
	::llc::string request;
	{
		::llc::lock_guard guard(app.Lock);
		if(app.Pending.size()) {
			if_fail_fe(request.append(app.Pending[0]));
			if_fail_fe(app.Pending.remove(0));
			app.Busy = true;
		}
	}
	if(!request.size()) { ::llc::sleep(1); rtrn 0; }
	::llc::SJSONReader reader;
	cnst ::llc::err_t result = ::llsRequest(reader, request);
	{
		::llc::lock_guard guard(app.Lock);
		app.WorkerResult = result;
		app.Busy = false;
	}
	rtrn result;
}
