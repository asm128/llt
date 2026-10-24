#include "lls_app.h"
#include "../lcp/lcp_text.h"
#include "../lcp/lcp_vs.h"
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
	if_fail_fe(text.append_string("failed jobs:"));
	llc::u2_t count = 0;
	for(llc::u2_t i = app.Jobs.size(); i && count < (llc::u2_t)limit;) {
		cnst auto & job = app.Jobs[--i];
		if(job.Historical) continue;
		if(job.State != "completed" || !(job.Result.Status < 0 || (job.Result.ExitKnown && job.Result.ExitCode))) continue;
		++count;
		llc::string summary;
		if_fail_fe(llsJobSummary(job, summary));
		if_fail_fe(llc::append_strings(text, '\n', llc::str(i), ": ", summary));
	}
	if(!count) { if_fail_fe(text.append_string("\nnone")); }
	rtrn lcpTextBody(body, text, false);
}
