#include "llc_path.h"
#include "llc_file.h"
#include "llc_string_compose.h"
#include "llc_string.h"
#include "llc_timer.h"
#include "llc_minmax.h"
#include "llc_args.h"
#include "llc_runtime.h"

sttc	::llc::err_t	ilc_entry_point		(::llc::SRuntimeValues & runtimeValues);
LLC_SYSTEM_OS_ENTRY_POINT(::ilc_entry_point);

LLC_USING_TYPEINT();
LLC_USING_APOD();
LLC_USING_VIEW();

sttc	::llc::err_t	ilc_entry_point		(::llc::SRuntimeValues & runtimeValues) {
	(void)runtimeValues;
	return 0;
}
