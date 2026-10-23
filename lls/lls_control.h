#include "llc_typeint.h"

#ifndef LLS_CONTROL_H
#define LLS_CONTROL_H

namespace lls {

stct SControlNames {
	cnst char * Instance;
	cnst char * Window;
	cnst char * Operation;
};
stxp SControlNames ControlDebug   = {"Global\\LT4.LLS.Debug.Instance",   "LT4.LLS.Debug.Control",   "Global\\LT4.LLS.Debug.Operation"};
stxp SControlNames ControlRelease = {"Global\\LT4.LLS.Release.Instance", "LT4.LLS.Release.Control", "Global\\LT4.LLS.Release.Operation"};
stxp char ShutdownMessage[] = "LT4.LLS.Shutdown.v1";

} // namespace lls
#endif
