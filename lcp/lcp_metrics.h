#include "llc_mutex.h"
#include "llc_string.h"
#include "llc_inout.h"
#include <cstdio>

#ifndef LCP_METRICS_H
#define LCP_METRICS_H

enum class COMMUNICATION_CHANNEL : llc::u0_t {
	Stdio,
	Relay,
	Process,
};

enum class COMMUNICATION_DIRECTION : llc::u0_t {
	In,
	Out,
};

stct SCommunicationChannel {
	COMMUNICATION_CHANNEL	Channel = {};
	llc::inout<llc::u3_t>	Bytes;
};

stct SCommunicationSample {
	COMMUNICATION_CHANNEL	Channel = {};
	COMMUNICATION_DIRECTION Direction = {};
	llc::inout<llc::u3_t>	Bytes;
	llc::u3_t			MessageBytes = {};
};

stct SCommunicationMetrics {
	llc::mutex			Lock;
	SCommunicationChannel	Stdio = {COMMUNICATION_CHANNEL::Stdio};
	SCommunicationChannel	Relay = {COMMUNICATION_CHANNEL::Relay};
	SCommunicationChannel	Process = {COMMUNICATION_CHANNEL::Process};
	FILE *				File = {};
	int				FileError = 0;
	llc::string			Path;
	bool				Installed = false;
					~SCommunicationMetrics();
};

llc::err_t lcpMetricsStart(SCommunicationMetrics & metrics, llc::vcst_t owner, llc::u3_t session);
llc::err_t lcpMetricsStop(SCommunicationMetrics & metrics);
llc::err_t lcpMetricsRecord(COMMUNICATION_CHANNEL channel, bool input, llc::vcu0_t payload, llc::u3_t byteCount = (llc::u3_t)-1);
stin llc::err_t lcpMetricsRecord(COMMUNICATION_CHANNEL channel, bool input, llc::vcst_t payload, llc::u3_t byteCount = (llc::u3_t)-1) {
	rtrn lcpMetricsRecord(channel, input, {(cnst llc::u0_t*)payload.begin(), payload.size()}, byteCount);
}

#endif
