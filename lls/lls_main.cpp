#include "llc_json.h"
#include "llc_runtime.h"

sttc	llc::err_t	lls_entry_point		(llc::SRuntimeValues & runtimeValues);
LLC_OS_ENTRY_POINT(::lls_entry_point);


sttc	llc::err_t	lls_entry_point		(llc::SRuntimeValues & runtimeValues) {
	(void)runtimeValues;
	rtrn 0;
}
