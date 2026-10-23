#include "llc_app_impl.h"
#include "lls_app.h"
#include "lls_mail.h"
#include "llc_cstdio.h"
#include "llc_json_expression.h"
#include "llc_mutex.h"
#include "llc_string_compose.h"
#include "llc_stdstring.h"
#include "llc_timer.h"
#include "llc_chrono.h"
#include "../lcp/lcp_help.h"
#include "../lcp/lcp_tools.h"
#include "../lcp/lcp_vs.h"
#include "../lcp/lcp_commands.h"
#include "../lcp/lcp_text.h"
#include "../lcp/lcp_identity.h"

LLC_DEFINE_APPLICATION_ENTRY_POINT(SApplication, "LT4 Limb System");

sttc	::llc::err_t	llsWrite			(::llc::vcst_t message, FILE * stream = stdout) {
	if_zero_vw(0, message.size());
	sttc ::llc::mutex lock;
	::llc::lock_guard guard(lock);
	if_true_ve(::llc::OS_ERROR, message.size() != fwrite(message.begin(), 1, message.size(), stream));
	if_true_ve(::llc::OS_ERROR, fflush(stream));
	rtrn message.size();
}

sttc	::llc::err_t	llsRespond			(cnst ::llc::SJSONReader & reader, ::llc::err_t id, ::llc::vcst_t body, SRelayPeer * peer = nullptr) {
	::llc::string response = "{\"jsonrpc\":\"2.0\",\"id\":";
	if_fail_fe(::llc::jsonWrite(reader.Tree[(::llc::u2_t)id], reader.View, response));
	if_fail_fe(::llc::append_strings(response, ',', body, "}\n"));
	rtrn peer ? relayQueue(*peer, response) : llsWrite(response);
}

sttc llc::err_t llsStatus(cnst llc::SJSONReader & reader, llc::err_t id, SApplication & app, SRelayPeer * peer) {
	cnst llc::err_t field = llc::jsonExpressionResolve("params.arguments.id", reader, 0);
	if(field < 0)
		rtrn llsRespond(reader, id, "\"error\":{\"code\":-32602,\"message\":\"Provide the original build request id.\"}", peer);
	llc::string target, body;
	if_fail_fe(llc::jsonWrite(reader.Tree[(llc::u2_t)field], reader.View, target));
	{
		llc::lock_guard guard(app.Lock);
		for(cnst SBuildJob & job : app.Jobs) {
			if(job.Historical || target != job.Id || (cnst SRelayPeer*)job.Peer != peer) continue;
			cnst bool completed = job.State == "completed";
			llc::STimer timer = job.Timer;
			if(!completed) timer.Frame();
			if_fail_fe(llc::append_strings(body, "\"result\":{\"resultType\":\"complete\",\"content\":[{\"type\":\"text\",\"text\":\"", job.State,
				"\"}],\"structuredContent\":{\"id\":", job.Id, ",\"state\":\"", job.State, "\",\"elapsedMs\":", llc::str(timer.LastTimeMicroseconds / 1000),
				",\"status\":"));
			if(completed) { if_fail_fe(llc::append_strings(body, llc::str(job.Result.Status))); }
			else { if_fail_fe(body.append_string("null")); }
			if_fail_fe(body.append_string(",\"exitCode\":"));
			if(completed && job.Result.ExitKnown) { if_fail_fe(llc::append_strings(body, llc::str(job.Result.ExitCode))); }
			else { if_fail_fe(body.append_string("null")); }
			if_fail_fe(llc::append_strings(body, ",\"pid\":", llc::str(job.PID), ",\"outputBytes\":", llc::str(job.Output.size())));
			if_fail_fe(body.append_string("},\"isError\":false}"));
			break;
		}
	}
	if(0 == body.size())
		rtrn llsRespond(reader, id, "\"error\":{\"code\":-32602,\"message\":\"Unknown build request id in this server session.\"}", peer);
	rtrn llsRespond(reader, id, body, peer);
}

sttc	::llc::err_t	llsToolCall			(cnst ::llc::SJSONReader & reader, SBuildResult & outcome, SApplication & app, llc::u2_t jobIndex, SRelayPeer * peer) {
	llc::vcst_t name, action;
	if_fail_fe(llc::jsonExpressionResolve("params.name", reader, 0, name));
	if(name != "vs") {
		llc::string body;
		llc::SPathTree tree;
		if(name == "tree") { if_fail_fe(lcpTreeCall(reader, body, tree)); }
		else {
			if_fail_fe(lcpShellCall(reader, body, name, [&](llc::vcst_t command, llc::vcu0_t input, llc::au0_t & output, llc::u2_t & exitCode) -> llc::err_t {
				cnst llc::err_t status = llsProcess(app, jobIndex, {}, command, true, output, exitCode, input);
				outcome = {status, exitCode, llc::succeeded(status)};
				rtrn status;
			}));
		}
		{ llc::lock_guard guard(app.Lock); if_fail_fe(app.Jobs[jobIndex].Response.append(body)); }
		rtrn 0;
	}
	if_fail_fe(llc::jsonExpressionResolve("params.arguments.action", reader, 0, action));
	if(action != "build") {
		llc::string body;
		if_fail_fe(lcpVSCall(reader, body, &outcome.Status));
		{ llc::lock_guard guard(app.Lock); if_fail_fe(app.Jobs[jobIndex].Response.append(body)); }
		rtrn 0;
	}
	cnst bool build = true;
	llc::string msbuild, command, result;
	if_fail_fe(lcpVSBuildCommand(reader, msbuild, command));
	::llc::au0_t output = {};
	::llc::u2_t exitCode = 0;
	cnst llc::err_t status = llsProcess(app, jobIndex, msbuild, command, build, output, exitCode);
	outcome = {status, exitCode, build && llc::succeeded(status)};
	if(!peer) { if_fail_fe(::llsWrite({(cnst ::llc::sc_t*)output.begin(), output.size()}, stderr)); }
	if_fail_fe(::llc::append_strings(result, "\"result\":{\"resultType\":\"complete\",\"content\":[{\"type\":\"text\",\"text\":\"Visual Studio ", action, ": status ", ::llc::str(status), ", exit code ", ::llc::str(exitCode), ". Output is on stderr.\"}],\"isError\":", ::llc::b82vcs(::llc::failed(status) || 0 != exitCode), '}'));
	if(peer) {
		llc::string text;
		if_fail_fe(llc::append_strings(text, "Visual Studio build: status ", llc::str(status), ", exit code ", llc::str(exitCode), '\n', llc::vcst_t{(cnst char*)output.begin(), output.size()}));
		if_fail_fe(result.clear());
		if_fail_fe(lcpTextBody(result, text, llc::failed(status) || exitCode != 0));
	}
	{
		llc::lock_guard guard(app.Lock);
		if_fail_fe(app.Jobs[jobIndex].Response.append(result));
	}
	rtrn 0;
}

sttc	::llc::err_t	llsRequest			(::llc::SJSONReader & reader, ::llc::vcst_t request, SApplication * app, llc::pobj<SRelayPeer> route = {}) {
	SRelayPeer * peer = route;
	if_fail_fe(reader.Reset());
	if(::llc::failed(::llc::jsonParse(reader, request, ::llc::JSON_PARSE_STRICT)))
		rtrn peer ? relayQueue(*peer, "{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32700,\"message\":\"Parse error\"}}\n") : ::llsWrite("{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32700,\"message\":\"Parse error\"}}\n");
	cnst ::llc::err_t id = ::llc::jsonObjectValueGet(reader, 0, "id");
	if(0 > id) rtrn 0;
	::llc::vcst_t method = {};
	if_fail_fe(::llc::jsonExpressionResolve("method", reader, 0, method));
	llc::vcst_t tool;
	if(method == "tools/call") llc::jsonExpressionResolve("params.name", reader, 0, tool);
	if(app && tool != "session") { llc::lock_guard guard(app->Lock); ++app->Requests; }
	if("server/discover" == method)
		rtrn ::llsRespond(reader, id, "\"result\":{\"resultType\":\"complete\",\"supportedVersions\":[\"2026-07-28\"],\"capabilities\":{\"tools\":{}},\"_meta\":{\"io.modelcontextprotocol/serverInfo\":{\"name\":\"lls\",\"version\":\"0.1.0\"}},\"instructions\":\"Use help path vs for build, launch, list, ping, close and restart. Use forward slashes in paths.\",\"ttlMs\":3600000,\"cacheScope\":\"public\"}", peer);
	if("initialize" == method)
		rtrn ::llsRespond(reader, id, "\"result\":{\"protocolVersion\":\"2025-11-25\",\"capabilities\":{\"tools\":{}},\"serverInfo\":{\"name\":\"lls\",\"version\":\"0.1.0\"},\"instructions\":\"Use vs list to select an IDE instance. Launch requires solution, project and configuration. Supply MSBuild for builds or devenv for a new IDE.\"}", peer);
	if("ping" == method)
		rtrn ::llsRespond(reader, id, "\"result\":{\"resultType\":\"complete\"}", peer);
	if("tools/list" == method)
		rtrn ::llsRespond(reader, id, lcpTools(), peer);
	if("tools/call" == method) {
		llc::vcst_t name;
		if_fail_fe(llc::jsonExpressionResolve("params.name", reader, 0, name));
		if(name == "session" && app) {
			llc::string body;
			if_fail_fe(llsSession(reader, *app, body));
			rtrn llsRespond(reader, id, body, peer);
		}
		if(name == "inbox" && app) {
			llc::string body;
			if_fail_fe(llsMail(reader, body));
			rtrn llsRespond(reader, id, body, peer);
		}
		if(name == "failures" && app) {
			llc::string body;
			if_fail_fe(llsFailures(reader, *app, body));
			rtrn llsRespond(reader, id, body, peer);
		}
		if(name == "help") {
			llc::vcst_t path;
			cnst llc::err_t field = llc::jsonExpressionResolve("params.arguments.path", reader, 0);
			cnst bool valid = field < 0 || llc::succeeded(llc::jsonObjectGetString(reader, (llc::u2_t)field, path));
			cnst llc::vcst_t text = valid ? help(path, true) : llc::vcst_t{};
			llc::string body;
			if_fail_fe(lcpTextBody(body, text.size() ? text : llc::vcst_t{"Unknown help path or non-string path. Open help with an empty path for the index."}, !text.size()));
			rtrn llsRespond(reader, id, body, peer);
		}
		if(name == "status" && app) rtrn llsStatus(reader, id, *app, peer);
		if(name == "vs") {
			llc::vcst_t action;
			if_fail_fe(llc::jsonExpressionResolve("params.arguments.action", reader, 0, action));
			if(action == "list" || action == "ping") {
				llc::string body;
				if_fail_fe(lcpVSCall(reader, body));
				rtrn llsRespond(reader, id, body, peer);
			}
		}
		if((name != "vs" && name != "tree" && name != "cmd" && name != "powershell" && name != "wsl") || !app)
			rtrn llsRespond(reader, id, "\"error\":{\"code\":-32602,\"message\":\"Unknown tool.\"}", peer);
		SBuildJob job;
		job.Peer = route;
		if_fail_fe(llc::jsonWrite(reader.Tree[(llc::u2_t)id], reader.View, job.Id));
		if_fail_fe(job.Request.append(request));
		{
			llc::lock_guard guard(app->Lock);
			for(cnst SBuildJob & previous : app->Jobs)
				if(!previous.Historical && job.Id == previous.Id && job.Peer == previous.Peer)
					rtrn llsRespond(reader, id, "\"error\":{\"code\":-32602,\"message\":\"Build request id already used in this server session.\"}", peer);
			job.OriginSession = app->SessionId;
			job.Sequence = app->Jobs.size();
			cnst llc::err_t saved = llsHistorySave(*app, job);
			if(llc::failed(saved)) {
				app->HistoryError = saved;
				rtrn llsRespond(reader, id, "\"error\":{\"code\":-32603,\"message\":\"History save failed; command was not queued or executed.\"}", peer);
			}
			if_fail_fe(app->Jobs.push_back(job));
			++app->Revision;
			always_printf("Job %u accepted; request id %s.", app->Jobs.size() - 1, job.Id.begin());
			rtrn 0;
		}
	}
	rtrn ::llsRespond(reader, id, "\"error\":{\"code\":-32601,\"message\":\"Method not found\"}", peer);
}



::llc::err_t setup(SApplication & app) {
	if_fail_fe(::llc::setupDefaultLogCallbacks());
	cnst llc::err_t instance = llsDesktopStart();
	if_fail_fe(instance);
	if(instance) { app.Secondary = true; rtrn 0; }
	app.SessionId = llc::timeCurrentInUs();
	if_fail_fe(llsLogStart(app.Log, app.SessionId));
	if_fail_fe(lcpIdentity(app.Identity));
	always_printf("%s", app.Identity.begin());
	app.HistoryError = llsHistoryLoad(app);
	app.NextJob = app.Jobs.size(); // Never execute any restored record, including partial recovery.
	if_fail_fe(lcpToolsLoad("lls_tools.json"));
	if_fail_fe(helpLoad());
	if_fail_fe(llc::tcpipInitialize());
	app.NetworkStarted = true;
#ifdef _DEBUG
	if_fail_fe(relayListen(app.Relay, true));
#else
	if_fail_fe(relayListen(app.Relay, false));
#endif
	if_true_fe(setvbuf(stdin, 0, _IONBF, 0));
	if_fail_fe(llsDesktopAttach(app));
	rtrn 0;
}
::llc::err_t cleanup(SApplication & app) {
	for(auto & peer : app.Relay.Peers) { if_fail_e(relayClose(*peer)); }
	app.Relay.Listener.close();
	if(app.NetworkStarted) { if_fail_e(llc::tcpipShutdown()); }
	if_fail_fe(llsDesktopClose(app));
	rtrn app.WorkerResult;
}

::llc::err_t update(SApplication & app, bool systemRequestedExit) {
	if(app.Secondary) rtrn llc::APPLICATION_STATE_EXIT;
	if(!app.Started) { app.Started = true; rtrn 0; } // Let lrt start draw before reading stdin.
	app.Closing |= systemRequestedExit;
	app.InputClosed |= systemRequestedExit;
	if_fail_fe(llsDesktopUpdate(app));
	if(!app.Closing) { if_fail_fe(relayAccept(app.Relay)); }
	for(llc::u2_t i = 0; i < app.Relay.Peers.size(); ++i) {
		auto peer = app.Relay.Peers[i];
		if(llc::timeCurrentInUs() - peer->Connection.LastPing > 600000000 || llc::failed(relayPump(*peer))) {
			if_fail_e(relayClose(*peer));
			if_fail_fe(app.Relay.Peers.remove_unordered(i--));
			continue;
		}
		if(!app.Closing)
			for(cnst auto & message : peer->Received) {
				cnst auto & payload = message->Payload;
				if(llc::failed(llsRequest(app.Reader, {(cnst char*)payload.begin(), payload.size()}, &app, peer))) {
					cnst llc::err_t id = llc::jsonObjectValueGet(app.Reader, 0, "id");
					if(id >= 0) { if_fail_e(llsRespond(app.Reader, id, "\"error\":{\"code\":-32602,\"message\":\"Invalid request arguments.\"}", peer)); }
				}
			}
		if_fail_fe(peer->Received.clear());
	}
	bool pending = false;
	for(auto & peer : app.Relay.Peers) {
		std::lock_guard guard(peer->Lock);
		pending |= peer->Outgoing.size() || peer->Connection.Queue.Send.size() || peer->Connection.Queue.Sent.size();
	}
	{
		::llc::lock_guard guard(app.Lock);
		if_fail_fe(app.WorkerResult);
		if(app.Closing && !app.Busy && app.NextJob == app.Jobs.size() && !pending)
			rtrn ::llc::APPLICATION_STATE_EXIT;
	}
	if(app.InputClosed) { ::llc::sleep(1); rtrn 0; }
	for(llc::u2_t read = 0; read < 4096; ++read) {
		{
			cnst llc::err_t available = llsInputAvailable();
			if_fail_fe(available);
			if(!available) rtrn 0;
		}
		cnst int character = fgetc(stdin);
		if(character == EOF) break;
		if('\n' != character) {
			if_fail_fe(app.Request.push_back((llc::sc_t)character));
		}
		else {
			if_fail_fe(llsRequest(app.Reader, app.Request, &app));
			rtrn app.Request.clear();
		}
		if(read == 4095) rtrn 0;
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
	llc::err_t checkpoint = 0;
	llc::u2_t jobIndex = 0;
	llc::pobj<SRelayPeer> route;
	{
		::llc::lock_guard guard(app.Lock);
		if(app.NextJob < app.Jobs.size()) {
			jobIndex = app.NextJob;
			route = app.Jobs[jobIndex].Peer;
			if_fail_fe(request.append(app.Jobs[jobIndex].Request));
			++app.NextJob;
			app.Jobs[jobIndex].State = "executing";
			checkpoint = llsHistorySave(app, app.Jobs[jobIndex]);
			if(llc::failed(checkpoint)) app.HistoryError = checkpoint;
			app.Busy = true;
			++app.Revision;
		}
	}
	if(0 == request.size()) {
		::llc::sleep(1);
		rtrn 0;
	}
	::llc::SJSONReader reader;
	if_fail_fe(llc::jsonParse(reader, request, llc::JSON_PARSE_STRICT));
	cnst llc::err_t id = llc::jsonObjectValueGet(reader, 0, "id");
	if_fail_fe(id);
	SRelayPeer * peer = route;
	SBuildResult outcome;
	always_printf("Job %u executing.", jobIndex);
	llc::err_t result = llc::failed(checkpoint) ? checkpoint : llsToolCall(reader, outcome, app, jobIndex, peer);
	if(llc::failed(result)) {
		outcome.Status = result;
		cnst llc::vcst_t error = "\"error\":{\"code\":-32603,\"message\":\"Command failed; inspect failures or session diagnostics.\"}";
		{ llc::lock_guard guard(app.Lock); app.Jobs[jobIndex].Response = error; }
	}
	llc::string response;
	{
		::llc::lock_guard guard(app.Lock);
		auto & job = app.Jobs[jobIndex];
		if(job.Response.size()) {
			llc::string encoded;
			llc::SJSONReader responseReader;
			if_fail_fe(llc::append_strings(encoded, '{', job.Response, '}'));
			if(llc::succeeded(llc::jsonParse(responseReader, encoded, llc::JSON_PARSE_STRICT))) {
				cnst auto responseResult = llc::jsonObjectValueGet(responseReader, 0, "result");
				bool isError = false;
				if(responseResult >= 0 && llc::succeeded(llc::jsonObjectGetAsBoolean(responseReader, responseResult, "isError", isError)) && isError && !outcome.Status && !outcome.ExitCode)
					outcome.Status = -1;
			}
		}
		job.OutputLogStatus = llsLogOutput(app.SessionId, jobIndex, job.Output, job.OutputPath);
		always_printf("Job %u completed: operation=%d exit=%u known=%u log=%s logStatus=%d", jobIndex, outcome.Status, outcome.ExitCode, (unsigned)outcome.ExitKnown, job.OutputPath.begin(), job.OutputLogStatus);
		app.Jobs[jobIndex].Result = outcome;
		app.Jobs[jobIndex].Timer.Frame();
		app.Jobs[jobIndex].State = "completed";
		cnst llc::err_t saved = llsHistorySave(app, job);
		if(llc::failed(saved)) {
			app.HistoryError = saved;
			always_printf("Job %u completed but history save failed: %d. Output file: %s", jobIndex, saved, job.OutputPath.begin());
		}
		if_fail_fe(response.append(job.Response));
		app.WorkerResult = 0;
		app.Busy = false;
		++app.Revision;
	}
	// Persist completion before replying. A lost connector must not kill the service or replay work.
	cnst llc::err_t delivered = llsRespond(reader, id, response, peer);
	if(llc::failed(delivered)) { always_printf("Job %u reply could not be queued: %d. Inspect retained history before retrying.", jobIndex, delivered); }
	rtrn 0;
}
