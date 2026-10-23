#include "lls_app.h"
#include "../lcp/lcp_text.h"
#include "llc_json_expression.h"

llc::err_t llsFailures(cnst llc::SJSONReader & reader, SApplication & app, llc::string & body) {
	llc::s3_t limit = 10;
	cnst auto args = llc::jsonExpressionResolve("params.arguments", reader, 0);
	if(args >= 0 && llc::jsonObjectValueGet(reader, args, "limit") >= 0) {
		if_fail_fe(llc::jsonObjectGetAsInteger(reader, args, "limit", limit));
	}
	if_true_fe(limit < 1 || limit > 100);
	llc::string text;
	llc::lock_guard guard(app.Lock);
	if_fail_fe(llc::append_strings(text, "Latest failures / session ", llc::str(app.SessionId), "\n"));
	llc::u2_t count = 0;
	for(llc::u2_t i = app.Jobs.size(); i && count < (llc::u2_t)limit;) {
		cnst auto & job = app.Jobs[--i];
		if(job.Historical) continue;
		if(job.State != "completed" || !(job.Result.Status < 0 || (job.Result.ExitKnown && job.Result.ExitCode))) continue;
		++count;
		if_fail_fe(llc::append_strings(text, "\nJOB ", llc::str(i), " / request ", job.Id, "\nPID ", llc::str(job.PID),
			" | status ", llc::str(job.Result.Status), " | exit "));
		if(job.Result.ExitKnown) { if_fail_fe(llc::append_strings(text, llc::str(job.Result.ExitCode))); }
		else { if_fail_fe(text.append_string("unknown")); }
		if_fail_fe(llc::append_strings(text, "\nCOMMAND\n", job.Command, "\nOUTPUT FILE\n", job.OutputPath, "\nLog status: ",
			llc::str(job.OutputLogStatus), "\nSTDOUT / STDERR\n", job.Output, "\nRESPONSE\n", job.Response, '\n'));
	}
	if(!count) { if_fail_fe(text.append_string("No failed jobs in this service session.\n")); }
	rtrn lcpTextBody(body, text, false);
}
