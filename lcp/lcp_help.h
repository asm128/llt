#include "llc_view.h"

#ifndef LCP_HELP_H
#define LCP_HELP_H

// Empty path opens the root; an unknown/unavailable topic returns an empty view.
::llc::vcst_t help(::llc::vcst_t path = {}, bool service = false);

#endif
