#include "lls-l.h"

#include "llc_array_static.h"
#include "llc_json.h"
#include "llc_runtime.h"


LLC_USING_TYPEINT();
LLC_USING_APOD();
LLC_USING_VIEW();

sttc	llc::err_t	lls_entry_point		(llc::SRuntimeValues & runtimeValues);
LLC_SYSTEM_OS_ENTRY_POINT(::lls_entry_point);

sttc	llc::err_t	lls_entry_point		(llc::SRuntimeValues & runtimeValues) {
	(void)runtimeValues;
	llc::SJSONFile				json;
	llc::astsc_t<4096>			input;
	while(llc::u2_t size = (llc::u2_t)::fread(input.begin(), 1, input.size(), stdin)) {
		if_true_fe(100U * 1024U * 1024U - json.Bytes.size() < size);
		if_fail_fe(json.Bytes.append({input.begin(), size}));
	}
	return ::ferror(stdin) ? llc::OS_ERROR : llc::jsonParse(json.Reader, json.Bytes);
}
