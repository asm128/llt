#include "lls-l.h"

#include "llc_runtime.h"


LLC_USING_TYPEINT();
LLC_USING_APOD();
LLC_USING_VIEW();

sttc	llc::err_t	lls_entry_point		(llc::SRuntimeValues & runtimeValues);
LLC_SYSTEM_OS_ENTRY_POINT(::lls_entry_point);

sttc	llc::err_t	lls_entry_point		(llc::SRuntimeValues & runtimeValues) {
	(void)runtimeValues;
	return 0;
}
