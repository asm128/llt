#include "llc_view.h"
#include "llc_array_obj.h"

#ifndef LCP_HELP_H
#define LCP_HELP_H

// Load at startup. Returned views remain valid until the next load.
llc::err_t helpLoad(llc::vcst_t fileName = "lcp_help.json");
llc::err_t helpPaths(llc::aobj<llc::vcst_t> & paths, bool service = false);
// Empty path opens the root; an unknown/unavailable topic returns an empty view.
::llc::vcst_t help(::llc::vcst_t path = {}, bool service = false);

#endif
