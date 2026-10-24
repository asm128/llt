#include "llc_array_static.h"
#include "llc_log.h"
#include "llc_string.h"
#include "llc_mutex.h"
#include <cstdio>

#ifndef LLS_LOG_H
#define LLS_LOG_H
// Adapted from llc_nn_log: callbacks cannot allocate or log recursively.
stct SServiceLog {
	llc::mutex Lock;
	llc::astatic<char, 65536> Bytes = {};
	llc::u2_t Start = 0, Count = 0;
	llc::u3_t Revision = 0;
	FILE * File = {};
	int FileError = 0;
	llc::string Path;
	llc::log_print_t PreviousPrint = {};
	llc::log_write_t PreviousWrite = {};
	bool Installed = false;
	~SServiceLog();
};
llc::err_t llsLogStart(SServiceLog & log, llc::u3_t session);
llc::err_t llsLogSnapshot(SServiceLog & log, llc::string & output, int & fileError);
llc::err_t llsLogOutput(llc::u3_t session, llc::u2_t job, llc::vcst_t output, llc::string & path);
#endif
