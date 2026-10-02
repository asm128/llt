#include "lls-l.h"

#include "llc_runtime.h"

LLC_USING_TYPEINT();
LLC_USING_APOD();
LLC_USING_VIEW();

sttc	llc::err_t	lls_terminal_entry_point		(llc::SRuntimeValues & runtimeValues);
LLC_SYSTEM_OS_ENTRY_POINT(::lls_terminal_entry_point);
// Yes. I'll enumerate the relevant sources you need to look into for previous likely useful features (in no particular order). 
sttc	sc_c		files[][256]	=
	{ "D:/dev_extras/llb/llt/dedup"
	, "D:/dev_extras/llb/llc/llc/llc_path.h"						// These should be improved/completed
	, "D:/dev_extras/llb/llc/llc/llc_file.h"						// These should be improved/completed
	, "D:/dev_extras/llb"
	, "D:/dev_extras/asm128/gpk/gpk/gpk_connection.*"
	, "D:/dev_extras/asm128/gpk/gpk/gpk_udp*.*"
	, "D:/dev_extras/asm128/gpk_samples/test_gpk_dialog"
	, "D:/dev_extras/asm128/gpk_samples/test_gpk_png"
	, "D:/dev_extras/asm128/gpk_samples/test_gpk_json"
	, "D:/dev_extras/asm128/gpk_samples/test_xml_reader"
	, "D:/dev_extras/asm128/gpk_samples/test_gpk_vox"
	, "D:/dev_extras/asm128/gpk_samples/test_vox_loader"
	, "D:/dev_extras/asm128/gpk_games/ssiege.server.win32"
	, "D:/dev_extras/asm128/gpk_games/ssiege.client.win32"
	, "D:/dev_extras/asm128/blitter"
	, "D:/dev_extras/asm128/demo"
	, "D:/dev_extras/tuobelisco"
	, "D:/dev_extras/profile"
	, "D:/dev_extras/mlc/src"
	, "D:/dev_extras/alc/src"
	, "D:/dev_extras/asm128/battleground"
	};
