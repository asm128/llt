#include "llc_json.h"
#include "llc_path.h"

#ifndef LCP_COMMANDS_H
#define LCP_COMMANDS_H
llc::err_t lcpTreeCall(cnst llc::SJSONReader & reader, llc::string & body, llc::SPathTree & tree);
using FCommandExecute = llc::function<llc::err_t(llc::vcst_t, llc::vcu0_t, llc::au0_t &, llc::u2_t &)>;
llc::err_t lcpShellCall(cnst llc::SJSONReader & reader, llc::string & body, llc::vcst_t shell, cnst FCommandExecute & execute = {});
#endif
