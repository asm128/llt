#include "llc_framework.h"
#include "llc_dialog.h"
#include "llc_dialog_controls.h"
#include "llc_gui_inputbox.h"
#include "llc_gui_control_list.h"
#include "llc_mutex.h"
#include "blitter.h"

#ifndef BLITTER_DASHBOARD_H_23627
#define BLITTER_DASHBOARD_H_23627
namespace blt {
	LLC_USING_TYPEINT();
	enum DASHBOARD_INPUT_MODE : u0_t { DASHBOARD_INPUT_Config, DASHBOARD_INPUT_Record };
	enum DASHBOARD_COMMAND : s2_t {
		DASHBOARD_COMMAND_Config, DASHBOARD_COMMAND_Record, DASHBOARD_COMMAND_Load, DASHBOARD_COMMAND_Refresh,
		DASHBOARD_COMMAND_Browse, DASHBOARD_COMMAND_Detail, DASHBOARD_COMMAND_Model, DASHBOARD_COMMAND_Append,
		DASHBOARD_COMMAND_Flush, DASHBOARD_COMMAND_Exit, DASHBOARD_COMMAND_Count,
	};
	struct SBlitterDashboard {
		::llc::mutex LockRender = {};
		::llc::SFramework Framework;
		::llc::pobj<::llc::rtbgra8d32> Offscreen = {};
		::llc::SDialog DialogMain = {};
		::llc::SInputBox Editor = {};
		::llc::SControlList Commands = {}, DatabaseRows = {}, OutputRows = {};
		::llc::pnco<::llc::SDialogTuner<u3_t>> TunerOffset = {}, TunerCount = {}, TunerDetail = {};
		::llc::cid_t IdTitle = ::llc::CID_INVALID, IdStatus = ::llc::CID_INVALID, IdInputMode = ::llc::CID_INVALID;
		::llc::cid_t ViewportDatabases = ::llc::CID_INVALID, ViewportOutput = ::llc::CID_INVALID;
		::llc::string StatusText = {}, ConfigPath = "./blitter.json", RecordText = "{}", OutputText = {};
		::llc::aobj<::llc::string> DatabaseRowText = {};
		::llc::aobj<::llc::vcst_t> OutputTextRows = {};
		::blt::SBlitter Blitter = {};
		DASHBOARD_INPUT_MODE InputMode = DASHBOARD_INPUT_Config;
		s2_t SelectedDatabase = -1;
		SBlitterDashboard(::llc::SRuntimeValues & runtimeValues) : Framework(runtimeValues) {}
	};
}
#endif
