#include "llc_json.h"
#include "llc_string.h"

#ifndef LCP_VS_H
#define LCP_VS_H

llc::err_t lcpVSBuildCommand(cnst llc::SJSONReader & reader, llc::string & application, llc::string & command);
llc::err_t lcpVSCall(cnst llc::SJSONReader & reader, llc::string & body, llc::err_t * operationStatus = 0);

#endif
