#include "lls_app.h"
#include "../lcp/lcp_text.h"
#include "../lcp/lcp_vs.h"
#include "llc_json_expression.h"

sttc llc::err_t llsJobTool(cnst SBuildJob & job, llc::string & tool, llc::string & action) {
	llc::SJSONReader reader;
	if(llc::failed(llc::jsonParse(reader, job.Request, llc::JSON_PARSE_STRICT))) rtrn 0;
	llc::vcst_t value;
	if(llc::succeeded(llc::jsonExpressionResolve("params.name", reader, 0, value))) { if_fail_fe(tool.append(value)); }
	if(llc::succeeded(llc::jsonExpressionResolve("params.arguments.action", reader, 0, value))) { if_fail_fe(action.append(value)); }
	rtrn tool.size();
}

sttc llc::err_t llsCommandOutputSummary(llc::vcst_t output, llc::string & summary) {
	while(output.size()) {
		llc::vcst_t line, remaining;
		cnst llc::err_t split = llc::split<llc::sc_c>('\n', output, line, remaining);
		if(llc::failed(split)) { line = output; remaining = {}; }
		output = remaining;
		llc::trim(line);
		if(!line.size() || line == "#< CLIXML" || (line.size() >= 5 && 0 == memcmp(line.begin(), "<Objs", 5))) continue;
		cnst llc::err_t xml = line.find(llc::vcst_t{"<Objs"});
		if(xml >= 0) line = {line.begin(), (llc::u2_t)xml};
		llc::trim(line);
		if(!line.size()) continue;
		cnst llc::u2_t limit = line.size() < 512 ? line.size() : 512;
		for(llc::u2_t i = 0; i < limit; ++i) {
			cnst llc::u0_t value = (llc::u0_t)line[i];
			if(value == '\t' || value >= ' ') { if_fail_fe(summary.push_back(line[i])); }
		}
		rtrn summary.size();
	}
	rtrn 0;
}

llc::err_t llsJobSummary(cnst SBuildJob & job, llc::string & summary) {
	if_fail_fe(summary.clear());
	if(job.State == "interrupted") rtrn summary.append_string("Interrupted.");
	if(job.State != "completed") rtrn summary.append(job.State);
	cnst bool failed = job.Result.Status < 0 || (job.Result.ExitKnown && job.Result.ExitCode);
	if(!failed) rtrn summary.append_string("Succeeded.");
	llc::string tool, action;
	if_fail_fe(llsJobTool(job, tool, action));
	if(tool == "vs" && action == "build") rtrn lcpVSBuildSummary(job.Output, job.Result.Status, job.Result.ExitCode, summary);
	if_fail_fe(llsCommandOutputSummary(job.Output, summary));
	if(summary.size()) rtrn summary.size();
	cnst llc::vcst_t toolName = tool.size() ? llc::vcst_t{tool.begin(), tool.size()} : llc::vcst_t{"Command"};
	rtrn llc::append_strings(summary, toolName, " failed (", llc::str(job.Result.Status), '/', llc::str(job.Result.ExitCode), ").");
}

#ifdef _DEBUG
llc::err_t llsJobSummaryTest() {
	SBuildJob job;
	job.State = "completed";
	job.Result = {0, 7, true};
	job.Request = "{\"params\":{\"name\":\"powershell\",\"arguments\":{\"command\":\"fixture\"}}}";
	job.Output = "#< CLIXML\r\nSURVIVED-DASHBOARD-EXIT<Objs Version=\"1.1.0.1\">noise</Objs>";
	llc::string summary;
	if_fail_fe(llsJobSummary(job, summary));
	if_true_ve(-1, summary != "SURVIVED-DASHBOARD-EXIT");
	job.Request = "{\"params\":{\"name\":\"vs\",\"arguments\":{\"action\":\"build\"}}}";
	job.Output = "D:\\source\\sample.cpp(7,3): error C2664: cannot convert argument 1 [sample.vcxproj]\n";
	if_fail_fe(llsJobSummary(job, summary));
	if_true_ve(-1, summary != "sample.cpp(7,3): error C2664: cannot convert argument 1");
	job.Request = "{\"params\":{\"name\":\"cmd\",\"arguments\":{\"command\":\"fixture\"}}}";
	job.Output.clear();
	if_fail_fe(llsJobSummary(job, summary));
	if_true_ve(-1, summary != "cmd failed (0/7).");
	rtrn 0;
}
#endif

sttc llc::err_t llsJobSummaryJSON(cnst SBuildJob & job, llc::string & json) {
	auto elapsed = job.Timer;
	if(!job.Historical && job.State != "completed") elapsed.Frame();
	if_fail_fe(json.append_string("{\"id\":"));
	if_fail_fe(lcpJSONString(json, job.Id));
	if_fail_fe(json.append_string(",\"state\":"));
	if_fail_fe(lcpJSONString(json, job.State));
	llc::string summary;
	if_fail_fe(llsJobSummary(job, summary));
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
