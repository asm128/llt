#include "llc_cstdio.h"
#include "llc_json_expression.h"
#include "llc_process.h"
#include "llc_runtime.h"
#include "llc_string_compose.h"

sttc	llc::err_t	lls_entry_point		(llc::SRuntimeValues & runtimeValues);
LLC_OS_ENTRY_POINT(::lls_entry_point);

sttc	::llc::err_t	llsWrite			(::llc::vcst_t message, FILE * stream = stdout) {
	if_zero_vw(0, message.size());
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

sttc	::llc::err_t	llsRequest			(::llc::SJSONReader & reader, ::llc::vcst_t request) {
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
	if("tools/call" == method)
		rtrn ::llsToolCall(reader, id);
	rtrn ::llsRespond(reader, id, "\"error\":{\"code\":-32601,\"message\":\"Method not found\"}");
}

sttc	llc::err_t	lls_entry_point		(llc::SRuntimeValues & runtimeValues) {
	(void)runtimeValues;
	if_fail_fe(::llc::setupDefaultLogCallbacks());
	::llc::SJSONReader reader = {};
	::llc::string request = {};
	for(int character; EOF != (character = fgetc(stdin));) {
		if('\n' != character) {
			if_fail_fe(request.push_back((::llc::sc_t)character));
			continue;
		}
		if_fail_fe(::llsRequest(reader, request));
		request.clear();
	}
	if_true_ve(::llc::OS_ERROR, ferror(stdin));
	rtrn request.size() ? ::llsRequest(reader, request) : 0;
}
