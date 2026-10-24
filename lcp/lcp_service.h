#include "llc_json.h"
#include "llc_string.h"

#ifndef LCP_SERVICE_H
#define LCP_SERVICE_H

llc::err_t lcpServiceCall(cnst llc::SJSONReader & reader, llc::string & body);
llc::err_t lcpServiceControl(llc::vcst_t action, llc::vcst_t configuration, llc::vcst_t executable, llc::string & text);

#endif
