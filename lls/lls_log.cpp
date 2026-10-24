#include "lls_log.h"
#include "llc_runtime.h"
#include "llc_string_compose.h"
#include <cstring>
#include <cerrno>
#ifdef LLC_WINDOWS
#include <share.h>
#endif

namespace {
SServiceLog * Active = {};
llc::err_t logWrite(cnst char * text, llc::u2_t count) {
	if(!Active || !text) rtrn 0;
	auto & log = *Active;
	llc::lock_guard guard(log.Lock);
	for(llc::u2_t i = 0; i < count; ++i) {
		log.Bytes[(log.Start + log.Count) % log.Bytes.size()] = text[i];
		if(log.Count == log.Bytes.size()) log.Start = (log.Start + 1) % log.Bytes.size();
		else ++log.Count;
	}
	++log.Revision;
	if(log.File && !log.FileError) {
		cnst bool failed = fwrite(text, 1, count, log.File) != count;
		if(fflush(log.File) || failed) log.FileError = errno ? errno : EIO;
	}
	rtrn (llc::err_t)count;
}
llc::err_t logPrint(cnst char * text) { rtrn text ? logWrite(text, (llc::u2_t)strlen(text)) : 0; }
llc::err_t logPath(llc::string & path, llc::u3_t session, llc::vcst_t suffix) {
	llc::string relative;
	if_fail_fe(llc::append_strings(relative, "lls_logs/session_", llc::str(session), suffix));
#ifdef LLC_WINDOWS
	char full[32768] = {};
	cnst DWORD size = GetFullPathNameA(relative.begin(), (DWORD)llc::size(full), full, 0);
	if_true_fe(!size || size >= llc::size(full));
	rtrn path.append(full, size);
#else
	rtrn path.append(relative);
#endif
}
}

llc::err_t llsLogStart(SServiceLog & log, llc::u3_t session) {
	if_fail_fe(logPath(log.Path, session, ".log"));
#ifdef LLC_WINDOWS
	if(!CreateDirectoryA("lls_logs", 0) && GetLastError() != ERROR_ALREADY_EXISTS) log.FileError = EACCES;
	if(!log.FileError) {
		log.File = _fsopen(log.Path.begin(), "ab", _SH_DENYNO);
		if(!log.File) log.FileError = errno ? errno : EIO;
	}
#else
	log.File = fopen(log.Path.begin(), "ab");
	if(!log.File) log.FileError = errno;
#endif
	if_fail_fe(llc::getLogCallbacks(log.PreviousPrint, log.PreviousWrite));
	Active = &log;
	log.Installed = true;
	if_fail_fe(llc::setupLogCallbacks(logPrint, logWrite));
	always_printf("--- LLS session started; diagnostics: %s; file status: %d ---", log.Path.begin(), log.FileError);
	rtrn 0;
}
llc::err_t llsLogSnapshot(SServiceLog & log, llc::string & output, int & fileError) {
	llc::astatic<char, 65536> snapshot = {};
	llc::u2_t count;
	{
		llc::lock_guard guard(log.Lock);
		count = log.Count;
		fileError = log.FileError;
		for(llc::u2_t i = 0; i < count; ++i) snapshot[i] = log.Bytes[(log.Start + i) % log.Bytes.size()];
	}
	rtrn output.append(snapshot.begin(), count);
}
llc::err_t llsLogOutput(llc::u3_t session, llc::u2_t job, llc::vcst_t output, llc::string & path) {
	llc::string suffix;
	if_fail_fe(llc::append_strings(suffix, "_job_", llc::str(job), ".stdout-stderr.log"));
	if_fail_fe(logPath(path, session, suffix));
	FILE * file = {};
#ifdef LLC_WINDOWS
	cnst int opened = fopen_s(&file, path.begin(), "wb");
	if_true_fe(opened);
#else
	file = fopen(path.begin(), "wb");
	if_null_fe(file);
#endif
	cnst bool failed = fwrite(output.begin(), 1, output.size(), file) != output.size();
	cnst int closed = fclose(file);
	if_true_fe(failed || closed);
	rtrn 0;
}
SServiceLog::~SServiceLog() {
	if(!Installed) rtrn;
	always_printf("--- LLS session stopped ---");
	llc::setupLogCallbacks(PreviousPrint, PreviousWrite);
	Active = {};
	if(File) fclose(File);
}
