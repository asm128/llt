#include "lcp_metrics.h"
#include "llc_chrono.h"
#include "llc_string_compose.h"
#include <cerrno>
#ifdef LLC_WINDOWS
#include <share.h>
#endif

namespace {
SCommunicationMetrics * Active = {};

llc::vcst_t channelName(COMMUNICATION_CHANNEL channel) {
	switch(channel) {
	case COMMUNICATION_CHANNEL::Stdio  : rtrn "stdio";
	case COMMUNICATION_CHANNEL::Relay  : rtrn "relay";
	case COMMUNICATION_CHANNEL::Process: rtrn "process";
	default: rtrn "unknown";
	}
}

SCommunicationChannel * channelCounters(SCommunicationMetrics & metrics, COMMUNICATION_CHANNEL channel) {
	switch(channel) {
	case COMMUNICATION_CHANNEL::Stdio  : rtrn &metrics.Stdio;
	case COMMUNICATION_CHANNEL::Relay  : rtrn &metrics.Relay;
	case COMMUNICATION_CHANNEL::Process: rtrn &metrics.Process;
	default: rtrn {};
	}
}

llc::err_t writeRecord(SCommunicationMetrics & metrics, cnst SCommunicationSample & sample) {
	if(!metrics.File || metrics.FileError) rtrn 0;
	char line[256] = {};
	cnst int count = snprintf(line, llc::size(line), "%" LLC_FMT_U3 "|%.*s|%s|%" LLC_FMT_U3 "|%" LLC_FMT_U3 "|%" LLC_FMT_U3 "\n"
		, (llc::u3_t)llc::timeCurrentInUs(), LLC_VIEW_ARGS(channelName(sample.Channel)), sample.Direction == COMMUNICATION_DIRECTION::In ? "in" : "out"
		, sample.MessageBytes, sample.Bytes.In, sample.Bytes.Out
		);
	if(count <= 0 || count >= (int)llc::size(line) || fwrite(line, 1, count, metrics.File) != (size_t)count || fflush(metrics.File))
		metrics.FileError = errno ? errno : EIO;
	rtrn 0;
}

llc::err_t update(SCommunicationMetrics & metrics, COMMUNICATION_CHANNEL channel, bool input, llc::u3_t bytes) {
	SCommunicationChannel * counters = channelCounters(metrics, channel);
	if_null_fe(counters);
	llc::u3_t & totalBytes = input ? counters->Bytes.In : counters->Bytes.Out;
	totalBytes += bytes;
	cnst SCommunicationSample sample = {channel, input ? COMMUNICATION_DIRECTION::In : COMMUNICATION_DIRECTION::Out, counters->Bytes, bytes};
	rtrn writeRecord(metrics, sample);
}
}

llc::err_t lcpMetricsStart(SCommunicationMetrics & metrics, llc::vcst_t owner, llc::u3_t session) {
	if_true_fe(metrics.Installed || Active);
#ifdef LLC_WINDOWS
	if(!CreateDirectoryA("lls_logs", 0) && GetLastError() != ERROR_ALREADY_EXISTS) rtrn llc::OS_ERROR;
#endif
	llc::string relative;
	if_fail_fe(llc::append_strings(relative, "lls_logs/", owner, '_', llc::str(session), ".communications.log"));
#ifdef LLC_WINDOWS
	char full[32768] = {};
	cnst DWORD size = GetFullPathNameA(relative.begin(), (DWORD)llc::size(full), full, 0);
	if_true_fe(!size || size >= llc::size(full));
	if_fail_fe(metrics.Path.append(full, size));
	metrics.File = _fsopen(metrics.Path.begin(), "ab", _SH_DENYNO);
#else
	if_fail_fe(metrics.Path.append(relative));
	metrics.File = fopen(metrics.Path.begin(), "ab");
#endif
	if_null_ve(-(errno ? errno : EIO), metrics.File);
	cnst char header[] = "time|channel|direction|messageBytes|bytesIn|bytesOut\n";
	if(fwrite(header, 1, llc::size(header) - 1, metrics.File) != llc::size(header) - 1 || fflush(metrics.File)) {
		metrics.FileError = errno ? errno : EIO;
		fclose(metrics.File);
		metrics.File = {};
		rtrn -metrics.FileError;
	}
	Active = &metrics;
	metrics.Installed = true;
	rtrn 0;
}

llc::err_t lcpMetricsStop(SCommunicationMetrics & metrics) {
	llc::lock_guard guard(metrics.Lock);
	if(!metrics.Installed) rtrn 0;
	if(Active == &metrics) Active = {};
	cnst int closed = metrics.File ? fclose(metrics.File) : 0;
	metrics.File = {};
	metrics.Installed = false;
	if(closed && !metrics.FileError) metrics.FileError = errno ? errno : EIO;
	rtrn metrics.FileError ? -metrics.FileError : 0;
}

llc::err_t lcpMetricsRecord(COMMUNICATION_CHANNEL channel, bool input, llc::vcu0_t payload, llc::u3_t byteCount) {
	if(!Active || (!payload.size() && (byteCount == (llc::u3_t)-1 || !byteCount))) rtrn 0;
	SCommunicationMetrics & metrics = *Active;
	llc::lock_guard guard(metrics.Lock);
	rtrn update(metrics, channel, input, byteCount == (llc::u3_t)-1 ? payload.size() : byteCount);
}

SCommunicationMetrics::~SCommunicationMetrics() { lcpMetricsStop(*this); }
