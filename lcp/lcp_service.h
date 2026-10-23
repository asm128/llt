#include "llc_json.h"
#include "llc_string.h"

#ifndef LCP_SERVICE_H
#define LCP_SERVICE_H

llc::err_t lcpServiceCall(cnst llc::SJSONReader & reader, llc::string & body);

#endif
