#include "lls_app.h"
#include "../lcp/lcp_text.h"
#include "llc_json_expression.h"

llc::err_t llsSession(cnst llc::SJSONReader & reader, SApplication & app, llc::string & body) {
	llc::u3_t session = 0;
	llc::u2_t revision = (llc::u2_t)-1;
	cnst llc::err_t args = llc::jsonExpressionResolve("params.arguments", reader, 0);
	if(args >= 0) {
		if(llc::jsonObjectValueGet(reader, args, "session") >= 0) { if_fail_fe(llc::jsonObjectGetAsInteger(reader, args, "session", session)); }
		if(llc::jsonObjectValueGet(reader, args, "revision") >= 0) { if_fail_fe(llc::jsonObjectGetAsInteger(reader, args, "revision", revision)); }
	}
	llc::string diagnostics;
	int logError = 0;
	if_fail_fe(llsLogSnapshot(app.Log, diagnostics, logError));
	llc::lock_guard guard(app.Lock);
	auto timer = app.Session;
	timer.Frame();
	cnst bool changed = session != app.SessionId || revision != app.Revision || app.Busy;
	if_fail_fe(llc::append_strings(body, "\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"LLS session snapshot\"}],\"isError\":false,\"structuredContent\":{\"session\":",
		llc::str(app.SessionId), ",\"revision\":", llc::str(app.Revision), ",\"requests\":", llc::str(app.Requests),
		",\"elapsedMs\":", llc::str(timer.LastTimeMicroseconds / 1000), ",\"changed\":", llc::b82vcs(changed), ",\"identity\":"));
	if_fail_fe(lcpJSONString(body, app.Identity));
	if_fail_fe(body.append_string(",\"diagnostics\":"));
	if_fail_fe(lcpJSONString(body, diagnostics));
	if_fail_fe(body.append_string(",\"logPath\":"));
	if_fail_fe(lcpJSONString(body, app.Log.Path));
	if_fail_fe(llc::append_strings(body, ",\"logError\":", llc::str(logError), ",\"historyError\":", llc::str(app.HistoryError)));
	if(changed) {
		if_fail_fe(body.append_string(",\"jobs\":["));
		for(llc::u2_t i = 0; i < app.Jobs.size(); ++i) {
			if(i) { if_fail_fe(body.push_back(',')); }
			if_fail_fe(llsJobJSON(app.Jobs[i], body));
		}
		if_fail_fe(body.push_back(']'));
	}
	rtrn body.append_string("}}");
}
