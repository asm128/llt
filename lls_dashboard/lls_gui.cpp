#include "dashboard_app.h"
#include "../lcp/lcp_help.h"
#include "../lcp/lcp_tools.h"
#include "llc_string_compose.h"
#include "llc_system_key.h"
#include <limits.h>
#include "llc_chrono.h"
#include "llc_runtime_module.h"

sttc llc::err_t llsControl(llc::SGUI & gui, llc::cid_t & id, llc::vcst_t text, bool label = false) {
	rtrn llsControl(gui, id, text, SGUIFamily{}, label);
}

sttc llc::err_t llsPlace(llc::SGUI & gui, llc::cid_t id, llc::s1_t x, llc::s1_t y, llc::s1_t w, llc::s1_t h) {
	gui.Controls.Placement[id].Area = {{x, y}, {w, h}};
	rtrn llc::controlMetricsInvalidate(gui, id);
}

sttc llc::err_t llsTextRelease(SLLSGUI & ui, cnst llc::acid & controls) {
	// controlTextSet borrows storage and compares the old view. Detach before its owner changes.
	for(cnst auto id : controls) { if_fail_fe(llc::controlTextSet(*ui.Framework.GUI, id, {})); }
	rtrn 0;
}

sttc llc::err_t llsDetailClear(SLLSGUI & ui) {
	if_fail_fe(llsTextRelease(ui, ui.Lines));
	if_fail_fe(llc::clear(ui.DetailLines, ui.DetailPaths));
	rtrn ui.Detail.clear();
}

sttc llc::err_t llsLines(SLLSGUI & ui) {
	if_fail_fe(ui.DetailLines.clear());
	llc::vcst_t remaining = ui.Detail;
	while(remaining.size()) {
		llc::u2_t count = 0;
		while(count < remaining.size() && remaining[count] != '\n' && count < ui.Columns) ++count;
		llc::u2_t consumed = count;
		if(count < remaining.size() && remaining[count] == '\n') ++consumed;
		else if(count == ui.Columns) {
			for(llc::u2_t word = count; word > count / 2; --word)
				if(remaining[word - 1] == ' ') { count = word - 1; consumed = word; break; }
		}
		if_fail_fe(ui.DetailLines.push_back({remaining.begin(), count}));
		if_fail_fe(remaining.slice(remaining, consumed));
	}
	ui.LineOffset = llc::min(ui.LineOffset, ui.DetailLines.size() ? ui.DetailLines.size() - 1 : 0U);
	rtrn llsPathLinks(ui.Detail, ui.DetailPaths);
}

sttc llc::err_t llsExplore(SLLSGUI & ui, llc::vcst_t path) {
	if_fail_fe(llsTextRelease(ui, ui.Breadcrumbs));
	if_fail_fe(llsExplorerOpen(ui.Explorer, path));
	ui.Tab = 4;
	ui.ListOffset = ui.LineOffset = 0;
	ui.Dirty = true;
	ui.LastSize = {};
	if_fail_fe(ui.Query.clear());
	rtrn ui.Input.SetText(*ui.Framework.GUI, {});
}

sttc llc::err_t llsLayout(SLLSGUI & ui) {
	auto & gui = *ui.Framework.GUI;
	cnst auto size = ui.Framework.RootWindow.Size;
	cnst llc::s1_t width = (llc::s1_t)llc::max(640U, (llc::u2_t)size.x);
	cnst llc::s1_t height = (llc::s1_t)llc::max(400U, (llc::u2_t)size.y);
	cnst llc::s1_t left = llc::min((llc::s1_t)320, (llc::s1_t)(width / 3));
	cnst llc::s1_t bottom = height - 46 - (ui.ShowKeyboard ? 230 : 0);
	cnst llc::s1_t top = ui.Tab == 4 ? 202 : 166;
	if_fail_fe(llsPlace(gui, ui.Title, 18, 12, width - 380, 30));
	if_fail_fe(llsPlace(gui, ui.Exit, width - 176, 12, 158, 30));
	if_fail_fe(llsPlace(gui, ui.Restart, width - 360, 12, 176, 30));
	if_fail_fe(llsPlace(gui, ui.Benchmark, width - 162, 164, 144, 28));
	if_fail_fe(llsPlace(gui, ui.Statistics, 18, 44, width - 36, 26));
	cnst llc::s1_t tabWidth = (llc::s1_t)llc::min(164U, (llc::u2_t)(width - 36) / llc::max(1U, ui.Tabs.size()));
	for(llc::u2_t i = 0; i < ui.Tabs.size(); ++i)
		if_fail_fe(llsPlace(gui, ui.Tabs[i], (llc::s1_t)(18 + i * tabWidth), 78, tabWidth - 10, 30));
	if_fail_fe(llsPlace(gui, ui.Input.IdRoot, 18, 120, width - 274, 30));
	if_fail_fe(llsPlace(gui, ui.Input.IdText, 0, 0, width - 274, 30));
	if_fail_fe(llsPlace(gui, ui.OpenPath, width - 246, 120, 106, 30));
	gui.Controls.Text[ui.Input.IdText].Align = llc::ALIGN_CENTER_LEFT;
	if_fail_fe(llsPlace(gui, ui.Keyboard, width - 132, 120, 114, 30));
	if_fail_fe(llsPlace(gui, ui.Hint, 18, height - 34, width - 36, 26));
	auto & keyboard = gui.Controls.Placement[ui.Input.VirtualKeyboard.IdRoot];
	keyboard.Align = llc::ALIGN_TOP_LEFT;
	keyboard.Area.Offset = {(llc::s1_t)(width - keyboard.Area.Size.x - 36), (llc::s1_t)(height - 120 - 264)};
	if_fail_fe(llc::controlMetricsInvalidate(gui, ui.Input.VirtualKeyboard.IdRoot));
	gui.Controls.SetHidden(ui.Input.VirtualKeyboard.IdRoot, !ui.ShowKeyboard);
	for(llc::u2_t i = 0; i < ui.Rows.size(); ++i) {
		if_fail_fe(llsPlace(gui, ui.Rows[i], 18, (llc::s1_t)(top + 30 * i), left - 28, 28));
	}
	ui.VisibleLines = llc::min((llc::u2_t)ui.Lines.size(), (llc::u2_t)llc::max(1, (bottom - top - 36) / 19));
	ui.VisibleRows = llc::min(ui.Rows.size(), (llc::u2_t)llc::max(1, (bottom - top - 30) / 30));
	ui.Columns = llc::max(1U, (llc::u2_t)(width - left - 44) / gui.Fonts[gui.SelectedFont]->CharSize.x);
	for(llc::u2_t i = 0; i < ui.Lines.size(); ++i)
		if_fail_fe(llsPlace(gui, ui.Lines[i], left, (llc::s1_t)(top + 19 * i), width - left - 18, 19));
	if_fail_fe(llsPlace(gui, ui.ListPrev, 18, bottom - 30, 90, 26));
	if_fail_fe(llsPlace(gui, ui.ListNext, 114, bottom - 30, 90, 26));
	if_fail_fe(llsPlace(gui, ui.PagePrev, left, bottom - 30, 110, 26));
	if_fail_fe(llsPlace(gui, ui.PageNext, left + 116, bottom - 30, 110, 26));
	ui.LastSize = size;
	rtrn llsLines(ui);
}

sttc llc::err_t llsSessionView(SApplication & app) {
	auto & ui = *app.UI;
	llc::lock_guard guard(app.Lock);
	llc::u2_t queued = 0, active = 0, completed = 0, failed = 0;
	for(cnst SDashboardJob & job : app.Jobs) {
		queued += job.State == "queued";
		active += !job.Historical && job.ProcessStarted && !job.ProcessFinished && !job.Detached && job.State != "completed";
		completed += job.State == "completed";
		failed += job.State == "completed" && (job.Result.Status < 0 || (job.Result.ExitKnown && job.Result.ExitCode));
	}
	if_fail_fe(llc::controlTextSet(*ui.Framework.GUI, ui.Statistics, {}));
	if_fail_fe(ui.Stats.clear());
	if_fail_fe(llc::append_strings(ui.Stats, "SESSION ", llc::str(app.ElapsedMs / 1000),
		"s   |   ", app.Connection, "   |   Requests ", llc::str(app.Requests), "   Active ", llc::str(active), "   Queued ", llc::str(queued),
		"   Done ", llc::str(completed), "   Failed ", llc::str(failed), "   History status ", llc::str(app.HistoryError)));
	if_fail_fe(llc::controlTextSet(*ui.Framework.GUI, ui.Statistics, ui.Stats));
	if(!ui.Dirty && ui.Revision == app.Revision && (ui.Tab == 0 || ui.Tab == 3 || ui.Tab == 8)) rtrn 0;
	if(ui.SessionId != app.SessionId) {
		ui.SessionId = app.SessionId;
		ui.Selected = (llc::u2_t)-1;
		ui.ListOffset = ui.LineOffset = 0;
		ui.Dirty = true;
	}
	if(ui.Tab == 6 || ui.Tab == 7 || ui.Tab == 8) {
		if_fail_fe(ui.Matches.clear());
		if_fail_fe(llsDetailClear(ui));
		if(ui.Tab == 8) {
			for(llc::u2_t i = 0; i < ui.CommandTemplateText.size(); ++i) { if_fail_fe(ui.Matches.push_back(i)); }
			if(ui.ListOffset >= ui.Matches.size()) ui.ListOffset = 0;
			for(llc::u2_t i = 0; i < ui.Rows.size(); ++i) {
				if_fail_fe(llc::controlTextSet(*ui.Framework.GUI, ui.Rows[i], {}));
				if_fail_fe(ui.RowText[i].clear());
				if(i + ui.ListOffset >= ui.Matches.size()) continue;
				cnst llc::u2_t command = ui.Matches[i + ui.ListOffset];
				if_fail_fe(ui.RowText[i].append(ui.CommandTemplateText[command]));
				if_fail_fe(llc::controlTextSet(*ui.Framework.GUI, ui.Rows[i], ui.RowText[i]));
			}
			if(app.CommandResponse.size()) { if_fail_fe(llc::append_strings(ui.Detail, "MCP RESPONSE JSON\n\n", app.CommandResponse)); }
			else { if_fail_fe(ui.Detail.append_string("LCP CONSOLE\n\nChoose a template to load its JSON into the edit box. Edit it if needed, then press Enter or Send JSON.")); }
		}
		else if(ui.Tab == 6) {
			if_fail_fe(llc::append_strings(ui.Detail, "DASHBOARD\nSERVICE / ", app.Service, "\n", app.Connection,
				"\nSession: ", llc::str(app.SessionId), "\nSnapshot age: ",
				llc::str(app.LastSnapshot ? ((llc::u3_t)llc::timeCurrentInUs() - app.LastSnapshot) / 1000000 : 0), " s"));
		}
		else {
			if_fail_fe(llc::append_strings(ui.Detail, "RENDER PERFORMANCE\nBackend: CPU / LLC guiDraw (no D3D device)\nTarget: ",
				llc::str((llc::u2_t)ui.LastSize.x), " x ", llc::str((llc::u2_t)ui.LastSize.y),
				" BGRA8\nLast draw: ", llc::str(ui.RenderUs), " us\nDraws measured: ", llc::str(ui.RenderCount),
				"\nMean draw: ", llc::str(ui.RenderCount ? ui.RenderTotalUs / ui.RenderCount : 0), " us\nPeak draw: ", llc::str(ui.RenderPeakUs),
				" us\n\nCPU draw timing excludes window presentation.\nFrame pacing includes UI update and intentional sleep.\n",
				"Benchmark: use the Benchmark button (100 draws, current size/content).\nLast benchmark mean: ", llc::str(ui.BenchmarkUs), " us"));
		}
		ui.Dirty = false;
		rtrn llsLines(ui);
	}
	if(ui.Tab == 4) {
		if(!ui.Dirty) rtrn 0;
		if_fail_fe(ui.Matches.clear());
		for(llc::u2_t i = 0; i < ui.Explorer.Rows.size(); ++i) { if_fail_fe(ui.Matches.push_back(i)); }
		ui.Selected = ui.Explorer.Selected;
		if_fail_fe(llsDetailClear(ui));
		ui.Detail = ui.Explorer.Summary;
		if(ui.ListOffset >= ui.Matches.size()) ui.ListOffset = 0;
		for(llc::u2_t i = 0; i < ui.Rows.size() && i + ui.ListOffset < ui.Matches.size(); ++i) {
			if_fail_fe(llc::controlTextSet(*ui.Framework.GUI, ui.Rows[i], {}));
			if_fail_fe(llsExplorerLabel(ui.Explorer, i + ui.ListOffset, ui.RowText[i]));
			if_fail_fe(llc::controlTextSet(*ui.Framework.GUI, ui.Rows[i], ui.RowText[i]));
		}
		ui.Dirty = false;
		rtrn llsLines(ui);
	}
	ui.Revision = app.Revision;
	if(ui.Tab == 0) {
		if_fail_fe(ui.Matches.clear());
		ui.Selected = (llc::u2_t)-1;
		if_fail_fe(llsDetailClear(ui));
		if_fail_fe(lcpToolsAvailable(ui.Query, ui.Detail));
		for(llc::cid_t row : ui.Rows) { if_fail_fe(llc::controlTextSet(*ui.Framework.GUI, row, {})); }
		ui.Dirty = false;
		rtrn llsLines(ui);
	}
	else {
		if_fail_fe(ui.Matches.clear());
		for(llc::u2_t i = 0; i < app.Jobs.size(); ++i) {
			cnst auto & job = app.Jobs[i];
			cnst bool included = ui.Tab == 1
				? !job.Historical && job.ProcessStarted && !job.ProcessFinished && !job.Detached && job.State != "completed"
				: ui.Tab == 2 ? job.State == "queued" : ui.Tab == 5 ? job.State == "completed" && (job.Result.Status < 0 || (job.Result.ExitKnown && job.Result.ExitCode)) : job.State == "completed" || job.State == "interrupted";
			if(included && (!ui.Query.size() || job.Id.find(ui.Query) >= 0 || job.Summary.find(ui.Query) >= 0)) {
				if_fail_fe(ui.Matches.push_back(i));
			}
		}
	}
	if(ui.Tab == 5) { if_fail_fe(llc::reverse(ui.Matches)); }
	if(ui.Matches.find(ui.Selected) < 0)
		ui.Selected = ui.Matches.size() ? ui.Matches[0] : (llc::u2_t)-1;
	if_fail_fe(llsDetailClear(ui));
	if(ui.Selected == (llc::u2_t)-1) {
		cnst llc::vcst_t empty = ui.Tab == 0 ? llc::vcst_t{"No matching help. Try a command name or a word such as build or output."}
			: llc::vcst_t{"No matching entries in this session.\n\nBuild requests received through MCP appear here.\nSelect a row to inspect its request, command and captured result.\nOnly this LLS session is shown; no machine-wide process scan."};
		if_fail_fe(ui.Detail.append(empty));
	}
	else if(ui.Tab == 0) {
		if_fail_fe(ui.Detail.append(help(ui.Paths[ui.Selected], true)));
	}
	else {
		cnst auto & job = app.Jobs[ui.Selected];
		if_fail_fe(llc::append_strings(ui.Detail, "REQUEST ", job.Id, "  /  ", job.State, "\nPID ", llc::str(job.PID),
			"   Elapsed ", llc::str(job.ElapsedMs), " ms\n"));
		if_fail_fe(llc::append_strings(ui.Detail, "Origin session ", llc::str(job.OriginSession), job.Historical ? llc::vcst_t{" / restored history\n"} : llc::vcst_t{" / current\n"}));
		if(job.State == "interrupted") { if_fail_fe(ui.Detail.append_string("Interrupted / outcome unknown. Not replayed.\n")); }
		if(job.State == "completed") {
			if_fail_fe(llc::append_strings(ui.Detail, "Operation status ", llc::str(job.Result.Status), "   Exit "));
			if(job.Result.ExitKnown) { if_fail_fe(llc::append_strings(ui.Detail, llc::str(job.Result.ExitCode))); }
			else { if_fail_fe(ui.Detail.append_string("unknown")); }
		}
		if(job.Detached) { if_fail_fe(ui.Detail.append_string("\nLaunch only: completion is not tracked.")); }
		if_fail_fe(llc::append_strings(ui.Detail, "\n\nRESULT\n", job.Summary));
	}
	cnst llc::u2_t rows = ui.Rows.size();
	if(ui.ListOffset >= ui.Matches.size()) ui.ListOffset = 0;
	for(llc::u2_t i = 0; i < rows; ++i) {
		if_fail_fe(llc::controlTextSet(*ui.Framework.GUI, ui.Rows[i], {}));
		if_fail_fe(ui.RowText[i].clear());
		if(i + ui.ListOffset >= ui.Matches.size()) continue;
		cnst llc::u2_t selected = ui.Matches[i + ui.ListOffset];
		if(ui.Tab == 0) {
			if_fail_fe(ui.RowText[i].append(ui.Paths[selected].size() ? ui.Paths[selected] : llc::vcst_t{"Help / index"}));
		}
		else {
			cnst auto & job = app.Jobs[selected];
			if_fail_fe(llc::append_strings(ui.RowText[i], job.Id, "  ", job.State, "  PID ", llc::str(job.PID)));
		}
		if_fail_fe(llc::controlTextSet(*ui.Framework.GUI, ui.Rows[i], ui.RowText[i]));
	}
	ui.Dirty = false;
	rtrn llsLines(ui);
}

llc::err_t llsGUISetup(SApplication & app) {
	if_null_fe(app.UI.create(app.Runtime));
	auto & ui = *app.UI;
	auto & gui = *ui.Framework.GUI;
	auto & window = ui.Framework.RootWindow;
	window.Size = {1180, 780};
	if_null_fe(window.BackBuffer.create());
	if_fail_fe(window.BackBuffer->resize(window.Size.u2()));
	if_fail_fe(llc::mainWindowCreate(window, app.Runtime.PlatformDetail, window.Input));
#ifdef LLC_WINDOWS
	SetWindowTextA(window.PlatformDetail.WindowHandle, "LLS - Session workspace");
#endif
	llc::SRasterFontManager fonts;
	if_fail_fe(llc::rasterFontDefaults(fonts));
	gui.Fonts = fonts.Fonts;
	gui.SelectedFont = 7; // Existing 8x16 font; readable without scaling the entire dashboard.
	if_fail_fe(llc::fileToMemory("lls_gui.json", ui.Layout.Bytes));
	if_fail_fe(llc::jsonParse(ui.Layout.Reader, ui.Layout.Bytes, llc::JSON_PARSE_STRICT));
	if_fail_fe(llsJSONLabels(ui.Layout.Reader, "tabs", "{title}", ui.TabText, ui.TabValues));
	{
		llc::au2_t values;
		if_fail_fe(llsJSONLabels(ui.Layout.Reader, "mcp.templates", "{title}", ui.CommandTemplateText, values));
		if_fail_fe(llsJSONLabels(ui.Layout.Reader, "mcp.templates", "{json}", ui.CommandTemplateJSON, values));
		if_true_fe(ui.CommandTemplateText.size() != ui.CommandTemplateJSON.size());
	}
	gui.ColorModeDefault = llc::GUI_COLOR_MODE_Flat;
	if_null_fe(gui.Colors.create());
	if_fail_fe(gui.Colors->Palettes.resize(llc::GUI_CONTROL_PALETTE_COUNT * 2));
	for(llc::u2_t state = 0; state < llc::GUI_CONTROL_PALETTE_COUNT; ++state) {
		auto & palette = gui.Colors->Palettes[state];
		for(auto & color : palette) color = {34, 27, 23, 255};
		palette[llc::UI_CONTROL_AREA_CLIENT] = state == llc::GUI_CONTROL_PALETTE_SELECTED || state == llc::GUI_CONTROL_PALETTE_SELECTED_HOVER
			? llc::bgra{90, 66, 24, 255} : state == llc::GUI_CONTROL_PALETTE_HOVER ? llc::bgra{62, 49, 34, 255} : llc::bgra{42, 33, 27, 255};
		palette[llc::UI_CONTROL_AREA_TEXT_FACE] = {231, 231, 222, 255};
		palette[llc::UI_CONTROL_AREA_BORDER_LEFT] = palette[llc::UI_CONTROL_AREA_BORDER_TOP]
			= palette[llc::UI_CONTROL_AREA_BORDER_RIGHT] = palette[llc::UI_CONTROL_AREA_BORDER_BOTTOM] = {87, 68, 43, 255};
		auto & linkPalette = gui.Colors->Palettes[state + llc::GUI_CONTROL_PALETTE_COUNT];
		linkPalette = palette;
		linkPalette[llc::UI_CONTROL_AREA_TEXT_FACE] = {240, 200, 112, 255};
	}
	if_fail_fe(llsControl(gui, ui.Title, app.Title, true));
	if_fail_fe(llsControl(gui, ui.Statistics, "", true));
	if_fail_fe(llsControl(gui, ui.Hint, "Type to filter. Select a result to inspect. Enter opens the first match. Mouse wheel scrolls details.", true));
	if_fail_fe(llsControlPool(gui, ui.Tabs, ui.TabText.size(), ui.Family));
	for(llc::u2_t i = 0; i < ui.Tabs.size(); ++i) { if_fail_fe(llc::controlTextSet(gui, ui.Tabs[i], ui.TabText[i])); }
	if_fail_fe(llsControl(gui, ui.Exit, "Exit dashboard"));
	if_fail_fe(llsControl(gui, ui.Restart, "Restart dashboard"));
	if_fail_fe(llsControl(gui, ui.Benchmark, "Benchmark"));
	if_fail_fe(llsControl(gui, ui.Keyboard, "Keyboard"));
	if_fail_fe(llsControl(gui, ui.OpenPath, "Open path"));
	if_fail_fe(llsControl(gui, ui.ListPrev, "< List"));
	if_fail_fe(llsControl(gui, ui.ListNext, "List >"));
	if_fail_fe(llsControl(gui, ui.PagePrev, "< Page"));
	if_fail_fe(llsControl(gui, ui.PageNext, "Page >"));
	if_fail_fe(ui.RowText.resize(16));
	if_fail_fe(llsControlPool(gui, ui.Rows, 16, ui.Family));
	if_fail_fe(llsControlPool(gui, ui.Lines, 40, {{}, {}}, true));
	if_fail_fe(llc::inputBoxCreate(ui.Input, gui));
	ui.Input.MaxLength = 4096;
	ui.Input.Edit(gui, true);
	for(auto & control : gui.Controls.Draw) {
		control.UseNewPalettes = true;
		for(llc::u2_t state = 0; state < llc::GUI_CONTROL_PALETTE_COUNT; ++state) control.Palettes[state] = (llc::s1_t)state;
	}
	if_fail_fe(helpPaths(ui.Paths, true));
	if_fail_fe(llsLayout(ui));
	if_fail_fe(llsSessionView(app));
	rtrn llc::guiUpdateMetrics(gui, window.Size, true);
}

llc::err_t llsGUIUpdate(SApplication & app) {
	auto & ui = *app.UI;
	auto & gui = *ui.Framework.GUI;
	auto & window = ui.Framework.RootWindow;
	cnst llc::err_t updated = llc::updateFramework(ui.Framework);
	if(updated) {
		if(llc::failed(updated)) error_printf("Dashboard framework failed: %i.", updated);
		rtrn updated;
	}
#ifdef LLC_WINDOWS
	{
		llc::string title;
		{ llc::lock_guard guard(app.Lock); if_fail_fe(llc::append_strings(title, app.Title, " | ", app.Connection, " | Session ", llc::str(app.SessionId))); }
		SetWindowTextA(window.PlatformDetail.WindowHandle, title.begin());
	}
#endif
	if(gui.Controls.IsAction(ui.Exit) || gui.Controls.IsAction(ui.Restart)) {
		info_printf("Dashboard requested %s.", gui.Controls.IsAction(ui.Restart) ? "restart" : "exit");
		app.Restart = gui.Controls.IsAction(ui.Restart);
		app.Closing = true;
		rtrn llc::APPLICATION_STATE_EXIT;
	}
	gui.Controls.SetHidden(ui.Benchmark, ui.Tab != 7);
	if(gui.Controls.IsAction(ui.Benchmark)) ui.BenchmarkLeft = 100;
	if(gui.Controls.IsAction(ui.Keyboard)) { ui.ShowKeyboard = !ui.ShowKeyboard; ui.LastSize = {}; }
	if_fail_fe(llc::controlTextSet(gui, ui.OpenPath, ui.Tab == 8 ? llc::vcst_t{"Send JSON"} : llc::vcst_t{"Open path"}));
	for(llc::u2_t tab = 0; tab < ui.Tabs.size(); ++tab) {
		if(gui.Controls.IsAction(ui.Tabs[tab])) {
			ui.Tab = ui.TabValues[tab];
			ui.ListOffset = ui.LineOffset = 0;
			ui.Selected = (llc::u2_t)-1;
			if_fail_fe(ui.Input.SetText(gui, {}));
			ui.Dirty = true;
			ui.LastSize = {};
			if(ui.Tab == 4 && !ui.Explorer.Branches.size()) {
				llc::vcst_t root;
				if_fail_fe(llc::jsonExpressionResolve("explorer.root", ui.Layout.Reader, 0, root));
				if_fail_fe(llsExplore(ui, root));
			}
		}
		gui.Controls.SetSelected(ui.Tabs[tab], ui.Tab == ui.TabValues[tab]);
	}
	for(llc::u2_t i = 0; i < ui.Breadcrumbs.size() && i < ui.Explorer.Crumbs.size(); ++i)
		if(ui.Tab == 4 && gui.Controls.IsAction(ui.Breadcrumbs[i])) {
			llc::string path = ui.Explorer.Crumbs[i];
			if_fail_fe(llsExplore(ui, path));
			break;
		}
	if_fail_fe(ui.Processable.clear());
	if_fail_fe(llc::guiGetProcessableControls(gui, ui.Processable));
	cnst llc::err_t input = ui.Input.Update(gui, ui.Input.VirtualKeyboard, window.EventQueue, ui.Processable);
	if_fail_fe(input);
	if(!ui.Input.Editing) { ui.Input.Edit(gui, true); ui.ShowKeyboard = false; ui.LastSize = {}; }
	if(ui.Query != ui.Input.Text) {
		ui.Query = ui.Input.Text;
		ui.Selected = (llc::u2_t)-1;
		ui.ListOffset = ui.LineOffset = 0;
		ui.Dirty = true;
	}
	if(ui.Tab == 8 && (gui.Controls.IsAction(ui.OpenPath) || input == INT_MAX) && ui.Input.Text.size()) {
		cnst llc::err_t queued = llsCommandQueue(app, ui.Input.Text);
		if(llc::failed(queued)) {
			llc::lock_guard guard(app.Lock);
			if_fail_fe(app.CommandResponse.clear());
			if_fail_fe(app.CommandResponse.append_string("Invalid command JSON or command already running."));
			++app.Revision;
		}
		ui.Dirty = true;
	}
	else if((gui.Controls.IsAction(ui.OpenPath) || (ui.Tab == 4 && input == INT_MAX)) && ui.Input.Text.size()) {
		llc::string path = ui.Input.Text;
		if_fail_fe(llsExplore(ui, path));
	}
	else if(input == INT_MAX && ui.Matches.size()) {
		ui.Selected = ui.Matches[0];
		ui.LineOffset = 0;
		ui.Dirty = true;
	}
	for(llc::u2_t i = 0; i < ui.Rows.size(); ++i)
		if(gui.Controls.IsAction(ui.Rows[i]) && i + ui.ListOffset < ui.Matches.size()) {
			if(ui.Tab == 4) {
				if_fail_fe(llsTextRelease(ui, ui.Breadcrumbs));
				if_fail_fe(llsExplorerSelect(ui.Explorer, i + ui.ListOffset));
			}
			ui.Selected = ui.Matches[i + ui.ListOffset];
			if(ui.Tab == 8) {
				if_fail_fe(ui.Input.SetText(gui, ui.CommandTemplateJSON[ui.Selected]));
				ui.Query = ui.Input.Text;
			}
			ui.LineOffset = 0;
			ui.Dirty = true;
		}
	for(llc::u2_t i = 0; i < ui.Lines.size() && i + ui.LineOffset < ui.DetailLines.size(); ++i) {
		if(!gui.Controls.IsAction(ui.Lines[i])) continue;
		cnst auto & area = gui.Controls.Area[ui.Lines[i]].Client.Global;
		cnst llc::s2_t x = window.Input->MouseCurrent.Position.x - area.Offset.x;
		if(x < 0) continue;
		cnst llc::u2_t character = (llc::u2_t)x / gui.Fonts[gui.SelectedFont]->CharSize.x;
		cnst auto & line = ui.DetailLines[i + ui.LineOffset];
		if(character >= line.size()) continue;
		cnst char * hit = line.begin() + character;
		for(llc::vcst_t path : ui.DetailPaths)
			if(hit >= path.begin() && hit < path.end()) {
				llc::string selected = path;
				if_fail_fe(llsExplore(ui, selected));
				break;
			}
		break;
	}
	if(gui.Controls.IsAction(ui.ListPrev)) { ui.ListOffset = ui.ListOffset > ui.VisibleRows ? ui.ListOffset - ui.VisibleRows : 0; ui.Dirty = true; }
	if(gui.Controls.IsAction(ui.ListNext) && ui.ListOffset + ui.VisibleRows < ui.Matches.size()) { ui.ListOffset += ui.VisibleRows; ui.Dirty = true; }
	if(gui.Controls.IsAction(ui.PagePrev) || window.Input->MouseCurrent.Deltas.z > 0)
		ui.LineOffset = ui.LineOffset > ui.VisibleLines ? ui.LineOffset - ui.VisibleLines : 0;
	if(gui.Controls.IsAction(ui.PageNext) || window.Input->MouseCurrent.Deltas.z < 0)
		if(ui.LineOffset + ui.VisibleLines < ui.DetailLines.size()) ui.LineOffset += ui.VisibleLines;
	if(ui.LastSize != window.Size) { if_fail_fe(llsLayout(ui)); ui.Dirty = true; }
	if_fail_fe(llc::controlTextSet(gui, ui.Hint, ui.Tab == 8
		? llc::vcst_t{"Choose a template to load it. Edit the JSON if needed; Enter or Send JSON sends it."}
		: ui.Tab == 4
		? llc::vcst_t{"PATH (not search): enter a folder path. Click folders to expand; breadcrumbs navigate."}
		: llc::vcst_t{"Type to filter. Click paths in details to browse. Open path uses the text box. Mouse wheel scrolls."}));
	ui.Refresh += ui.Framework.FrameInfo.Seconds.LastFrame;
	if(ui.Dirty || ui.Refresh >= .2) { if_fail_fe(llsSessionView(app)); ui.Refresh = 0; }
	if_fail_fe(llsControlPool(gui, ui.Breadcrumbs, ui.Explorer.Crumbs.size(), ui.Family));
	llc::s1_t crumbX = 18;
	for(llc::u2_t i = 0; i < ui.Breadcrumbs.size(); ++i) {
		cnst bool exists = ui.Tab == 4 && i < ui.Explorer.Crumbs.size();
		gui.Controls.SetHidden(ui.Breadcrumbs[i], !exists);
		if(!exists) continue;
		llc::vcst_t path = ui.Explorer.Crumbs[i], name = path;
		while(name.size() > 3 && name[name.size() - 1] == '/') name = {name.begin(), name.size() - 1};
		if(i) { if_fail_fe(llc::pathFilename(name, name)); }
		cnst llc::s1_t width = (llc::s1_t)llc::min(220U, 20U + name.size() * 8);
		gui.Controls.SetHidden(ui.Breadcrumbs[i], crumbX + width > window.Size.x - 18);
		if_fail_fe(llc::controlTextSet(gui, ui.Breadcrumbs[i], name));
		if_fail_fe(llsPlace(gui, ui.Breadcrumbs[i], crumbX, 164, width, 26));
		crumbX += width + 6;
	}
	for(llc::u2_t i = 0; i < ui.Rows.size(); ++i) {
		gui.Controls.SetHidden(ui.Rows[i], i + ui.ListOffset >= ui.Matches.size() || i >= ui.VisibleRows);
		gui.Controls.SetSelected(ui.Rows[i], i + ui.ListOffset < ui.Matches.size() && ui.Matches[i + ui.ListOffset] == ui.Selected);
	}
	for(llc::u2_t i = 0; i < ui.Lines.size(); ++i) {
		cnst bool visible = i < ui.VisibleLines && i + ui.LineOffset < ui.DetailLines.size();
		gui.Controls.SetHidden(ui.Lines[i], !visible);
		if(visible) {
			cnst auto line = ui.DetailLines[i + ui.LineOffset];
			bool linked = false;
			for(llc::vcst_t path : ui.DetailPaths) linked |= line.begin() < path.end() && path.begin() < line.end();
			for(llc::u2_t state = 0; state < llc::GUI_CONTROL_PALETTE_COUNT; ++state)
				gui.Controls.Draw[ui.Lines[i]].Palettes[state] = (llc::s1_t)(state + (linked ? llc::GUI_CONTROL_PALETTE_COUNT : 0));
			if_fail_fe(llc::controlTextSet(gui, ui.Lines[i], line));
		}
	}
	if_fail_fe(llc::guiUpdateMetrics(gui, window.Size, false));
	if(!window.NoDraw) {
		if_fail_fe(window.BackBuffer->resize(window.Size.u2()));
		if_fail_fe(window.BackBuffer->Color.View.fill(llc::bgra{25, 20, 17, 255}));
		cnst auto start = llc::timeCurrentInUs();
		if_fail_fe(llc::guiDraw(gui, window.BackBuffer->Color.View));
		ui.RenderUs = llc::timeCurrentInUs() - start;
		++ui.RenderCount;
		ui.RenderTotalUs += ui.RenderUs;
		ui.RenderPeakUs = llc::max(ui.RenderPeakUs, ui.RenderUs);
		if(ui.BenchmarkLeft) {
			if(ui.BenchmarkLeft == 100) ui.BenchmarkTotalUs = 0;
			ui.BenchmarkTotalUs += ui.RenderUs;
			if(!--ui.BenchmarkLeft) { ui.BenchmarkUs = ui.BenchmarkTotalUs / 100; ui.Dirty = true; }
		}
	}
	llc::sleep(window.NoDraw ? 50 : 8);
	rtrn 0;
}
