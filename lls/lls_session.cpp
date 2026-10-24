#include "lls_app.h"
#include "../lcp/lcp_text.h"
#include "../lcp/lcp_vs.h"
#include "llc_json_expression.h"

sttc llc::err_t llsJobSummaryJSON(cnst SBuildJob & job, llc::string & json) {
	auto elapsed = job.Timer;
	if(!job.Historical && job.State != "completed") elapsed.Frame();
	if_fail_fe(json.append_string("{\"id\":"));
	if_fail_fe(lcpJSONString(json, job.Id));
	if_fail_fe(json.append_string(",\"state\":"));
	if_fail_fe(lcpJSONString(json, job.State));
	llc::string summary;
	if(job.State == "completed") {
		if(job.Result.Status < 0 || (job.Result.ExitKnown && job.Result.ExitCode)) {
			if_fail_fe(lcpVSBuildSummary(job.Output, job.Result.Status, job.Result.ExitCode, summary));
		}
		else {
			if_fail_fe(summary.append_string("Succeeded."));
		}
	}
	else if(job.State == "interrupted") { if_fail_fe(summary.append_string("Interrupted.")); }
	else { if_fail_fe(summary.append(job.State)); }
	if_fail_fe(json.append_string(",\"summary\":"));
	if_fail_fe(lcpJSONString(json, summary));
	rtrn llc::append_strings(json, ",\"pid\":", llc::str(job.PID), ",\"elapsedMs\":", llc::str(job.Historical ? job.SavedElapsedMs : elapsed.LastTimeMicroseconds / 1000),
		",\"logStatus\":", llc::str(job.OutputLogStatus), ",\"status\":", llc::str(job.Result.Status), ",\"exitCode\":", llc::str(job.Result.ExitCode),
		",\"exitKnown\":", llc::b82vcs(job.Result.ExitKnown), ",\"started\":", llc::b82vcs(job.ProcessStarted), ",\"finished\":", llc::b82vcs(job.ProcessFinished),
		",\"detached\":", llc::b82vcs(job.Detached), ",\"session\":", llc::str(job.OriginSession), ",\"sequence\":", llc::str(job.Sequence),
		",\"historical\":", llc::b82vcs(job.Historical), '}');
}

llc::err_t llsSession(cnst llc::SJSONReader & reader, SApplication & app, llc::string & body) {
	llc::u3_t session = 0;
	llc::u2_t revision = (llc::u2_t)-1;
	cnst llc::err_t args = llc::jsonExpressionResolve("params.arguments", reader, 0);
	if(args >= 0) {
		if(llc::jsonObjectValueGet(reader, args, "session") >= 0) { if_fail_fe(llc::jsonObjectGetAsInteger(reader, args, "session", session)); }
		if(llc::jsonObjectValueGet(reader, args, "revision") >= 0) { if_fail_fe(llc::jsonObjectGetAsInteger(reader, args, "revision", revision)); }
	}
	int logError = 0;
	{ llc::lock_guard logGuard(app.Log.Lock); logError = app.Log.FileError; }
	llc::lock_guard guard(app.Lock);
	auto timer = app.Session;
	timer.Frame();
	cnst bool changed = session != app.SessionId || revision != app.Revision || app.Busy;
	if_fail_fe(llc::append_strings(body, "\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"ok\"}],\"isError\":false,\"structuredContent\":{\"session\":",
		llc::str(app.SessionId), ",\"revision\":", llc::str(app.Revision), ",\"requests\":", llc::str(app.Requests),
		",\"elapsedMs\":", llc::str(timer.LastTimeMicroseconds / 1000), ",\"changed\":", llc::b82vcs(changed)));
	if_fail_fe(llc::append_strings(body, ",\"logError\":", llc::str(logError), ",\"historyError\":", llc::str(app.HistoryError)));
	if(changed) {
		if_fail_fe(body.append_string(",\"jobs\":["));
		for(llc::u2_t i = 0; i < app.Jobs.size(); ++i) {
			if(i) { if_fail_fe(body.push_back(',')); }
			if_fail_fe(llsJobSummaryJSON(app.Jobs[i], body));
		}
		if_fail_fe(body.push_back(']'));
	}
	rtrn body.append_string("}}");
}
