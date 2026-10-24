#include "llc_runtime.h"
#include "lcp_help.h"
#include "lcp_tools.h"
#include "lcp_vs.h"
#include "lcp_service.h"
#include "lcp_commands.h"
#include "lcp_relay.h"
#include "lcp_text.h"
#include "lcp_metrics.h"
#include "llc_process.h"
#include "llc_path.h"
#include "llc_file.h"
#include "llc_cstdio.h"
#include "llc_chrono.h"
#include "llc_json_expression.h"
#include "llc_string_compose.h"
#include "llc_encoding.h"
#include "llc_base64.h"

#ifdef LLC_WINDOWS
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#endif

sttc ::llc::err_t lcp_entry_point(::llc::SRuntimeValues & runtimeValues);
LLC_OS_ENTRY_POINT(::lcp_entry_point);

sttc	::llc::err_t	lcpWrite			(::llc::vcst_t message, FILE * stream = stdout) {
	if_zero_vw(0, message.size());
	if_true_ve(::llc::OS_ERROR, message.size() != fwrite(message.begin(), 1, message.size(), stream));
	if_true_ve(::llc::OS_ERROR, fflush(stream));
	if(stream == stdout) { if_fail_fe(lcpMetricsRecord(COMMUNICATION_CHANNEL::Stdio, false, message)); }
	rtrn message.size();
}

sttc	::llc::err_t	lcpRespond			(cnst ::llc::SJSONReader & reader, ::llc::err_t id, ::llc::vcst_t body) {
	::llc::string response = "{\"jsonrpc\":\"2.0\",\"id\":";
	if_fail_fe(::llc::jsonWrite(reader.Tree[(::llc::u2_t)id], reader.View, response));
	if_fail_fe(::llc::append_strings(response, ',', body, "}\n"));
	rtrn ::lcpWrite(response);
}

sttc	::llc::err_t	lcpText				(cnst ::llc::SJSONReader & reader, ::llc::err_t id, ::llc::vcst_t text, bool isError) {
	llc::string body;
	if_fail_fe(lcpTextBody(body, text, isError));
	rtrn ::lcpRespond(reader, id, body);
}

sttc	::llc::err_t	lcpToolCall			(cnst ::llc::SJSONReader & reader, ::llc::err_t id, ::llc::SPathTree & tree) {
	::llc::vcst_t name = {};
	if_fail_fe(::llc::jsonExpressionResolve("params.name", reader, 0, name));
	if("help" == name) {
		llc::vcst_t path;
		cnst llc::err_t field = llc::jsonExpressionResolve("params.arguments.path", reader, 0);
		if(field >= 0 && llc::failed(llc::jsonObjectGetString(reader, (llc::u2_t)field, path)))
			rtrn lcpText(reader, id, "Help path must be a string.", true);
		llc::string text;
		if_fail_fe(lcpToolsAvailable(path, text));
		rtrn lcpText(reader, id, text, false);
	}
	if("tree" == name) {
		llc::string body;
		if_fail_fe(lcpTreeCall(reader, body, tree));
		rtrn lcpRespond(reader, id, body);
	}
	if("lls" == name) {
		llc::string body;
		if_fail_fe(lcpServiceCall(reader, body));
		rtrn lcpRespond(reader, id, body);
	}
	if(name == "cmd" || name == "powershell" || name == "wsl") {
		llc::string body;
		if_fail_fe(lcpShellCall(reader, body, name));
		rtrn lcpRespond(reader, id, body);
	}
	if(name == "status" || name == "session" || name == "failures" || name == "inbox") rtrn lcpText(reader, id, "Use --service=Debug or --service=Release for service status and inboxes.", true);
	if("vs" != name)
		rtrn ::lcpRespond(reader, id, "\"error\":{\"code\":-32602,\"message\":\"Unknown tool. Call help for available commands.\"}");
	llc::string body;
	if_fail_fe(lcpVSCall(reader, body));
	rtrn lcpRespond(reader, id, body);
}

sttc	::llc::err_t	lcpRequest			(::llc::SJSONReader & reader, ::llc::vcst_t request, ::llc::SPathTree & tree, SRelayPeer * relay, bool debug) {
	if_fail_fe(reader.Reset());
	if(::llc::failed(::llc::jsonParse(reader, request, ::llc::JSON_PARSE_STRICT)))
		rtrn ::lcpWrite("{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32700,\"message\":\"Parse error\"}}\n");
	cnst ::llc::err_t id = ::llc::jsonObjectValueGet(reader, 0, "id");
	if(0 > id) rtrn 0;
	::llc::vcst_t method = {};
	if_fail_fe(::llc::jsonExpressionResolve("method", reader, 0, method));
	if("server/discover" == method)
		rtrn ::lcpRespond(reader, id, "\"result\":{\"resultType\":\"complete\",\"supportedVersions\":[\"2026-07-28\"],\"capabilities\":{\"tools\":{}},\"_meta\":{\"io.modelcontextprotocol/serverInfo\":{\"name\":\"lcp\",\"version\":\"0.1.0\"}},\"instructions\":\"Use help path vs for build, launch, list, ping, close and restart. Use forward slashes in paths.\",\"ttlMs\":3600000,\"cacheScope\":\"public\"}");
	if("initialize" == method)
		rtrn ::lcpRespond(reader, id, "\"result\":{\"protocolVersion\":\"2025-11-25\",\"capabilities\":{\"tools\":{}},\"serverInfo\":{\"name\":\"lcp\",\"version\":\"0.1.0\"},\"instructions\":\"Use vs list to select an IDE instance. Launch requires solution, project and configuration. Supply MSBuild for builds or devenv for a new IDE.\"}");
	llc::vcst_t name, path;
	if(method == "tools/call") {
		if_fail_fe(llc::jsonExpressionResolve("params.name", reader, 0, name));
		llc::jsonExpressionResolve("params.arguments.path", reader, 0, path);
	}
	cnst bool local = name == "lls" || (name == "help" && (path == "lls" || path.find("lls ") == 0));
	if(relay && !local && (method == "tools/call" || method == "ping")) {
		llc::string response;
		if(llc::failed(relayCall(*relay, debug, request, response))) {
			if_fail_e(relayClose(*relay));
			rtrn lcpText(reader, id, "LLS relay unavailable or interrupted. Delivery/completion may be unknown; no command was replayed. Use lls status and inspect Results before retrying.", true);
		}
		rtrn lcpWrite(response);
	}
	if("ping" == method)
		rtrn ::lcpRespond(reader, id, "\"result\":{\"resultType\":\"complete\"}");
	if("tools/list" == method)
		rtrn ::lcpRespond(reader, id, lcpTools());
	if("tools/call" == method) {
		rtrn ::lcpToolCall(reader, id, tree);
	}
	rtrn ::lcpRespond(reader, id, "\"error\":{\"code\":-32601,\"message\":\"Method not found\"}");
}

sttc ::llc::err_t lcp_entry_point(::llc::SRuntimeValues & runtimeValues) {
	llc::vcst_t service;
	llc::argsOptionValue(runtimeValues.EntryPointArgs, "service", service);
	if_true_fe(service.size() && service != "Debug" && service != "Release");
	SRelayPeer peer;
	SRelayPeer * relay = service.size() ? &peer : nullptr;
	if(relay) { if_fail_fe(llc::tcpipInitialize()); }
	if_fail_fe(::llc::setupDefaultLogCallbacks());
#ifdef _DEBUG
	if_fail_fe(lcpVSBuildSummaryTest());
#endif
	SCommunicationMetrics metrics;
	if_fail_fe(lcpMetricsStart(metrics, "lcp", llc::timeCurrentInUs()));
	if_true_fe(setvbuf(stdin, 0, _IONBF, 0));
	if_fail_fe(lcpToolsLoad("lcp_tools.json"));
	if_fail_fe(helpLoad());
	::llc::SJSONReader reader;
	::llc::SPathTree tree;
	::llc::string request;
	for(int character; EOF != (character = fgetc(stdin));) {
		if('\n' != character) {
			if_fail_fe(request.push_back((::llc::sc_t)character));
			continue;
		}
		if_fail_fe(lcpMetricsRecord(COMMUNICATION_CHANNEL::Stdio, true, request, request.size() + 1));
		if_fail_fe(::lcpRequest(reader, request, tree, relay, service == "Debug"));
		if_fail_fe(request.clear());
	}
	if_true_ve(::llc::OS_ERROR, ferror(stdin));
	if(request.size()) {
		if_fail_fe(lcpMetricsRecord(COMMUNICATION_CHANNEL::Stdio, true, request));
		if_fail_fe(::lcpRequest(reader, request, tree, relay, service == "Debug"));
	}
	if(relay) { if_fail_fe(relayClose(peer)); if_fail_fe(llc::tcpipShutdown()); }
	rtrn lcpMetricsStop(metrics);
}
