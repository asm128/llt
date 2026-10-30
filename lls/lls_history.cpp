#include "lls_app.h"
#include "lls_store.h"
#include "../lcp/lcp_text.h"
#include "llc_path.h"
#include <algorithm>

llc::err_t llsJobJSON(cnst SBuildJob & job, llc::string & json) {
	using namespace llc;
	auto elapsed = job.Timer;
	if(!job.Historical && job.State != "completed") elapsed.Frame();
	if_fail_fe(json.push_back('{'));
	cnst vcst_t names[] = {"id", "state", "command", "request", "output", "response", "outputPath"};
	cnst vcst_t values[] = {job.Id, job.State, job.Command, job.Request, job.Output, job.Response, job.OutputPath};
	for(u2_t i = 0; i < llc::size(names); ++i) {
		if(i) { if_fail_fe(json.push_back(',')); }
		if_fail_fe(append_strings(json, '"', names[i], "\":"));
		if_fail_fe(lcpJSONString(json, values[i]));
	}
	rtrn append_strings(json, ",\"pid\":", str(job.PID), ",\"elapsedMs\":", str(job.Historical ? job.SavedElapsedMs : elapsed.LastTimeMicroseconds / 1000),
		",\"logStatus\":", str(job.OutputLogStatus), ",\"status\":", str(job.Result.Status), ",\"exitCode\":", str(job.Result.ExitCode),
		",\"exitKnown\":", b82st(job.Result.ExitKnown), ",\"started\":", b82st(job.ProcessStarted), ",\"finished\":", b82st(job.ProcessFinished),
		",\"detached\":", b82st(job.Detached), ",\"session\":", str(job.OriginSession), ",\"sequence\":", str(job.Sequence),
		",\"historical\":", b82st(job.Historical), '}');
}
llc::err_t llsHistorySave(SApplication & app, cnst SBuildJob & job) {
	using namespace llc;
	string path, json;
	if_true_fe(!app.HistoryRoot.size());
	if_fail_fe(append_strings(path, app.HistoryRoot, '/', str(job.OriginSession), '_', str(job.Sequence), ".json"));
	if_fail_fe(llsJobJSON(job, json));
	rtrn llsStoreSave(path, json);
}
namespace {
llc::err_t historyString(cnst llc::SJSONReader & reader, llc::vcst_t key, llc::string & output) {
	llc::vcst_t value; llc::string decoded;
	if_fail_fe(llc::jsonObjectGetAsString(reader, 0, key, value));
	if_fail_fe(lcpStringDecode(value, decoded));
	rtrn output.append(value);
}
llc::err_t historyJob(llc::vcst_t path, SBuildJob & job) {
	using namespace llc;
	string bytes, state; SJSONReader reader;
	if_fail_fe(llsStoreLoad(path, bytes, reader, 64 * 1024 * 1024));
	if_fail_fe(historyString(reader, "id", job.Id));
	if_fail_fe(historyString(reader, "state", state));
	if_fail_fe(historyString(reader, "command", job.Command));
	if_fail_fe(historyString(reader, "request", job.Request));
	if_fail_fe(historyString(reader, "output", job.Output));
	if_fail_fe(historyString(reader, "response", job.Response));
	if_fail_fe(historyString(reader, "outputPath", job.OutputPath));
	if_fail_fe(jsonObjectGetAsInteger(reader, 0, "pid", job.PID));
	if_fail_fe(jsonObjectGetAsInteger(reader, 0, "elapsedMs", job.SavedElapsedMs));
	if_fail_fe(jsonObjectGetAsInteger(reader, 0, "session", job.OriginSession));
	if_fail_fe(jsonObjectGetAsInteger(reader, 0, "sequence", job.Sequence));
	if_fail_fe(jsonObjectGetAsInteger(reader, 0, "logStatus", job.OutputLogStatus));
	if_fail_fe(jsonObjectGetAsInteger(reader, 0, "status", job.Result.Status));
	if_fail_fe(jsonObjectGetAsInteger(reader, 0, "exitCode", job.Result.ExitCode));
	if_fail_fe(jsonObjectGetAsBoolean(reader, 0, "exitKnown", job.Result.ExitKnown));
	if_fail_fe(jsonObjectGetAsBoolean(reader, 0, "started", job.ProcessStarted));
	if_fail_fe(jsonObjectGetAsBoolean(reader, 0, "finished", job.ProcessFinished));
	if_fail_fe(jsonObjectGetAsBoolean(reader, 0, "detached", job.Detached));
	job.Historical = true;
	job.State = state == "completed" ? vcst_t{"completed"} : vcst_t{"interrupted"};
	if(job.State == "interrupted") {
		// Persisted executing/queued work is never replayed or presumed successful.
		job.Result.ExitKnown = false;
		job.ProcessFinished = false;
	}
	rtrn 0;
}
}
llc::err_t llsHistoryLoad(SApplication & app) {
	using namespace llc;
	string parent;
	if_fail_fe(pathAbsolute("../.lls-history", parent));
	if_fail_fe(llsStoreDirectory(parent));
#ifdef _DEBUG
	if_fail_fe(append_strings(app.HistoryRoot, parent, "/Debug"));
#else
	if_fail_fe(append_strings(app.HistoryRoot, parent, "/Release"));
#endif
	if_fail_fe(llsStoreDirectory(app.HistoryRoot));
	SPathTree tree;
	if_fail_fe(pathList(tree, app.HistoryRoot, [](bool folder, vcst_c & entry)->err_t {
		if(folder) rtrn 1;
		vcst_t name; if_fail_fe(pathFilename(entry, name));
		rtrn name.size() > 5 && vcst_t{name.begin() + name.size() - 5, 5} == ".json" ? 0 : 1;
	}));
	err_t result = 0;
	for(cnst auto & name : tree.Name) {
		string path;
		if_fail_fe(append_strings(path, app.HistoryRoot, '/', name));
		SBuildJob job;
		cnst err_t loaded = historyJob(path, job);
		if(failed(loaded)) {
			result = loaded;
			always_printf("History record unreadable; retained on disk: %s", path.begin());
			continue;
		}
		if_fail_fe(app.Jobs.push_back(job));
	}
	std::sort(app.Jobs.begin(), app.Jobs.end(), [](cnst SBuildJob & a, cnst SBuildJob & b) {
		rtrn a.OriginSession != b.OriginSession ? a.OriginSession < b.OriginSession : a.Sequence < b.Sequence;
	});
	app.NextJob = app.Jobs.size();
	always_printf("History restored: %u jobs; status %d; root %s", app.Jobs.size(), result, app.HistoryRoot.begin());
	rtrn result;
}
