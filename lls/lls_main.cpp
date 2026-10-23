#include "llc_app_impl.h"
#include "lls_app.h"
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
			if(target != job.Id || (cnst SRelayPeer*)job.Peer != peer) continue;
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

sttc	::llc::err_t	llsToolCall			(cnst ::llc::SJSONReader & reader, ::llc::err_t id, SBuildResult & outcome, SApplication & app, llc::u2_t jobIndex, SRelayPeer * peer) {
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
		rtrn llsRespond(reader, id, body, peer);
	}
	if_fail_fe(llc::jsonExpressionResolve("params.arguments.action", reader, 0, action));
	if(action != "build") {
		llc::string body;
		if_fail_fe(lcpVSCall(reader, body, &outcome.Status));
		{ llc::lock_guard guard(app.Lock); if_fail_fe(app.Jobs[jobIndex].Response.append(body)); }
		rtrn llsRespond(reader, id, body, peer);
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
	rtrn ::llsRespond(reader, id, result, peer);
}

sttc	::llc::err_t	llsRequest			(::llc::SJSONReader & reader, ::llc::vcst_t request, SApplication * app, llc::pobj<SRelayPeer> route = {}) {
	SRelayPeer * peer = route;
	if(app) { llc::lock_guard guard(app->Lock); ++app->Requests; }
	if_fail_fe(reader.Reset());
	if(::llc::failed(::llc::jsonParse(reader, request, ::llc::JSON_PARSE_STRICT)))
		rtrn peer ? relayQueue(*peer, "{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32700,\"message\":\"Parse error\"}}\n") : ::llsWrite("{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32700,\"message\":\"Parse error\"}}\n");
	cnst ::llc::err_t id = ::llc::jsonObjectValueGet(reader, 0, "id");
	if(0 > id) rtrn 0;
	::llc::vcst_t method = {};
	if_fail_fe(::llc::jsonExpressionResolve("method", reader, 0, method));
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
				if(job.Id == previous.Id && job.Peer == previous.Peer)
					rtrn llsRespond(reader, id, "\"error\":{\"code\":-32602,\"message\":\"Build request id already used in this server session.\"}", peer);
			if_fail_fe(app->Jobs.push_back(job));
			++app->Revision;
			rtrn 0;
		}
	}
	rtrn ::llsRespond(reader, id, "\"error\":{\"code\":-32601,\"message\":\"Method not found\"}", peer);
}



#ifdef _DEBUG
sttc ::llc::err_t testSearch();
#endif
::llc::err_t setup(SApplication & app) {
	if_fail_fe(::llc::setupDefaultLogCallbacks());
	cnst llc::err_t instance = llsDesktopStart();
	if_fail_fe(instance);
	if(instance) { app.Secondary = true; rtrn 0; }
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
	if_fail_fe(llsGUISetup(app));
	if_fail_fe(llsDesktopAttach(app));
#ifdef _DEBUG
	if_fail_fe(::testSearch());
	if_fail_fe(llsExplorerTest());
#endif
	rtrn 0;
}
::llc::err_t cleanup(SApplication & app) {
	for(auto & peer : app.Relay.Peers) { if_fail_e(relayClose(*peer)); }
	app.Relay.Listener.close();
	if(app.NetworkStarted) { if_fail_e(llc::tcpipShutdown()); }
	if_fail_fe(llsDesktopClose(app));
	if(app.UI) {
		if_fail_fe(llc::mainWindowDestroy((*app.UI).Framework.RootWindow));
	}
	rtrn app.WorkerResult;
}

::llc::err_t update(SApplication & app, bool systemRequestedExit) {
	if(app.Secondary) rtrn llc::APPLICATION_STATE_EXIT;
	if(!app.Started) { app.Started = true; rtrn 0; } // Let lrt start draw before reading stdin.
	app.Closing |= systemRequestedExit;
	app.InputClosed |= systemRequestedExit;
	if(app.UI) {
		if_fail_fe(llsDesktopUpdate(app));
		cnst llc::err_t guiResult = llsGUIUpdate(app);
		if_fail_fe(guiResult);
		if(guiResult) app.Closing = app.InputClosed = true;
	}
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
		if(app.InputClosed && (!app.UI || app.Closing) && !app.Busy && app.NextJob == app.Jobs.size() && !pending)
			rtrn ::llc::APPLICATION_STATE_EXIT;
	}
	if(app.InputClosed) { ::llc::sleep(1); rtrn 0; }
	for(llc::u2_t read = 0; read < 4096; ++read) {
		if(app.UI) {
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
	llc::err_t result = llsToolCall(reader, id, outcome, app, jobIndex, peer);
	if(llc::failed(result)) {
		outcome.Status = result;
		result = llsRespond(reader, id, "\"error\":{\"code\":-32603,\"message\":\"Build request failed; check stderr and status.\"}", peer);
	}
	{
		::llc::lock_guard guard(app.Lock);
		app.Jobs[jobIndex].Result = outcome;
		app.Jobs[jobIndex].Timer.Frame();
		app.Jobs[jobIndex].State = "completed";
		app.WorkerResult = result;
		app.Busy = false;
		++app.Revision;
	}
	rtrn result;
}

LLC_USING_TYPEINT();

GDEFINE_ENUM_TYPE(AD_SHOP_CATEGORY, uint8_t);
GDEFINE_ENUM_VALUE(AD_SHOP_CATEGORY, tours, 0);
GDEFINE_ENUM_VALUE(AD_SHOP_CATEGORY, meals, 1);
GDEFINE_ENUM_VALUE(AD_SHOP_CATEGORY, shops, 2);
GDEFINE_ENUM_VALUE(AD_SHOP_CATEGORY, shows, 3);

struct SItemViews {
	u2_t						ItemIndex	= 0;
	::llc::vcst_t				URL			= {};
	::llc::vcst_t				Name		= {};
	::llc::vcst_t				Title		= {};
	::llc::vcst_t				Text		= {};
	::llc::vcst_t				ImageHRef	= {};
	::llc::vcst_t				ImageSrc	= {};
	::llc::vcst_t				ImageTitle	= {};
	::llc::vcst_t				ImageAlt	= {};
	::llc::vcst_t				MapURL		= {};
	::llc::au2_t				Barrio		= {};
	::llc::aobj<::llc::vcst_t>	Phones		= {};
	::llc::aobj<::llc::vcst_t>	Addresses	= {};
	::llc::aobj<::llc::vcst_t>	WPs			= {};
};

[[maybe_unused]] sttc ::llc::err_t getItemViews(::llc::SJSONReader & content, const ::AD_SHOP_CATEGORY category, const ::llc::vcst_t & /*lang*/, ::llc::array_obj<::SItemViews> & indicesToDisplay) {
	for(int32_t iItem = 0, countItems = ::llc::jsonArraySize(*content[0]); iItem < countItems; ++iItem) {
		SItemViews												views								= {(uint32_t)iItem};
		const ::llc::error_t									jsonIndexCurrentItem				= ::llc::jsonArrayValueGet(*content[0], iItem);
		::llc::view_const_string								viewWikiCategory					= {};
		const ::llc::error_t									jsonIndexArrayCategory				= ::llc::jsonExpressionResolve(::llc::vcs{"category"}, content, jsonIndexCurrentItem, viewWikiCategory);
		bool													isCategory							= false;
		for(uint32_t iCat = 0, countCat = (uint32_t)::llc::jsonArraySize(*content[jsonIndexArrayCategory]); iCat < countCat; ++iCat) {
			const ::llc::error_t									jsonIndexNodeCategory				= ::llc::jsonArrayValueGet(*content[jsonIndexArrayCategory], iCat);
			const ::llc::SJSONToken									& jsonToken							= content.Token[jsonIndexNodeCategory];
			if(category == jsonToken.Value) {
				isCategory											= true;
				break;
			}
		}
		if(false == isCategory)
			continue;

		// ---- Root arrays.
		const ::llc::error_t									jsonIndexArrayPhone					= ::llc::jsonExpressionResolve(::llc::vcs{"phone"		}, content, jsonIndexCurrentItem, views.MapURL	);
		const ::llc::error_t									jsonIndexArrayWP					= ::llc::jsonExpressionResolve(::llc::vcs{"whatsapp"	}, content, jsonIndexCurrentItem, views.MapURL	);
		const ::llc::error_t									jsonIndexArrayAddr					= ::llc::jsonExpressionResolve(::llc::vcs{"address"		}, content, jsonIndexCurrentItem, views.MapURL	);
		const ::llc::error_t									jsonIndexArrayBarrios				= ::llc::jsonExpressionResolve(::llc::vcs{"barrio"		}, content, jsonIndexCurrentItem, views.MapURL	);
		// ---- Root properties.																								   ::llc::vcs{
		//const ::llc::error_t									jsonIndexName						= ::llc::jsonExpressionResolve(::llc::vcs{"name"		}, content, jsonIndexCurrentItem, views.Name	);
		//const ::llc::error_t									jsonIndexMap						= ::llc::jsonExpressionResolve(::llc::vcs{"location"	}, content, jsonIndexCurrentItem, views.MapURL	);
		// ---- Language-based properties.																						   ::llc::vcs{
		//const ::llc::error_t									jsonIndexLang						= ::llc::jsonExpressionResolve(lang						 , content, jsonIndexCurrentItem, views.URL		);
		//const ::llc::error_t									jsonIndexWiki						= ::llc::jsonExpressionResolve(::llc::vcs{"wiki"		}, content, jsonIndexLang, views.URL			);
		//const ::llc::error_t									jsonIndexTitle						= ::llc::jsonExpressionResolve(::llc::vcs{"title"		}, content, jsonIndexLang, views.Title			);
		//const ::llc::error_t									jsonIndexText						= ::llc::jsonExpressionResolve(::llc::vcs{"text"		}, content, jsonIndexLang, views.Text			);
		//const ::llc::error_t									jsonIndexImageHRef					= ::llc::jsonExpressionResolve(::llc::vcs{"image.href"	}, content, jsonIndexLang, views.ImageHRef		);
		//const ::llc::error_t									jsonIndexImageSrc					= ::llc::jsonExpressionResolve(::llc::vcs{"image.src"	}, content, jsonIndexLang, views.ImageSrc		);
		//const ::llc::error_t									jsonIndexImageTitle					= ::llc::jsonExpressionResolve(::llc::vcs{"image.title" }, content, jsonIndexLang, views.ImageTitle		);
		//const ::llc::error_t									jsonIndexImageAlt					= ::llc::jsonExpressionResolve(::llc::vcs{"image.alt"	}, content, jsonIndexLang, views.ImageAlt		);

		for(int32_t iPhone	= 0, countAddr		= ::llc::jsonArraySize(*content[jsonIndexArrayAddr		]); iPhone	< countAddr		; ++iPhone	)	views.Addresses	.push_back(content.View[::llc::jsonArrayValueGet(*content[jsonIndexArrayAddr	], iPhone	)]);
		for(int32_t iPhone	= 0, countPhones	= ::llc::jsonArraySize(*content[jsonIndexArrayPhone		]); iPhone	< countPhones	; ++iPhone	)	views.Phones	.push_back(content.View[::llc::jsonArrayValueGet(*content[jsonIndexArrayPhone	], iPhone	)]);
		for(int32_t iPhone	= 0, countWPs		= ::llc::jsonArraySize(*content[jsonIndexArrayWP		]); iPhone	< countWPs		; ++iPhone	)	views.WPs		.push_back(content.View[::llc::jsonArrayValueGet(*content[jsonIndexArrayWP		], iPhone	)]);
		for(int32_t iBarrio	= 0, countBarrios	= ::llc::jsonArraySize(*content[jsonIndexArrayBarrios	]); iBarrio	< countBarrios	; ++iBarrio	)	views.Barrio	.push_back((uint32_t)content.Token[::llc::jsonArrayValueGet(*content[jsonIndexArrayBarrios], iBarrio	)].Value);

		llc_necall(indicesToDisplay.push_back(views), "%s", "Out of memory?");
	}
	return 0;
}

sttc	llc::err_t			getWordList							(const ::SItemViews & item, ::llc::aobj<::llc::string> & itemWords) {
	::llc::aobj<::llc::vcst_t>		upper								= {};
	llc_necall(::llc::split(item.Text	, ' ', upper), "%s", "Out of memory?");
	llc_necall(::llc::split(item.Title	, ' ', upper), "%s", "Out of memory?");
	llc_necall(::llc::split(item.URL	, ' ', upper), "%s", "Out of memory?");
	llc_necall(::llc::split(item.Text	, ' ', upper), "%s", "Out of memory?");
	for(uint32_t iAddr = 0; iAddr < item.Addresses	.size(); ++iAddr) llc_necall(::llc::split(item.Addresses[iAddr], ' ', upper), "%s", "Out of memory?");
	for(uint32_t iAddr = 0; iAddr < item.Phones		.size(); ++iAddr) llc_necall(::llc::split(item.Phones	[iAddr], ' ', upper), "%s", "Out of memory?");
	for(uint32_t iAddr = 0; iAddr < item.WPs		.size(); ++iAddr) llc_necall(::llc::split(item.WPs		[iAddr], ' ', upper), "%s", "Out of memory?");

	llc_necall(itemWords.resize(upper.size()), "%s", "Out of memory?");
	const ::llc::kv<::llc::vcst_t>	replaceDict	[]						=
		{ ::llc::kv<::llc::vcst_t>
			{ {"áéíóúàèìòùäëïöüãõñçºª"}
			, {"aeiouaeiouaeiouaoncoa"}
			}
		};
	::llc::string					fixedWord;
	for(uint32_t iWord = 0, countWords = upper.size(); iWord < countWords; ++iWord) {
		::llc::vcst_t					originalWord						= upper[iWord];
		::llc::trim(originalWord, originalWord);
		::llc::string					& finalWord							= itemWords[iWord];
		finalWord					= originalWord;
		::llc::tolower(finalWord);
		if( finalWord == "the"	
		 || finalWord == "and"	
		 || finalWord == "it"	
		 || finalWord == "to"	
		 || finalWord == "an"	
		 || finalWord == "by"	
		 || finalWord == "was"	
		 || finalWord == "is"	
		 || finalWord == "in"	
		 || finalWord == "as"	
		 || finalWord == "for"	
		 || finalWord == "of"	
		 || finalWord == "up"	
		 || finalWord == "a"	
		 || finalWord == "un"	
		 || finalWord == "con"	
		 || finalWord == "del"	
		 || finalWord == "por"	
		 || finalWord == "que"	
		 || finalWord == "es"	
		 || finalWord == "en"	
		 || finalWord == "de"	
		 || finalWord == "la"	
		 || finalWord == "el"	
		 || finalWord == "para"
		 || finalWord == "los"	
		 || finalWord == "las"	
		 )	{
			finalWord.clear();
			continue;
		}
		info_printf("finalWord: %.*s.", LLC_VIEW_ARGS(finalWord));
		fixedWord											= finalWord;
		bool													replaced		= false;
		for(uint32_t iChar = 0; iChar < fixedWord.size(); ++iChar) {
			char													& wordChar		= fixedWord[iChar];
			for(uint32_t iTable = 0; iTable < ::llc::size(replaceDict); ++iTable) {
				const ::llc::kv<::llc::vcst_t> &	table = replaceDict[iTable];
				for(uint32_t iItem = 0; iItem < table.Key.size(); ++iItem)
					if(wordChar == table.Key[iItem]) {
						wordChar											= table.Val[iItem];
						replaced											= true;
					}
			}
		}
		if(replaced) {
			itemWords.push_back(fixedWord);
			info_printf("fixedWord: %.*s.", LLC_VIEW_ARGS(fixedWord));
		}
	}
	return 0;
}

struct SWordIndices {
	::llc::string			Text;
	::llc::au2_t			Indices;
};

struct SSearchCache {
	::llc::string				Query;
	::llc::aobj<::llc::vcst_t>	Words;
	::llc::au0_t				Matched;
};

// Indices address items; query and output storage must be separate from the cache.
[[maybe_unused]] sttc ::llc::err_t obeSearch
	( ::llc::vcst_t textToFind, ::llc::view<cnst SWordIndices> dictionary, ::llc::view<cnst SItemViews> items
	, ::llc::au2_t & output, SSearchCache & cache, ::llc::s2_t selectedBarrio = -1
	) {
	if_fail_fe(output.clear());
	if_fail_fe(cache.Words.clear());
	if_fail_fe(cache.Query.clear());
	if_fail_fe(cache.Query.append(textToFind));
	if_fail_fe(::llc::tolower(cache.Query));
	cnst ::llc::vcst_t query = cache.Query;
	if_fail_fe(::llc::split(query, ' ', cache.Words));
	if_fail_fe(cache.Matched.resize(items.size()));
	cache.Matched.fill(0);
	for(cnst SWordIndices & word : dictionary)
		for(::llc::vcst_t toFind : cache.Words) {
			if(0 > word.Text.find(toFind))
				continue;
			for(::llc::u2_t id : word.Indices)
				cache.Matched[id] = 1;
			break;
		}
	for(::llc::u2_t id = 0; id < items.size(); ++id) {
		if(!cache.Matched[id])
			continue;
		if(selectedBarrio >= 0 && 0 > ::llc::find((::llc::u2_t)selectedBarrio, {items[id].Barrio.begin(), items[id].Barrio.size()}))
			continue;
		if_fail_fe(output.push_back(id));
	}
	rtrn output.size();
}

llc::err_t llsHelpFind(llc::vcst_t query, llc::au2_t & matches) {
	sttc llc::aobj<SWordIndices> titles, dictionary;
	sttc llc::aobj<SItemViews> items;
	sttc SSearchCache cache;
	sttc llc::au2_t contents;
	if(!items.size()) {
		llc::aobj<llc::vcst_t> paths;
		if_fail_fe(helpPaths(paths, true));
		if_fail_fe(items.resize(paths.size()));
		if_fail_fe(titles.resize(paths.size()));
		if_fail_fe(dictionary.resize(paths.size()));
		for(llc::u2_t i = 0; i < paths.size(); ++i) {
			cnst llc::vcst_t page = help(paths[i], true);
			llc::vcst_t title, body;
			if_fail_fe(llc::split<llc::sc_c>('\n', page, title, body));
			llc::vcst_t family = paths[i], child;
			llc::split<llc::sc_c>(' ', paths[i], family, child);
			if_fail_fe(llc::append_strings(titles[i].Text, paths[i], ' ', title, ' ', lcpToolTitle(family)));
			if_fail_fe(llc::tolower(titles[i].Text));
			if_fail_fe(titles[i].Indices.push_back(i));
			if_fail_fe(dictionary[i].Text.append(body));
			if_fail_fe(llc::tolower(dictionary[i].Text));
			if_fail_fe(dictionary[i].Indices.push_back(i));
		}
	}
	if(query.size()) {
		// Keep catalog hierarchy within each tier: titles first, body-only matches last.
		if_fail_fe(obeSearch(query, titles, items, matches, cache));
		if_fail_fe(obeSearch(query, dictionary, items, contents, cache));
		for(llc::u2_t id : contents)
			if(matches.find(id) < 0) { if_fail_fe(matches.push_back(id)); }
		rtrn matches.size();
	}
	if_fail_fe(matches.clear());
	for(llc::u2_t i = 0; i < items.size(); ++i) {
		if_fail_fe(matches.push_back(i));
	}
	rtrn matches.size();
}

#ifdef _DEBUG
sttc ::llc::err_t testSearch() {
	SItemViews items[3] = {};
	if_fail_fe(items[0].Barrio.push_back(0));
	if_fail_fe(items[0].Barrio.push_back(1));
	if_fail_fe(items[1].Barrio.push_back(1));
	if_fail_fe(items[2].Barrio.push_back(0));
	cnst SWordIndices dictionary[] = {{"museum", {2, 0}}, {"music", {1, 0}}, {"park", {2}}};
	cnst struct { ::llc::vcst_t Query; ::llc::s2_t Barrio; ::llc::u2_t Expected[3], Count; } cases[] = {
		{"MUS", -1, {0, 1, 2}, 3}, {"seu", -1, {0, 2}, 2},
		{"park music", -1, {0, 1, 2}, 3}, {"mus mus", -1, {0, 1, 2}, 3},
		{"  MUS   park ", -1, {0, 1, 2}, 3}, {"missing", -1, {}, 0},
		{"", -1, {}, 0}, {"   ", -1, {}, 0},
		{"mus", 0, {0, 2}, 2}, {"mus", 1, {0, 1}, 2},
		{"mus", 99, {}, 0}, {"park", 1, {}, 0}, {"park", 0, {2}, 1},
		{"mus\tpark", -1, {}, 0}, {"music", -1, {0, 1}, 2},
	};
	SSearchCache cache;
	::llc::au2_t output;
	for(cnst auto & test : cases) {
		if_fail_fe(output.push_back(99)); // Every search replaces the previous result.
		::llc::err_t count;
		if_fail_fe(count = ::obeSearch(test.Query, dictionary, items, output, cache, test.Barrio));
		if_true_fef(count != (::llc::err_t)test.Count || output.size() != test.Count, "Search '%.*s', barrio:%i, count:%i, expected:%u.", LLC_VIEW_ARGS(test.Query), test.Barrio, count, test.Count);
		for(::llc::u2_t i = 0; i < test.Count; ++i) {
			if_true_fef(output[i] != test.Expected[i], "Search '%.*s', result:%u, id:%u, expected:%u.", LLC_VIEW_ARGS(test.Query), i, output[i], test.Expected[i]);
		}
	}
	if_fail_fe(::obeSearch("mus", {}, items, output, cache));
	if_true_fef(output.size(), "%s", "Empty dictionary produced matches.");
	if_fail_fe(::obeSearch("mus", {}, {}, output, cache));
	if_true_fef(output.size(), "%s", "Empty input produced matches.");
	llc::aobj<llc::vcst_t> paths;
	if_fail_fe(helpPaths(paths, true));
	if_fail_fe(llsHelpFind("BuIlD", output));
	if_true_fe(!output.size() || paths[output[0]] != "vs build");
	if_fail_fe(llsHelpFind("visu", output));
	bool foundBuild = false, foundRun = false;
	for(llc::u2_t id : output) { foundBuild |= paths[id] == "vs build"; foundRun |= paths[id] == "vs run"; }
	if_true_fe(!foundBuild || !foundRun);
	for(llc::u2_t i = 0; i < output.size(); ++i)
		for(llc::u2_t j = i + 1; j < output.size(); ++j) { if_true_fe(output[i] == output[j]); }
	if_fail_fe(llsHelpFind({}, output));
	if_true_fe(output.size() != paths.size());
	info_printf("Search tests: %u cases and help title-priority checks passed.", (::llc::u2_t)::llc::size(cases) + 2);
	rtrn 0;
}
#endif

static	::llc::error_t	getWordDict			(const ::SItemViews & item, u2_t iItem, ::llc::aobj<::SWordIndices> & wordSet, ::llc::aobj<::llc::string> & cacheItemWords) {
	if_fail_fe(::getWordList(item, cacheItemWords));
	for(u2_t iItemWord = 0; iItemWord < cacheItemWords.size(); ++iItemWord) {
		::llc::vcst_t				itemWord						= cacheItemWords[iItemWord];
		info_printf("itemWord:\"%.*s\".", LLC_VIEW_ARGS(itemWord));
		if(1 >= itemWord.size())
			continue;
		bool													bFound							= false;
		if(',' == itemWord[itemWord.size() - 1] || '.' == itemWord[itemWord.size() - 1]) {
			itemWord											= {itemWord.begin(), itemWord.size() - 1};
			if(0 >= itemWord.size())
				continue;
		}
		for(uint32_t iWordInSet = 0; iWordInSet < wordSet.size(); ++iWordInSet) {
			SWordIndices											& dictWord						= wordSet[iWordInSet];
			if(itemWord != dictWord.Text)
				continue;
			bFound												= true;
			if(0 > ::llc::find(iItem, {dictWord.Indices.begin(), dictWord.Indices.size()})) {
				if_fail_fe(dictWord.Indices.push_back(iItem));
			}
		}
		if(false == bFound) {
			::llc::err_t index;
			if_fail_fe(index = wordSet.push_back({itemWord}));
			if_fail_fe(wordSet[index].Indices.push_back(iItem));
		}
	}
	return 0;
}

struct SBarrio {
			::llc::vcst_t						Name						= {};
			::llc::vcst_t						Area						= {};
			::llc::vcst_t						Population					= {};
			::llc::vcst_t						Commune						= {};
};

[[maybe_unused]] sttc ::llc::err_t outputSearchScript(const ::llc::view_array<const ::SItemViews> & indicesToDisplay, ::llc::string & output) {
	::llc::array_obj<::SWordIndices>						wordSet;
	::llc::array_obj<::llc::string>				cacheItemWords;

	for(uint32_t iItem = 0, countItems = indicesToDisplay.size(); iItem < countItems; ++iItem) {
		if_fail_fe(::getWordDict(indicesToDisplay[iItem], iItem, wordSet, cacheItemWords));
	}

	output.append_string("\nvar names=[");
	for(uint32_t iWord = 0, countWords = wordSet.size(); iWord < countWords; ++iWord) {
		const SWordIndices										& element							= wordSet[iWord];
		info_printf("Word:\"%.*s\"", LLC_VIEW_ARGS(element.Text));
		if(0 <= ::llc::find('\'', {element.Text})) {
			if(0 <= ::llc::find('"', {element.Text}))
				continue;
			output.push_back('"');
			output.append(element.Text);
			output.push_back('"');
		}
		else {
			output.push_back('\'');
			output.append(element.Text);
			output.push_back('\'');
		}
		if(iWord < (wordSet.size() - 1))
			output.push_back(',');
	}
	output.append_string("];");

	output.append_string("\nvar indices=[");
	char													tempIntStr[256]				= {};
	for(uint32_t iWord = 0; iWord < wordSet.size(); ++iWord) {
		const SWordIndices		& element = wordSet[iWord];
		output.push_back('[');
		for(uint32_t iArticle = 0; iArticle < element.Indices.size(); ++iArticle) {
			sprintf_s(tempIntStr, "%u", element.Indices[iArticle]);
			output.append_string(tempIntStr);
			if(iArticle < (element.Indices.size() - 1))
				output.push_back(',');
		}
		output.push_back(']');
		if(iWord < (wordSet.size() - 1))
			output.push_back(',');
	}
	output.append_string("];");

	output.append_string("\nvar idList=[");
	for(uint32_t iArticle = 0; iArticle < indicesToDisplay.size(); ++iArticle) {
		sprintf_s(tempIntStr, "%u", iArticle);
		output.append_string(tempIntStr);
		if(iArticle < (indicesToDisplay.size() - 1))
			output.push_back(',');
	}
	output.append_string("];");
	output.append_string("\nvar barrioIdMap=[");
	for(uint32_t iArticle = 0; iArticle < indicesToDisplay.size(); ++iArticle) {
		const ::SItemViews										& ad								= indicesToDisplay[iArticle];
		output.push_back('[');
		for(uint32_t iBarrio = 0; iBarrio < ad.Barrio.size(); ++iBarrio) {
			sprintf_s(tempIntStr, "%u", ad.Barrio[iBarrio]);
			output.append_string(tempIntStr);
			if(iBarrio < (ad.Barrio.size() - 1))
				output.push_back(',');
		}
		output.push_back(']');
		if(iArticle < (indicesToDisplay.size() - 1))
			output.push_back(',');
	}
	output.append_string("];");
	return 0;
}
