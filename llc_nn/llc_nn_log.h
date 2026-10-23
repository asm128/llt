#ifndef LLC_NN_LOG_H
#define LLC_NN_LOG_H
#include "llc_array_static.h"
#include "llc_log.h"
#include <cstdio>
#include <cstring>
#include <cerrno>
#ifdef LLC_WINDOWS
#include <share.h>
#endif

// Callback boundary: fixed storage and CRT file I/O deliberately cannot log recursively.
stct SNNLog {
 ::llc::astatic<char, 65536> Bytes = {};
 ::llc::u2_t Start = 0, Count = 0;
 ::llc::u3_t Revision = 0;
 FILE * File = {};
 int FileError = 0;
 bool Alert = false, Armed = false, Installed = false;
 bool ErrorBody = false;
 ::llc::astchar<4096> LastError = {};
 ::llc::log_print_t PreviousPrint = {};
 ::llc::log_write_t PreviousWrite = {};
 ~SNNLog();
};

sttc SNNLog * nnLogActive = {};

sttc ::llc::err_t nnLogWrite(const char * text, ::llc::u2_t count) {
 if(!nnLogActive || !text) rtrn 0;
 auto & log = *nnLogActive;
 if(log.Armed && log.ErrorBody) {
  auto length = ::llc::min(count, log.LastError.size() - 1);
  while(length && (text[length - 1] == '\n' || text[length - 1] == '\r')) --length;
  memcpy(log.LastError.Storage, text, length);
  log.LastError[length] = 0;
 }
 for(::llc::u2_t i = 0; i < count; ++i) {
  log.Bytes[(log.Start + log.Count) % log.Bytes.size()] = text[i];
  if(log.Count == log.Bytes.size()) log.Start = (log.Start + 1) % log.Bytes.size();
  else ++log.Count;
 }
 ++log.Revision;
 if(log.File && !log.FileError) {
  const bool failed = fwrite(text, 1, count, log.File) != count;
  if(fflush(log.File) || failed) { log.FileError = errno ? errno : EIO; log.Alert = true; }
 }
 rtrn (::llc::err_t)count;
}

sttc ::llc::err_t nnLogPrint(const char * text) {
 if(!text) rtrn 0;
 // LLC emits severity and source through print, followed by the body through write.
 if(nnLogActive && text[0] && text[1] == '|') {
  nnLogActive->ErrorBody = false;
  const auto result = nnLogWrite(text, (::llc::u2_t)strlen(text));
  nnLogActive->ErrorBody = text[0] == '0' || text[0] == '1';
  if(nnLogActive->Armed && nnLogActive->ErrorBody) nnLogActive->Alert = true;
  rtrn result;
 }
 rtrn nnLogWrite(text, (::llc::u2_t)strlen(text));
}

sttc ::llc::err_t nnLogStart(SNNLog & log) {
 if_fail_fe(::llc::getLogCallbacks(log.PreviousPrint, log.PreviousWrite));
#ifdef LLC_WINDOWS
 log.File = ::_fsopen("NN_SESSION.log", "ab", _SH_DENYNO);
 if(!log.File) log.FileError = errno;
#else
 log.File = fopen("NN_SESSION.log", "ab");
 if(!log.File) log.FileError = errno;
#endif
 if(log.FileError) log.Alert = true;
 nnLogActive = &log;
 log.Installed = true;
 if_fail_fe(::llc::setupLogCallbacks(nnLogPrint, nnLogWrite));
 always_printf("--- Neural Lab session started; persistent log: NN_SESSION.log (working directory) ---");
 rtrn 0;
}

SNNLog::~SNNLog() {
 if(!Installed) rtrn;
 ::llc::log_print_t print = {}; ::llc::log_write_t write = {};
 ::llc::getLogCallbacks(print, write);
 if(print == nnLogPrint && write == nnLogWrite) ::llc::setupLogCallbacks(PreviousPrint, PreviousWrite);
 nnLogActive = {};
 if(File) fclose(File);
}
#endif
