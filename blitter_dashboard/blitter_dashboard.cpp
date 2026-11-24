#include "blitter_dashboard.h"
#include "llc_bitmap_target.h"
#include "llc_path.h"
#include "llc_string.h"
#include "llc_view_bit.h"
#include "llc_app_impl.h"
#ifdef LLC_WINDOWS
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")
#endif

LLC_DEFINE_APPLICATION_ENTRY_POINT(::blt::SBlitterDashboard, "Blitter Dashboard");
using namespace ::llc;

namespace {
	struct SDashboardCommand { ::llc::vcst_t Label; ::blt::DASHBOARD_COMMAND Command; };
	static const SDashboardCommand DASHBOARD_COMMANDS[] = {
		{"Config", ::blt::DASHBOARD_COMMAND_Config}, {"Record JSON", ::blt::DASHBOARD_COMMAND_Record}, {"Load", ::blt::DASHBOARD_COMMAND_Load},
		{"Refresh", ::blt::DASHBOARD_COMMAND_Refresh}, {"Browse", ::blt::DASHBOARD_COMMAND_Browse}, {"Detail", ::blt::DASHBOARD_COMMAND_Detail},
		{"Model", ::blt::DASHBOARD_COMMAND_Model}, {"Append", ::blt::DASHBOARD_COMMAND_Append}, {"Flush", ::blt::DASHBOARD_COMMAND_Flush}, {"Exit", ::blt::DASHBOARD_COMMAND_Exit},
	};
	static ::llc::vcst_t hostLabel(::blt::DATABASE_HOST host) {
		switch(host) { default: case ::blt::DATABASE_HOST_LOCAL: return "local"; case ::blt::DATABASE_HOST_REMOTE: return "remote"; case ::blt::DATABASE_HOST_DEFLATE: return "local+deflate"; case ::blt::DATABASE_HOST_REMOTE_DEFLATE: return "remote+deflate"; }
	}
	static ::llc::u2_t databaseDirtyCount(const ::blt::SDatabase & db) {
		if(!db.BlockDirty.size()) return 0;
		const ::llc::view_bit<const ::llc::u2_t> dirty = {db.BlockDirty.begin(), db.Blocks.size()};
		::llc::u2_t count = 0; for(::llc::u2_t i = 0; i < db.Blocks.size(); ++i) count += dirty[i] ? 1 : 0; return count;
	}
	static void dashboardControlThemeSet(::blt::SBlitterDashboard & app, ::llc::cid_t id, ::llc::ASCII_COLOR color) {
		::llc::SControlDraw & draw = app.Framework.GUI->Controls.Draw[id]; draw.UseNewPalettes = false; draw.ColorMode = ::llc::GUI_COLOR_MODE_Flat; draw.ColorTheme = (::llc::s1_t)(color * 16 + 12 + 1);
	}
	static void dashboardRowThemeSet(::blt::SBlitterDashboard & app, ::llc::cid_t id, ::llc::ASCII_COLOR color, bool staticLabel) {
		::llc::SControlTable & controls = app.Framework.GUI->Controls; ::dashboardControlThemeSet(app, id, color);
		const ::llc::GUI_CONTROL_FLAG interactive = (::llc::GUI_CONTROL_FLAG)(::llc::GUI_CONTROL_FLAG_Hovered | ::llc::GUI_CONTROL_FLAG_Pressed | ::llc::GUI_CONTROL_FLAG_Action);
		if(staticLabel) { controls.States[id].Flag &= ~interactive; controls.States[id].Mask &= ~interactive; }
		else controls.States[id].Mask |= interactive;
		controls.SetDisabled(id, false);
	}
	static void dashboardTunerThemeSet(::blt::SBlitterDashboard & app, const ::llc::SDialogTuner<::llc::u3_t> & tuner, ::llc::ASCII_COLOR color) {
		::dashboardControlThemeSet(app, tuner.IdGUIControl, color); ::dashboardControlThemeSet(app, tuner.IdDecrease, color); ::dashboardControlThemeSet(app, tuner.IdIncrease, color);
	}
	static ::llc::err_t statusSet(::blt::SBlitterDashboard & app, ::llc::vcst_t text) { app.StatusText = text; return ::llc::controlTextSet(*app.Framework.GUI, app.IdStatus, app.StatusText); }
	static ::llc::err_t configurationExamplePath(::llc::string & path) {
		if_fail_fe(path.clear());
#ifdef LLC_WINDOWS
		char modulePath[32768] = {}; const DWORD moduleLength = ::GetModuleFileNameA(nullptr, modulePath, (DWORD)::llc::size(modulePath)); if_zero_fef(moduleLength && moduleLength < ::llc::size(modulePath), "GetModuleFileName error:%u. Length:%u.", ::GetLastError(), moduleLength);
		::llc::vcst_t moduleDirectory = {}; if_fail_fe(::llc::pathDirectory({modulePath, moduleLength}, moduleDirectory)); return ::llc::pathNameCompose(moduleDirectory, "blitter.json", path);
#else
		path = "./blitter.json"; return 0;
#endif
	}
	static ::llc::err_t editorTextStore(::blt::SBlitterDashboard & app) {
		::llc::string & target = app.InputMode == ::blt::DASHBOARD_INPUT_Config ? app.ConfigPath : app.RecordText;
		if_fail_fe(target.clear()); return target.append({app.Editor.Text.begin(), app.Editor.Text.size()});
	}
	static ::llc::err_t inputModeSet(::blt::SBlitterDashboard & app, ::blt::DASHBOARD_INPUT_MODE mode) {
		if_fail_fe(::editorTextStore(app)); app.InputMode = mode; ::llc::SGUI & gui = *app.Framework.GUI;
		const ::llc::string & source = mode == ::blt::DASHBOARD_INPUT_Config ? app.ConfigPath : app.RecordText;
		if_fail_fe(app.Editor.SetText(gui, {source.begin(), source.size()}));
		if_fail_fe(::llc::controlTextSet(gui, app.IdInputMode, mode == ::blt::DASHBOARD_INPUT_Config ? ::llc::vcst_t{"INPUT: CONFIGURATION PATH"} : ::llc::vcst_t{"INPUT: JSON RECORD"}));
		return ::statusSet(app, mode == ::blt::DASHBOARD_INPUT_Config ? ::llc::vcst_t{"Configuration input selected. Edit the path and press Load."} : ::llc::vcst_t{"Record input selected. Enter one JSON object and press Append."});
	}
	static ::llc::err_t configurationSelect(::blt::SBlitterDashboard & app) {
#ifdef LLC_WINDOWS
		if_fail_fe(::inputModeSet(app, ::blt::DASHBOARD_INPUT_Config)); ::llc::string examplePath = {}; if_fail_fe(::configurationExamplePath(examplePath));
		::llc::vcst_t exampleDirectory = {}; if_fail_fe(::llc::pathDirectory(examplePath, exampleDirectory)); char initialDirectory[32768] = {}, path[32768] = {};
		if_fail_fe(::llc::sprintf_s(initialDirectory, "%.*s", LLC_VIEW_ARGS(exampleDirectory))); if_fail_fe(::llc::sprintf_s(path, "%.*s", LLC_VIEW_ARGS(examplePath)));
		OPENFILENAMEA dialog = {}; dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = app.Framework.RootWindow.PlatformDetail.WindowHandle; dialog.lpstrFilter = "Blitter configuration\0*.json\0JSON files\0*.json\0All files\0*.*\0\0"; dialog.lpstrFile = path; dialog.nMaxFile = (DWORD)::llc::size(path); dialog.lpstrInitialDir = initialDirectory; dialog.lpstrTitle = "Open Blitter configuration"; dialog.lpstrDefExt = "json"; dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
		if(!::GetOpenFileNameA(&dialog)) { const DWORD error = ::CommDlgExtendedError(); if_true_fef(error, "Blitter configuration picker failed: Windows dialog error 0x%lX.", error); return ::statusSet(app, "Configuration selection canceled."); }
		if_fail_fe(app.ConfigPath.clear()); if_fail_fe(app.ConfigPath.append(::llc::vcst_t{path, (::llc::u2_t)-1})); if_fail_fe(app.Editor.SetText(*app.Framework.GUI, app.ConfigPath)); return ::statusSet(app, "Configuration selected. Press Load to open it.");
#else
		(void)app; return -1;
#endif
	}
	static ::llc::err_t outputRowsUpdate(::blt::SBlitterDashboard & app) {
		::llc::SGUI & gui = *app.Framework.GUI; static const ::llc::vcst_t emptyText = "No result yet.";
		::llc::pnco<::llc::SDialogViewport> viewport = {}; if_fail_fe(app.DialogMain.Get(app.ViewportOutput, viewport));
		const ::llc::s1_t listWidth = ::llc::max((::llc::s1_t)200, (::llc::s1_t)(gui.Controls.Placement[viewport->IdGUIControl].Area.Size.x - 16));
		const ::llc::u2_t charWidth = gui.Fonts.size() > gui.SelectedFont && gui.Fonts[gui.SelectedFont] ? gui.Fonts[gui.SelectedFont]->CharSize.x : 8;
		const ::llc::u2_t columns = ::llc::max(1U, (::llc::u2_t)(listWidth - 16) / charWidth);
		if_fail_fe(app.OutputTextRows.clear());
		const ::llc::vcst_t output = app.OutputText.size() ? ::llc::vcst_t{app.OutputText.begin(), app.OutputText.size()} : emptyText;
		for(::llc::u2_t column = 0; column < output.size(); column += columns) {
			::llc::vcst_t part = {};
			if_fail_fe(output.slice(part, column, ::llc::min(columns, output.size() - column)));
			if_fail_fe(app.OutputTextRows.push_back(part));
		}
		if(!app.OutputTextRows.size()) if_fail_fe(app.OutputTextRows.push_back(emptyText));
		const ::llc::u2_t rowCount = (::llc::u2_t)::llc::max(1U, app.OutputTextRows.size());
		for(::llc::u2_t i = 0; i < rowCount; ++i) {
			const ::llc::vcst_t text = app.OutputTextRows.size() ? app.OutputTextRows[i] : emptyText;
			if(i >= app.OutputRows.IdControls.size()) if_fail_fe(::llc::controlListPush(gui, app.OutputRows, text));
			if_fail_fe(::llc::controlTextSet(gui, app.OutputRows.IdControls[i], text)); ::dashboardRowThemeSet(app, app.OutputRows.IdControls[i], ::llc::ASCII_COLOR_Dark_blue, true); gui.Controls.SetHidden(app.OutputRows.IdControls[i], false);
		}
		for(::llc::u2_t i = rowCount; i < app.OutputRows.IdControls.size(); ++i) gui.Controls.SetHidden(app.OutputRows.IdControls[i], true);
		if_fail_fe(::llc::controlListArrange(gui, app.OutputRows)); gui.Controls.Placement[app.OutputRows.IdControl].Area.Size.x = listWidth; return 0;
	}
	static ::llc::err_t outputSet(::blt::SBlitterDashboard & app, ::llc::vcst_t text) { app.OutputText = text; return ::outputRowsUpdate(app); }
	static ::llc::err_t databaseRowsUpdate(::blt::SBlitterDashboard & app) {
		::llc::SGUI & gui = *app.Framework.GUI; const ::llc::u2_t rowCount = (::llc::u2_t)::llc::max(1U, app.Blitter.Databases.size()) + 1;
		if(app.SelectedDatabase >= (::llc::s2_t)app.Blitter.Databases.size()) app.SelectedDatabase = app.Blitter.Databases.size() ? 0 : -1;
		if_fail_fe(app.DatabaseRowText.resize(rowCount)); app.DatabaseRowText[0] = "DATABASE                 HOST               BLOCK   MEMORY   DISK  DIRTY  FIELDS  PUBLIC  ENCRYPTED";
		if(!app.Blitter.Databases.size()) app.DatabaseRowText[1] = "No Blitter configuration loaded.";
		else for(::llc::u2_t i = 0; i < app.Blitter.Databases.size(); ++i) {
			const ::blt::TNamedBlitterDB & named = app.Blitter.Databases[i]; const ::blt::SDatabase & db = named.Val; char line[512] = {};
			::sprintf_s(line, sizeof(line), "%-24.*s %-18.*s %7u %8u %6u %6u %7u %7s %10s", LLC_VIEW_ARGS(named.Key), LLC_VIEW_ARGS(::hostLabel(db.HostType)), (unsigned)db.BlockSize, (unsigned)db.Blocks.size(), (unsigned)db.BlocksOnDisk.size(), (unsigned)::databaseDirtyCount(db), (unsigned)db.Description.Fields.size(), db.Description.Public ? "yes" : "no", db.EncryptionKey.size() ? "yes" : "no");
			app.DatabaseRowText[i + 1] = line;
		}
		for(::llc::u2_t i = 0; i < rowCount; ++i) {
			if(i >= app.DatabaseRows.IdControls.size()) {
				if_fail_fe(::llc::controlListPush(gui, app.DatabaseRows, app.DatabaseRowText[i]));
			}
			else {
				if_fail_fe(::llc::controlTextSet(gui, app.DatabaseRows.IdControls[i], app.DatabaseRowText[i]));
			}
			::dashboardRowThemeSet(app, app.DatabaseRows.IdControls[i], ::llc::ASCII_COLOR_Dark_cyan, i == 0 || !app.Blitter.Databases.size()); gui.Controls.SetHidden(app.DatabaseRows.IdControls[i], false); gui.Controls.SetSelected(app.DatabaseRows.IdControls[i], i > 0 && app.SelectedDatabase == (::llc::s2_t)i - 1);
		}
		for(::llc::u2_t i = rowCount; i < app.DatabaseRows.IdControls.size(); ++i) gui.Controls.SetHidden(app.DatabaseRows.IdControls[i], true);
		::llc::pnco<::llc::SDialogViewport> viewport = {}; if_fail_fe(app.DialogMain.Get(app.ViewportDatabases, viewport));
		if_fail_fe(::llc::controlListArrange(gui, app.DatabaseRows)); gui.Controls.Placement[app.DatabaseRows.IdControl].Area.Size.x = ::llc::max((::llc::s1_t)200, (::llc::s1_t)(gui.Controls.Placement[viewport->IdGUIControl].Area.Size.x - 16)); return 0;
	}
	static ::llc::err_t dashboardPlace(::llc::SGUI & gui, ::llc::cid_t id, ::llc::s1_t x, ::llc::s1_t y, ::llc::s1_t width, ::llc::s1_t height) {
		gui.Controls.Placement[id].Area = {{x, y}, {width, height}}; return ::llc::controlMetricsInvalidate(gui, id);
	}
	static ::llc::err_t dashboardLayout(::blt::SBlitterDashboard & app) {
		::llc::SGUI & gui = *app.Framework.GUI; ::llc::SControlTable & controls = gui.Controls; const ::llc::n2u1_t size = app.Framework.RootWindow.Size;
		const ::llc::s1_t width = (::llc::s1_t)::llc::max(800U, (::llc::u2_t)size.x), height = (::llc::s1_t)::llc::max(700U, (::llc::u2_t)size.y);
		const ::llc::s1_t margin = 24, contentWidth = width - margin * 2, editorReserve = ::llc::min((::llc::s1_t)240, (::llc::s1_t)(height / 3));
		const ::llc::s1_t databaseTop = 116, contentBottom = height - editorReserve - 18, available = ::llc::max((::llc::s1_t)320, (::llc::s1_t)(contentBottom - databaseTop - 52));
		const ::llc::s1_t databaseHeight = ::llc::max((::llc::s1_t)150, (::llc::s1_t)(available * 45 / 100)), tunerY = databaseTop + databaseHeight + 14;
		const ::llc::s1_t outputY = tunerY + 40, outputHeight = ::llc::max((::llc::s1_t)150, (::llc::s1_t)(contentBottom - outputY));
		const ::llc::s1_t tunerWidth = ::llc::max((::llc::s1_t)120, ::llc::min((::llc::s1_t)220, (::llc::s1_t)((contentWidth - 48) / 4)));
		::llc::pnco<::llc::SDialogViewport> databaseViewport = {}, outputViewport = {};
		if_fail_fe(app.DialogMain.Get(app.ViewportDatabases, databaseViewport)); if_fail_fe(app.DialogMain.Get(app.ViewportOutput, outputViewport));
		controls.Placement[app.DialogMain.Root].Area = {{}, {width, height}}; if_fail_fe(::llc::controlMetricsInvalidate(gui, app.DialogMain.Root));
		if_fail_fe(::dashboardPlace(gui, app.IdTitle, margin, 18, contentWidth, 24)); if_fail_fe(::dashboardPlace(gui, app.IdStatus, margin, 82, contentWidth, 22));
		controls.Placement[databaseViewport->IdGUIControl].Area = {{0, databaseTop}, {contentWidth, databaseHeight}}; if_fail_fe(::llc::controlMetricsInvalidate(gui, databaseViewport->IdGUIControl));
		if_fail_fe(::dashboardPlace(gui, app.TunerOffset->IdGUIControl, margin, tunerY, tunerWidth, 24));
		if_fail_fe(::dashboardPlace(gui, app.TunerCount->IdGUIControl, margin + tunerWidth + 16, tunerY, tunerWidth, 24));
		if_fail_fe(::dashboardPlace(gui, app.TunerDetail->IdGUIControl, margin + (tunerWidth + 16) * 2, tunerY, tunerWidth, 24));
		const ::llc::s1_t inputX = margin + (tunerWidth + 16) * 3; if_fail_fe(::dashboardPlace(gui, app.IdInputMode, inputX, tunerY, width - margin - inputX, 24));
		controls.Placement[outputViewport->IdGUIControl].Area = {{0, outputY}, {contentWidth, outputHeight}}; if_fail_fe(::llc::controlMetricsInvalidate(gui, outputViewport->IdGUIControl));
		controls.Placement[app.Editor.IdRoot].Area.Offset = {0, 16}; if_fail_fe(::llc::controlMetricsInvalidate(gui, app.Editor.IdRoot));
		app.LayoutSize = size; if_fail_fe(::databaseRowsUpdate(app)); return ::outputRowsUpdate(app);
	}
	static ::llc::err_t selectedDatabaseSet(::blt::SBlitterDashboard & app, ::llc::u2_t index) {
		if(index >= app.Blitter.Databases.size()) return ::statusSet(app, "Select a loaded database row."); app.SelectedDatabase = (::llc::s2_t)index; if_fail_fe(::databaseRowsUpdate(app));
		const ::blt::TNamedBlitterDB & selected = app.Blitter.Databases[index]; char status[512] = {};
		::sprintf_s(status, sizeof(status), "Selected %.*s: %u fields, block size %u, %u block(s) on disk.", LLC_VIEW_ARGS(selected.Key), (unsigned)selected.Val.Description.Fields.size(), (unsigned)selected.Val.BlockSize, (unsigned)selected.Val.BlocksOnDisk.size()); return ::statusSet(app, status);
	}
	static ::llc::err_t configurationLoad(::blt::SBlitterDashboard & app) {
		if(app.InputMode == ::blt::DASHBOARD_INPUT_Config) if_fail_fe(::editorTextStore(app)); ::llc::vcst_t path = app.ConfigPath; ::llc::trim(path); if(!path.size()) return ::statusSet(app, "Enter a Blitter configuration path before loading.");
		const ::llc::err_t result = ::blt::loadConfig(app.Blitter, path); char status[512] = {};
		if(::llc::failed(result)) ::sprintf_s(status, sizeof(status), "Load failed (%i): %.*s", result, LLC_VIEW_ARGS(path));
		else { app.SelectedDatabase = app.Blitter.Databases.size() ? 0 : -1; ::sprintf_s(status, sizeof(status), "Loaded %u database(s) from %.*s. Database root: %.*s", (unsigned)app.Blitter.Databases.size(), LLC_VIEW_ARGS(path), LLC_VIEW_ARGS(app.Blitter.Folder)); }
		if_fail_fe(::statusSet(app, status)); if_fail_fe(::databaseRowsUpdate(app));
		return ::outputSet(app, ::llc::failed(result) ? ::llc::vcst_t{"Configuration load failed. See status and process log for diagnostics."} : ::llc::vcst_t{"Configuration loaded. Select a database, then Browse, Detail, Model, or enter a JSON record and Append."});
	}
	static ::llc::err_t queryExecute(::blt::SBlitterDashboard & app, ::llc::vcst_t command, bool detail = false) {
		if(app.SelectedDatabase < 0 || app.SelectedDatabase >= (::llc::s2_t)app.Blitter.Databases.size()) return ::statusSet(app, "Load a configuration and select a database first.");
		::blt::SBlitterQuery query = {}; const ::blt::TNamedBlitterDB & selected = app.Blitter.Databases[(::llc::u2_t)app.SelectedDatabase];
		query.Database = selected.Key; query.Command = command; query.Range = {app.TunerOffset->ValueCurrent, app.TunerCount->ValueCurrent}; if(detail) query.Detail = (::llc::s3_t)app.TunerDetail->ValueCurrent;
		if(command == ::llc::vcst_t{"push_back"}) {
			if(app.InputMode != ::blt::DASHBOARD_INPUT_Record) { if_fail_fe(::inputModeSet(app, ::blt::DASHBOARD_INPUT_Record)); return ::statusSet(app, "Record input selected. Review the JSON object, then press Append again."); }
			if_fail_fe(::editorTextStore(app)); ::llc::vcst_t record = app.RecordText; ::llc::trim(record); if(!record.size()) return ::statusSet(app, "Enter one JSON object before appending."); query.Record = record;
			const ::llc::err_t parsed = ::llc::jsonParse(query.RecordReader, query.Record, ::llc::JSON_PARSE_STRICT);
			if(::llc::failed(parsed) || !query.RecordReader.Token.size() || query.RecordReader.Token[0].Type != ::llc::JSON_TYPE_OBJECT) return ::statusSet(app, "Append rejected: the input must be one valid JSON object.");
		}
		::llc::string output; const ::llc::s3_t result = ::blt::queryProcess(app.Blitter.LoadCache, app.Blitter.Databases, app.Blitter.ExpressionReader, query, app.Blitter.Folder, output);
		if(::llc::failed(result)) { char status[256] = {}; ::sprintf_s(status, sizeof(status), "%.*s failed for %.*s (%lli).", LLC_VIEW_ARGS(command), LLC_VIEW_ARGS(query.Database), (long long)result); return ::statusSet(app, status); }
		char status[512] = {};
		if(command == ::llc::vcst_t{"push_back"}) { ::sprintf_s(status, sizeof(status), "Appended record %llu to %.*s. Flush to persist dirty blocks.", (unsigned long long)result, LLC_VIEW_ARGS(query.Database)); char text[512] = {}; ::sprintf_s(text, sizeof(text), "{\"database\":\"%.*s\",\"record\":%llu,\"dirty\":true}", LLC_VIEW_ARGS(query.Database), (unsigned long long)result); if_fail_fe(::outputSet(app, text)); }
		else { ::sprintf_s(status, sizeof(status), "%.*s completed for %.*s: %u output byte(s).", LLC_VIEW_ARGS(command), LLC_VIEW_ARGS(query.Database), (unsigned)output.size()); if_fail_fe(::outputSet(app, output)); }
		if_fail_fe(::statusSet(app, status)); return ::databaseRowsUpdate(app);
	}
	static ::llc::err_t databasesFlush(::blt::SBlitterDashboard & app) {
		if(!app.Blitter.Databases.size()) return ::statusSet(app, "No loaded databases to flush."); const ::llc::err_t result = ::blt::blitterFlush(app.Blitter); char status[256] = {};
		if(::llc::failed(result)) ::sprintf_s(status, sizeof(status), "Flush failed (%i).", result); else ::sprintf_s(status, sizeof(status), "Flushed %u database(s).", (unsigned)app.Blitter.Databases.size()); if_fail_fe(::statusSet(app, status)); return ::databaseRowsUpdate(app);
	}
	static ::llc::err_t dashboardCommand(::blt::SBlitterDashboard & app, ::llc::cid_t id) {
		for(::llc::u2_t i = 0; i < app.Commands.IdControls.size(); ++i) if(id == app.Commands.IdControls[i]) switch((::blt::DASHBOARD_COMMAND)app.Commands.Events[i]) {
		default: return 0; case ::blt::DASHBOARD_COMMAND_Config: return ::inputModeSet(app, ::blt::DASHBOARD_INPUT_Config); case ::blt::DASHBOARD_COMMAND_Record: return ::inputModeSet(app, ::blt::DASHBOARD_INPUT_Record);
		case ::blt::DASHBOARD_COMMAND_Load: return ::configurationLoad(app); case ::blt::DASHBOARD_COMMAND_Refresh: return ::databaseRowsUpdate(app); case ::blt::DASHBOARD_COMMAND_Browse: return ::queryExecute(app, "get"); case ::blt::DASHBOARD_COMMAND_Detail: return ::queryExecute(app, "get", true); case ::blt::DASHBOARD_COMMAND_Model: return ::queryExecute(app, "model"); case ::blt::DASHBOARD_COMMAND_Append: return ::queryExecute(app, "push_back"); case ::blt::DASHBOARD_COMMAND_Flush: return ::databasesFlush(app); case ::blt::DASHBOARD_COMMAND_Exit: return ::llc::APPLICATION_STATE_EXIT; }
		return 0;
	}
}

::llc::err_t cleanup(::blt::SBlitterDashboard & app) { return ::llc::mainWindowDestroy(app.Framework.RootWindow); }
::llc::err_t setup(::blt::SBlitterDashboard & app) {
	::llc::SFramework & framework = app.Framework; ::llc::SWindow & window = framework.RootWindow; framework.GUI = app.DialogMain.GUI; app.DialogMain.Input = window.Input; window.Size = {1600, 1000}; if_true_e(::llc::failed(::llc::mainWindowCreate(window, framework.RuntimeValues.PlatformDetail, window.Input)));
	::llc::SGUI & gui = *framework.GUI; ::llc::SControlTable & controls = gui.Controls; gui.ColorModeDefault = ::llc::GUI_COLOR_MODE_Flat; gui.ThemeDefault = ::llc::ASCII_COLOR_Dark_grey * 16 + ::llc::ASCII_COLOR_White;
	if_fail_fe(app.IdTitle = ::llc::controlCreate(gui)); controls.Placement[app.IdTitle].Area = {{24, 18}, {720, 24}}; controls.Text[app.IdTitle].Align = ::llc::ALIGN_CENTER_LEFT; if_fail_fe(::llc::controlTextSet(gui, app.IdTitle, "BLITTER DATABASE MANAGER"));
	if_fail_fe(::llc::controlListInitialize(gui, app.Commands)); app.Commands.Orientation = ::llc::DIRECTION_RIGHT; controls.Placement[app.Commands.IdControl].Area.Offset = {24, 52}; for(const SDashboardCommand & c : DASHBOARD_COMMANDS) if_fail_fe(::llc::controlListPush(gui, app.Commands, c.Label, c.Command));
	if_fail_fe(app.IdStatus = ::llc::controlCreate(gui)); controls.Placement[app.IdStatus].Area = {{24, 82}, {1540, 22}}; controls.Text[app.IdStatus].Align = ::llc::ALIGN_CENTER_LEFT;
	::llc::pobj<::llc::SDialogViewport> dbView = {}; if_fail_fe(app.ViewportDatabases = ::llc::viewportCreate(app.DialogMain, dbView)); controls.Placement[dbView->IdGUIControl].Area.Size = {1540, 300}; controls.Placement[dbView->IdGUIControl].Area.Offset = {0, 116}; controls.Placement[dbView->IdGUIControl].Align = ::llc::ALIGN_CENTER_TOP; if_fail_fe(::llc::controlTextSet(gui, dbView->IdTitle, "DATABASES - click a row to select"));
	if_fail_fe(::llc::controlListInitialize(gui, app.DatabaseRows)); app.DatabaseRows.Orientation = ::llc::DIRECTION_DOWN; controls.Placement[app.DatabaseRows.IdControl].Area.Offset = {8, 8}; controls.Placement[app.DatabaseRows.IdControl].Area.Size.x = 1460; if_fail_fe(::llc::controlSetParent(gui, app.DatabaseRows.IdControl, dbView->IdClient));
	if_fail_fe(::llc::tunerCreate(app.DialogMain, app.TunerOffset)); app.TunerOffset->ValueLimits = {0, ::blt::MAX_TABLE_RECORD_COUNT}; app.TunerOffset->FuncValueFormat = [](::llc::vcst_t & f, ::llc::u3_t, const ::llc::minmax<::llc::u3_t> &) { f = "Offset: %llu"; return 0; }; if_fail_fe(app.TunerOffset->SetValue(0)); controls.Placement[app.TunerOffset->IdGUIControl].Area = {{24, 430}, {220, 24}};
	if_fail_fe(::llc::tunerCreate(app.DialogMain, app.TunerCount)); app.TunerCount->ValueLimits = {1, 1000}; app.TunerCount->FuncValueFormat = [](::llc::vcst_t & f, ::llc::u3_t, const ::llc::minmax<::llc::u3_t> &) { f = "Count: %llu"; return 0; }; if_fail_fe(app.TunerCount->SetValue(25)); controls.Placement[app.TunerCount->IdGUIControl].Area = {{260, 430}, {220, 24}};
	if_fail_fe(::llc::tunerCreate(app.DialogMain, app.TunerDetail)); app.TunerDetail->ValueLimits = {0, ::blt::MAX_TABLE_RECORD_COUNT - 1}; app.TunerDetail->FuncValueFormat = [](::llc::vcst_t & f, ::llc::u3_t, const ::llc::minmax<::llc::u3_t> &) { f = "Detail: %llu"; return 0; }; if_fail_fe(app.TunerDetail->SetValue(0)); controls.Placement[app.TunerDetail->IdGUIControl].Area = {{496, 430}, {220, 24}};
	::dashboardTunerThemeSet(app, *app.TunerOffset, ::llc::ASCII_COLOR_Dark_cyan); ::dashboardTunerThemeSet(app, *app.TunerCount, ::llc::ASCII_COLOR_Dark_blue); ::dashboardTunerThemeSet(app, *app.TunerDetail, ::llc::ASCII_COLOR_Dark_magenta);
	if_fail_fe(app.IdInputMode = ::llc::controlCreate(gui)); controls.Placement[app.IdInputMode].Area = {{740, 430}, {820, 24}}; controls.Text[app.IdInputMode].Align = ::llc::ALIGN_CENTER_LEFT; if_fail_fe(::llc::controlTextSet(gui, app.IdInputMode, "INPUT: CONFIGURATION PATH"));
	::llc::pobj<::llc::SDialogViewport> outView = {}; if_fail_fe(app.ViewportOutput = ::llc::viewportCreate(app.DialogMain, outView)); controls.Placement[outView->IdGUIControl].Area.Size = {1540, 300}; controls.Placement[outView->IdGUIControl].Area.Offset = {0, 470}; controls.Placement[outView->IdGUIControl].Align = ::llc::ALIGN_CENTER_TOP; if_fail_fe(::llc::controlTextSet(gui, outView->IdTitle, "QUERY / MODEL RESULT"));
	if_fail_fe(::llc::controlListInitialize(gui, app.OutputRows)); app.OutputRows.Orientation = ::llc::DIRECTION_DOWN; controls.Placement[app.OutputRows.IdControl].Area.Offset = {8, 8}; controls.Placement[app.OutputRows.IdControl].Area.Size.x = 1460; if_fail_fe(::llc::controlSetParent(gui, app.OutputRows.IdControl, outView->IdClient));
	if_fail_fe(::configurationExamplePath(app.ConfigPath)); if_fail_fe(::llc::inputBoxCreate(app.Editor, gui)); app.Editor.MaxLength = 8192; controls.Placement[app.Editor.IdRoot].Align = ::llc::ALIGN_CENTER_BOTTOM; controls.Placement[app.Editor.IdRoot].Area.Offset = {}; if_fail_fe(app.Editor.SetText(gui, app.ConfigPath)); if_fail_fe(app.Editor.Edit(gui, true));
	if_fail_fe(::statusSet(app, "Enter a Blitter configuration path and press Load. Database write operations are Append and Flush.")); if_fail_fe(::outputSet(app, "Supported operations: load configuration, select database, browse range, inspect detail, inspect model, append one JSON object, and flush dirty blocks.")); if_fail_fe(::databaseRowsUpdate(app)); return ::dashboardLayout(app);
}
::llc::err_t update(::blt::SBlitterDashboard & app, bool exitSignal) {
	if_true_vi(::llc::APPLICATION_STATE_EXIT, exitSignal); { ::llc::lock_guard lock(app.LockRender); app.Framework.RootWindow.BackBuffer = app.Offscreen; } if_true_vi(::llc::APPLICATION_STATE_EXIT, ::llc::APPLICATION_STATE_EXIT == ::llc::updateFramework(app.Framework)); if(app.LayoutSize != app.Framework.RootWindow.Size) if_fail_fe(::dashboardLayout(app));
	::llc::SGUI & gui = *app.Framework.GUI; ::llc::acid processable = {};
	if_fail_fe(::llc::guiGetProcessableControls(gui, processable));
	if(app.InputMode == ::blt::DASHBOARD_INPUT_Config && gui.Controls.IsAction(app.Editor.IdText)) if_fail_fe(::configurationSelect(app));
	const ::llc::err_t input = app.Editor.Update(gui, app.Editor.VirtualKeyboard, app.Framework.RootWindow.EventQueue, processable);
	if_fail_fe(input);
	if(input == INT_MAX) {
		if(app.InputMode == ::blt::DASHBOARD_INPUT_Config) {
			if_fail_fe(::configurationLoad(app));
		}
		else {
			if_fail_fe(::queryExecute(app, "push_back"));
		}
	}
	::llc::err_t commandResult = 0; if_fail_fe(::llc::guiProcessControls(gui, processable, [&app, &commandResult](::llc::cid_t id) { for(::llc::u2_t i = 0; i < app.Commands.IdControls.size(); ++i) if(id == app.Commands.IdControls[i]) { commandResult = ::dashboardCommand(app, id); return 1; } for(::llc::u2_t i = 1; i < app.DatabaseRows.IdControls.size(); ++i) if(id == app.DatabaseRows.IdControls[i]) { commandResult = ::selectedDatabaseSet(app, i - 1); return 1; } return 0; }));
	if(::llc::APPLICATION_STATE_EXIT == commandResult) return ::llc::APPLICATION_STATE_EXIT; if_fail_fe(commandResult); return app.DialogMain.Update();
}
::llc::err_t draw(::blt::SBlitterDashboard & app) {
	::llc::pobj<::llc::rtbgra8d32> target = {}; if_true_fe(0 == target.create()); if_fail_fe(target->resize(app.Framework.RootWindow.Size, ::llc::DARKGRAY, 0xFFFFFFFFU)); { ::llc::lock_guard lock(app.Framework.LockGUI); if_fail_fe(::llc::guiDraw(*app.DialogMain.GUI, target->Color.View)); } { ::llc::lock_guard lock(app.LockRender); app.Offscreen = target; } return 0;
}
