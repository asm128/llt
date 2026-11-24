#include "dashboard_app.h"
#include "../lcp/lcp_help.h"
#include "../lcp/lcp_tools.h"
#include "llc_string_compose.h"
#include "llc_system_key.h"
#include <limits.h>
#include "llc_chrono.h"
#include "llc_event_input.h"
#include "llc_runtime_module.h"
#include "llc_gui_control_list.h"

tplt<tpnm TEnum>
stin llc::cid_t llsChild(cnst llc::SGUI & gui, llc::cid_t root, TEnum index) {
	rtrn gui.Controls.Children[root][(llc::u2_t)index];
}

tplt<tpnm TEnum>
sttc llc::err_t llsEnumCreate(llc::SGUI & gui, llc::cid_t root, cnst llc::n2u1_t & size, llc::ALIGN controlAlign, llc::ALIGN textAlign) {
	llc::acid created;
	if_fail_fe(llc::guiCreateControlList<TEnum>(gui, root, size, {}, controlAlign, textAlign, created));
	if_true_fef(created.size() != llc::get_value_count<TEnum>(), "Created:%u. Reflected:%u.", created.size(), llc::get_value_count<TEnum>());
	if_true_fef(gui.Controls.Children[root].size() != llc::get_value_count<TEnum>(), "Root:%i. Children:%u. Reflected:%u.", root, gui.Controls.Children[root].size(), llc::get_value_count<TEnum>());
	for(cnst llc::cid_t id : created) if_fail_fe(llsControlPalette(gui, id));
	rtrn 0;
}

sttc llc::err_t llsControl(llc::SGUI & gui, llc::cid_t & id, llc::vcst_t text, bool label = false, llc::cid_t parent = llc::CID_INVALID) {
	rtrn llsControl(gui, id, text, SGUIFamily{}, label, parent);
}

sttc llc::err_t llsPlace(llc::SGUI & gui, llc::cid_t id, llc::s1_t x, llc::s1_t y, llc::s1_t w, llc::s1_t h) {
	gui.Controls.Placement[id].Area = {{x, y}, {w, h}};
	rtrn llc::controlMetricsInvalidate(gui, id);
}

sttc llc::err_t llsToolbarCreate(SLLSGUI & ui) {
	auto & gui = *ui.Framework.GUI;
	if_fail_fe(ui.Toolbar = llc::controlCreateChild(gui, ui.Host));
	if_fail_fe(llsControlPalette(gui, ui.Toolbar));
	gui.Controls.Draw[ui.Toolbar].NoBorder = true;
	gui.Controls.Draw[ui.Toolbar].NoClient = true;
	if_fail_fe(llsEnumCreate<LLS_TOOLBAR>(gui, ui.Toolbar, {46, 46}, llc::ALIGN_TOP_LEFT, llc::ALIGN_CENTER));
	for(llc::u2_t i = 0; i < llc::get_value_count<LLS_TOOLBAR>(); ++i) {
		cnst llc::cid_t id = gui.Controls.Children[ui.Toolbar][i];
		gui.Controls.Placement[id].Area = {{(llc::s1_t)(46 * i), 0}, {46, 46}};
		gui.Controls.Placement[id].Border = {2, 2, 2, 2};
		gui.Controls.Placement[id].Margin = {1, 1, 1, 1};
		gui.Controls.Text[id].FontSelected = 10;
		if_fail_fe(llc::controlTextSet(gui, id, llc::get_value_descv((LLS_TOOLBAR)i)));
	}
	rtrn 0;
}

tplt<tpnm TContent>
sttc llc::err_t llsRootCreate(SLLSGUI & ui, llc::cid_t & root, llc::vcst_t title, cnst llc::rect2s1_t & area) {
	auto & gui = *ui.Framework.GUI;
	if_fail_fe(root = llc::controlCreateChild(gui, ui.Host));
	if_fail_fe(llsControlPalette(gui, root));
	gui.Controls.Placement[root].Area = area;
	gui.Controls.Placement[root].Border = {3, 3, 3, 3};
	gui.Controls.Placement[root].Margin = {1, 1, 1, 1};
	if_fail_fe(llsEnumCreate<LLS_VIEWPORT>(gui, root, {1, 1}, llc::ALIGN_TOP_LEFT, llc::ALIGN_CENTER));
	cnst llc::cid_t titleId = llsChild(gui, root, LLS_VIEWPORT_Title);
	cnst llc::cid_t closeId = llsChild(gui, root, LLS_VIEWPORT_Close);
	cnst llc::cid_t targetId = llsChild(gui, root, LLS_VIEWPORT_Target);
	cnst llc::cid_t resizeId = llsChild(gui, root, LLS_VIEWPORT_Resize);
	if_fail_fe(llc::controlTextSet(gui, titleId, title));
	if_fail_fe(llc::controlTextSet(gui, closeId, "X"));
	if_fail_fe(llc::controlTextSet(gui, targetId, {}));
	if_fail_fe(llc::controlTextSet(gui, resizeId, "+"));
	gui.Controls.Text[titleId].FontSelected = 7;
	gui.Controls.Text[closeId].FontSelected = 7;
	gui.Controls.Text[resizeId].FontSelected = 7;
	gui.Controls.Text[titleId].Align = llc::ALIGN_CENTER;
	if_fail_fe(llsEnumCreate<TContent>(gui, targetId, {1, 1}, llc::ALIGN_TOP_LEFT, llc::ALIGN_CENTER_LEFT));
	for(llc::cid_t id : gui.Controls.Children[targetId]) gui.Controls.Text[id].FontSelected = 7;
	rtrn 0;
}

sttc llc::err_t llsRootChromeLayout(llc::SGUI & gui, llc::cid_t root) {
	cnst llc::n2s1_t size = gui.Controls.Placement[root].Area.Size;
	if_fail_fe(llsPlace(gui, llsChild(gui, root, LLS_VIEWPORT_Title ),  4,  4, llc::max((llc::s1_t)1, (llc::s1_t)(size.x - 40)), 28));
	if_fail_fe(llsPlace(gui, llsChild(gui, root, LLS_VIEWPORT_Close ), (llc::s1_t)(size.x - 32),  4, 28, 28));
	if_fail_fe(llsPlace(gui, llsChild(gui, root, LLS_VIEWPORT_Target),  4, 36, llc::max((llc::s1_t)1, (llc::s1_t)(size.x -  8)), llc::max((llc::s1_t)1, (llc::s1_t)(size.y - 40))));
	rtrn llsPlace(gui, llsChild(gui, root, LLS_VIEWPORT_Resize), (llc::s1_t)(size.x - 20), (llc::s1_t)(size.y - 20), 16, 16);
}

sttc llc::err_t llsProcessLayout(llc::SGUI & gui, llc::cid_t root) {
	if_fail_fe(llsRootChromeLayout(gui, root));
	cnst llc::cid_t target = llsChild(gui, root, LLS_VIEWPORT_Target);
	cnst llc::s1_t width = llc::max((llc::s1_t)1, (llc::s1_t)(gui.Controls.Placement[target].Area.Size.x - 12));
	for(llc::u2_t i = 0; i < llc::get_value_count<PROCESS_VIEWPORT>(); ++i)
		if_fail_fe(llsPlace(gui, gui.Controls.Children[target][i], 6, (llc::s1_t)(6 + 27 * i), width, 25));
	rtrn 0;
}

sttc llc::err_t llsServiceLayout(llc::SGUI & gui, llc::cid_t root) {
	if_fail_fe(llsRootChromeLayout(gui, root));
	cnst llc::cid_t target = llsChild(gui, root, LLS_VIEWPORT_Target);
	cnst llc::s1_t width = llc::max((llc::s1_t)1, (llc::s1_t)(gui.Controls.Placement[target].Area.Size.x - 12));
	cnst llc::s1_t labelWidth = llc::min((llc::s1_t)132, (llc::s1_t)(width / 3));
	for(llc::u2_t pair = 0; pair < 4; ++pair) {
		cnst llc::s1_t y = (llc::s1_t)(6 + 34 * pair);
		if_fail_fe(llsPlace(gui, gui.Controls.Children[target][pair * 2 + 0], 6, y, labelWidth, 30));
		if_fail_fe(llsPlace(gui, gui.Controls.Children[target][pair * 2 + 1], (llc::s1_t)(12 + labelWidth), y, llc::max((llc::s1_t)1, (llc::s1_t)(width - labelWidth - 6)), 30));
	}
	rtrn llsPlace(gui, llsChild(gui, target, SERVICE_VIEWPORT_Stop), 6, 148, width, 34);
}

stxp llc::u2_t LLS_EXPLORER_ROWS = (llc::u2_t)EXPLORER_VIEWPORT_Previous - (llc::u2_t)EXPLORER_VIEWPORT_Row00;

sttc llc::cid_t llsExplorerRow(cnst llc::SGUI & gui, llc::cid_t root, llc::u2_t row) {
	cnst llc::cid_t target = llsChild(gui, root, LLS_VIEWPORT_Target);
	rtrn gui.Controls.Children[target][(llc::u2_t)EXPLORER_VIEWPORT_Row00 + row];
}

sttc llc::err_t llsExplorerLayout(llc::SGUI & gui, llc::cid_t root) {
	if_fail_fe(llsRootChromeLayout(gui, root));
	cnst llc::cid_t target = llsChild(gui, root, LLS_VIEWPORT_Target);
	cnst llc::n2s1_t size = gui.Controls.Placement[target].Area.Size;
	cnst llc::s1_t width = llc::max((llc::s1_t)1, (llc::s1_t)(size.x - 12));
	if_fail_fe(llsPlace(gui, llsChild(gui, target, EXPLORER_VIEWPORT_Path), 6, 6, width, 28));
	for(llc::u2_t i = 0; i < LLS_EXPLORER_ROWS; ++i)
		if_fail_fe(llsPlace(gui, llsExplorerRow(gui, root, i), 6, (llc::s1_t)(40 + 27 * i), width, 25));
	cnst llc::s1_t navigationY = (llc::s1_t)(46 + 27 * LLS_EXPLORER_ROWS);
	if_fail_fe(llsPlace(gui, llsChild(gui, target, EXPLORER_VIEWPORT_Previous), 6, navigationY, 92, 28));
	if_fail_fe(llsPlace(gui, llsChild(gui, target, EXPLORER_VIEWPORT_Next), 104, navigationY, 92, 28));
	rtrn llsPlace(gui, llsChild(gui, target, EXPLORER_VIEWPORT_Summary), 6, (llc::s1_t)(navigationY + 34), width, 28);
}

stxp llc::u2_t LLS_HELP_TOPICS = (llc::u2_t)HELP_VIEWPORT_TopicPrevious - (llc::u2_t)HELP_VIEWPORT_Topic00;
stxp llc::u2_t LLS_HELP_LINES = (llc::u2_t)HELP_VIEWPORT_PagePrevious - (llc::u2_t)HELP_VIEWPORT_Line00;

sttc llc::cid_t llsHelpTopic(cnst llc::SGUI & gui, llc::cid_t root, llc::u2_t row) {
	cnst llc::cid_t target = llsChild(gui, root, LLS_VIEWPORT_Target);
	rtrn gui.Controls.Children[target][(llc::u2_t)HELP_VIEWPORT_Topic00 + row];
}

sttc llc::cid_t llsHelpLine(cnst llc::SGUI & gui, llc::cid_t root, llc::u2_t row) {
	cnst llc::cid_t target = llsChild(gui, root, LLS_VIEWPORT_Target);
	rtrn gui.Controls.Children[target][(llc::u2_t)HELP_VIEWPORT_Line00 + row];
}

sttc llc::err_t llsHelpLayout(llc::SGUI & gui, llc::cid_t root) {
	if_fail_fe(llsRootChromeLayout(gui, root));
	cnst llc::cid_t target = llsChild(gui, root, LLS_VIEWPORT_Target);
	cnst llc::n2s1_t size = gui.Controls.Placement[target].Area.Size;
	cnst llc::s1_t topicWidth = llc::min((llc::s1_t)280, (llc::s1_t)(size.x / 3));
	cnst llc::s1_t contentX = (llc::s1_t)(topicWidth + 18);
	cnst llc::s1_t contentWidth = llc::max((llc::s1_t)1, (llc::s1_t)(size.x - contentX - 6));
	for(llc::u2_t i = 0; i < LLS_HELP_TOPICS; ++i)
		if_fail_fe(llsPlace(gui, llsHelpTopic(gui, root, i), 6, (llc::s1_t)(6 + 30 * i), topicWidth, 28));
	cnst llc::s1_t topicNavigationY = (llc::s1_t)(12 + 30 * LLS_HELP_TOPICS);
	if_fail_fe(llsPlace(gui, llsChild(gui, target, HELP_VIEWPORT_TopicPrevious), 6, topicNavigationY, 92, 28));
	if_fail_fe(llsPlace(gui, llsChild(gui, target, HELP_VIEWPORT_TopicNext), 104, topicNavigationY, 92, 28));
	for(llc::u2_t i = 0; i < LLS_HELP_LINES; ++i)
		if_fail_fe(llsPlace(gui, llsHelpLine(gui, root, i), contentX, (llc::s1_t)(6 + 24 * i), contentWidth, 22));
	cnst llc::s1_t pageNavigationY = (llc::s1_t)(12 + 24 * LLS_HELP_LINES);
	if_fail_fe(llsPlace(gui, llsChild(gui, target, HELP_VIEWPORT_PagePrevious), contentX, pageNavigationY, 92, 28));
	rtrn llsPlace(gui, llsChild(gui, target, HELP_VIEWPORT_PageNext), (llc::s1_t)(contentX + 98), pageNavigationY, 92, 28);
}

sttc llc::err_t llsRootArrange(SLLSGUI & ui) {
	auto & gui = *ui.Framework.GUI;
	cnst llc::s1_t width = (llc::s1_t)llc::max(900U, (llc::u2_t)ui.Framework.RootWindow.Size.x);
	cnst llc::s1_t height = (llc::s1_t)llc::max(620U, (llc::u2_t)ui.Framework.RootWindow.Size.y);
	gui.Controls.Placement[ui.Toolbar].Area = {{18, 12}, {(llc::s1_t)(46 * llc::get_value_count<LLS_TOOLBAR>()), 46}};
	cnst llc::s1_t contentTop = 202;
	cnst llc::s1_t contentHeight = llc::max((llc::s1_t)280, (llc::s1_t)(height - contentTop - 42));
	cnst llc::s1_t available = (llc::s1_t)(width - 48);
	cnst llc::s1_t processWidth = (llc::s1_t)(available * 3 / 5);
	gui.Controls.Placement[ui.Processes].Area = {{18, contentTop}, {processWidth, contentHeight}};
	gui.Controls.Placement[ui.Services].Area = {{(llc::s1_t)(30 + processWidth), contentTop}, {(llc::s1_t)(available - processWidth), contentHeight}};
	if_fail_fe(llc::controlMetricsInvalidate(gui, ui.Toolbar));
	if_fail_fe(llc::controlMetricsInvalidate(gui, ui.Processes));
	if_fail_fe(llc::controlMetricsInvalidate(gui, ui.Services));
	if_fail_fe(llsProcessLayout(gui, ui.Processes));
	rtrn llsServiceLayout(gui, ui.Services);
}

sttc llc::err_t llsRootActivate(SLLSGUI & ui, llc::cid_t root) {
	auto & gui = *ui.Framework.GUI;
	gui.Controls.SetHidden(root, false);
	if_fail_fe(llc::controlSetParent(gui, root, llc::CID_INVALID));
	rtrn llc::controlSetParent(gui, root, ui.Host);
}

using TLLSRootLayout = llc::err_t (*)(llc::SGUI &, llc::cid_t);

sttc llc::err_t llsRootUpdate(SLLSGUI & ui, llc::cid_t root, cnst llc::n2s1_t & minimum, TLLSRootLayout layout) {
	auto & gui = *ui.Framework.GUI;
	if(gui.Controls.IsHidden(root)) rtrn 0;
	auto & input = *ui.Framework.RootWindow.Input;
	cnst llc::cid_t title = llsChild(gui, root, LLS_VIEWPORT_Title);
	cnst llc::cid_t close = llsChild(gui, root, LLS_VIEWPORT_Close);
	cnst llc::cid_t target = llsChild(gui, root, LLS_VIEWPORT_Target);
	cnst llc::cid_t resize = llsChild(gui, root, LLS_VIEWPORT_Resize);
	if(gui.Controls.IsPressed(root) || gui.Controls.IsPressed(title) || gui.Controls.IsPressed(target)) if_fail_fe(llsRootActivate(ui, root));
	if(gui.Controls.IsAction(close)) { gui.Controls.SetHidden(root, true); rtrn 0; }
	if(gui.Controls.IsPressed(title) && (input.MouseCurrent.Deltas.x || input.MouseCurrent.Deltas.y)) {
		gui.Controls.Placement[root].Area.Offset += {(llc::s1_t)input.MouseCurrent.Deltas.x, (llc::s1_t)input.MouseCurrent.Deltas.y};
		if_fail_fe(llc::controlMetricsInvalidate(gui, root));
	}
	if(gui.Controls.IsPressed(resize) && (input.MouseCurrent.Deltas.x || input.MouseCurrent.Deltas.y)) {
		auto & size = gui.Controls.Placement[root].Area.Size;
		size.x = llc::max(minimum.x, (llc::s1_t)(size.x + input.MouseCurrent.Deltas.x));
		size.y = llc::max(minimum.y, (llc::s1_t)(size.y + input.MouseCurrent.Deltas.y));
		if_fail_fe(layout(gui, root));
		if_fail_fe(llc::controlMetricsInvalidate(gui, root));
	}
	rtrn 0;
}

sttc llc::err_t llsTextRelease(SLLSGUI & ui, cnst llc::acid & controls) {
	// controlTextSet borrows storage and compares the old view. Detach before its owner changes.
	for(cnst auto id : controls) { if_fail_fe(llc::controlTextSet(*ui.Framework.GUI, id, {})); }
	rtrn 0;
}

sttc llc::err_t llsDoubleClick(cnst llc::TQueueSystemEvent & events, bool & doubleClick) {
	doubleClick = false;
	for(llc::u2_t iEvent = 0; iEvent < events.size(); ++iEvent) {
		cnst llc::pobj<llc::SEventSystem> & eventSystem = events[iEvent];
		if(!eventSystem.get_ref() || eventSystem->Type != llc::SYSTEM_EVENT_Mouse) continue;
		llc::SEView<llc::EVENT_MOUSE> eventMouse = {};
		if_fail_fe(eventSystem->ExtractChild(eventMouse));
		if(eventMouse.Type != llc::EVENT_MOUSE_DoubleClick) continue;
		llc::vcu0_t input = eventMouse.Data;
		llc::u0_t button = 0;
		if_fail_fe(llc::loadPOD(input, button));
		if_true_fe(input.size());
		if(!button) doubleClick = true;
	}
	rtrn 0;
}

sttc llc::err_t llsViewportCreate(SLLSGUI & ui, LLS_GUI_VIEWPORT index, llc::vcst_t title, cnst llc::rect2s1_t & area) {
	auto & gui = *ui.Framework.GUI;
	auto & viewport = ui.Viewports[index];
	if_fail_fe(llc::guiCreateViewport(gui, viewport, ui.Host, ui.ViewportActions[index]));
	auto & root = gui.Controls.Placement[viewport.IdControl];
	root.Area = area;
	root.Border = {3, 3, 3, 3};
	root.Margin = {1, 1, 1, 1};
	cnst llc::cid_t idTitle = viewport.IdControls[llc::VIEWPORT_CONTROL_TITLE];
	cnst llc::cid_t idClient = viewport.IdControls[llc::VIEWPORT_CONTROL_TARGET];
	gui.Controls.Text[idTitle].Text = title;
	gui.Controls.Text[idTitle].FontSelected = 7;
	gui.Controls.Placement[idTitle].Border = {2, 2, 2, 2};
	gui.Controls.Placement[idClient].Area.Size = {
		(llc::s1_t)(area.Size.x > 8 ? area.Size.x - 8 : 1),
		(llc::s1_t)(area.Size.y > 32 ? area.Size.y - 32 : 1),
	};
	gui.Controls.Placement[idClient].Border = {1, 1, 1, 1};
	viewport.UnfoldedSize = area.Size;
	rtrn llc::controlMetricsInvalidate(gui, viewport.IdControl);
}

sttc llc::err_t llsViewportPlace(SLLSGUI & ui, LLS_GUI_VIEWPORT index, cnst llc::rect2s1_t & area) {
	auto & gui = *ui.Framework.GUI;
	auto & viewport = ui.Viewports[index];
	gui.Controls.Placement[viewport.IdControl].Area = area;
	gui.Controls.Placement[viewport.IdControls[llc::VIEWPORT_CONTROL_TARGET]].Area.Size = {
		(llc::s1_t)(area.Size.x > 8 ? area.Size.x - 8 : 1),
		(llc::s1_t)(area.Size.y > 32 ? area.Size.y - 32 : 1),
	};
	viewport.UnfoldedSize = area.Size;
	rtrn llc::controlMetricsInvalidate(gui, viewport.IdControl);
}

sttc llc::err_t llsViewportArrange(SLLSGUI & ui) {
	cnst auto size = ui.Framework.RootWindow.Size;
	cnst llc::s1_t width = (llc::s1_t)llc::max(900U, (llc::u2_t)size.x);
	cnst llc::s1_t height = (llc::s1_t)llc::max(620U, (llc::u2_t)size.y);
	cnst llc::s1_t contentTop = 202;
	cnst llc::s1_t contentHeight = (llc::s1_t)(height - contentTop - 42);
	cnst llc::s1_t workspaceWidth = (llc::s1_t)llc::min(440U, llc::max(360U, (llc::u2_t)width / 3));
	if_fail_fe(llsViewportPlace(ui, LLS_GUI_VIEWPORT_SESSION, {{18, 70}, {(llc::s1_t)(width - 36), 118}}));
	if_fail_fe(llsViewportPlace(ui, LLS_GUI_VIEWPORT_WORKSPACE, {{18, contentTop}, {workspaceWidth, contentHeight}}));
	rtrn llsViewportPlace(ui, LLS_GUI_VIEWPORT_INSPECTOR, {{(llc::s1_t)(workspaceWidth + 30), contentTop}, {(llc::s1_t)(width - workspaceWidth - 48), contentHeight}});
}

sttc llc::err_t llsDetailClear(SLLSGUI & ui) {
	if_fail_fe(llsTextRelease(ui, ui.Lines));
	if_fail_fe(llc::clear(ui.DetailLines, ui.DetailPaths));
	rtrn ui.Detail.clear();
}

sttc llc::err_t llsTextLines(llc::vcst_t text, llc::u2_t columns, llc::aobj<llc::vcst_t> & lines) {
	if_fail_fe(lines.clear());
	llc::vcst_t remaining = text;
	while(remaining.size()) {
		llc::u2_t count = 0;
		while(count < remaining.size() && remaining[count] != '\n' && count < columns) ++count;
		llc::u2_t consumed = count;
		if(count < remaining.size() && remaining[count] == '\n') ++consumed;
		else if(count == columns) {
			for(llc::u2_t word = count; word > count / 2; --word)
				if(remaining[word - 1] == ' ') { count = word - 1; consumed = word; break; }
		}
		if_fail_fe(lines.push_back({remaining.begin(), count}));
		if_fail_fe(remaining.slice(remaining, consumed));
	}
	rtrn lines.size();
}

sttc llc::err_t llsLines(SLLSGUI & ui) {
	if_fail_fe(llsTextLines(ui.Detail, ui.Columns, ui.DetailLines));
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
	if_fail_fe(llsRootArrange(ui));
	cnst auto size = ui.Framework.RootWindow.Size;
	cnst llc::s1_t width = (llc::s1_t)llc::max(900U, (llc::u2_t)size.x);
	cnst llc::s1_t height = (llc::s1_t)llc::max(620U, (llc::u2_t)size.y);
	cnst llc::cid_t sessionClient = ui.Viewports[LLS_GUI_VIEWPORT_SESSION].IdControls[llc::VIEWPORT_CONTROL_TARGET];
	cnst llc::cid_t workspaceClient = ui.Viewports[LLS_GUI_VIEWPORT_WORKSPACE].IdControls[llc::VIEWPORT_CONTROL_TARGET];
	cnst llc::cid_t inspectorClient = ui.Viewports[LLS_GUI_VIEWPORT_INSPECTOR].IdControls[llc::VIEWPORT_CONTROL_TARGET];
	cnst llc::n2s1_t sessionSize = gui.Controls.Placement[sessionClient].Area.Size;
	cnst llc::n2s1_t workspaceSize = gui.Controls.Placement[workspaceClient].Area.Size;
	cnst llc::n2s1_t inspectorSize = gui.Controls.Placement[inspectorClient].Area.Size;
	cnst llc::s1_t sessionButtons = 512;
	if_fail_fe(llsPlace(gui, ui.Title, 10, 6, (llc::s1_t)llc::max(120, sessionSize.x - sessionButtons - 20), 28));
	if_fail_fe(llsPlace(gui, ui.StopService, (llc::s1_t)(sessionSize.x - 522), 6, 154, 28));
	if_fail_fe(llsPlace(gui, ui.Restart, (llc::s1_t)(sessionSize.x - 360), 6, 164, 28));
	if_fail_fe(llsPlace(gui, ui.Exit, (llc::s1_t)(sessionSize.x - 188), 6, 178, 28));
	if_fail_fe(llsPlace(gui, ui.Statistics, 10, 40, (llc::s1_t)(sessionSize.x - 20), 26));
	cnst llc::s1_t tabColumns = 3;
	cnst llc::s1_t tabWidth = (llc::s1_t)llc::max(90, (workspaceSize.x - 20) / tabColumns);
	for(llc::u2_t i = 0; i < ui.Tabs.size(); ++i) {
		cnst llc::s1_t column = (llc::s1_t)(i % tabColumns);
		cnst llc::s1_t row = (llc::s1_t)(i / tabColumns);
		if_fail_fe(llsPlace(gui, ui.Tabs[i], (llc::s1_t)(10 + column * tabWidth), (llc::s1_t)(8 + row * 30), (llc::s1_t)(tabWidth - 6), 28));
	}
	if_fail_fe(llsPlace(gui, ui.Input.IdRoot, 10, 104, (llc::s1_t)(workspaceSize.x - 20), 30));
	if_fail_fe(llsPlace(gui, ui.Input.IdText, 0, 0, (llc::s1_t)(workspaceSize.x - 20), 30));
	cnst llc::s1_t actionWidth = (llc::s1_t)((workspaceSize.x - 26) / 2);
	if_fail_fe(llsPlace(gui, ui.OpenPath, 10, 138, actionWidth, 28));
	gui.Controls.Text[ui.Input.IdText].Align = llc::ALIGN_CENTER_LEFT;
	if_fail_fe(llsPlace(gui, ui.Keyboard, (llc::s1_t)(16 + actionWidth), 138, actionWidth, 28));
	if_fail_fe(llsPlace(gui, ui.Hint, 18, height - 34, width - 36, 26));
	auto & keyboard = gui.Controls.Placement[ui.Input.VirtualKeyboard.IdRoot];
	keyboard.Align = llc::ALIGN_TOP_LEFT;
	keyboard.Area.Offset = {0, 34};
	if_fail_fe(llc::controlMetricsInvalidate(gui, ui.Input.VirtualKeyboard.IdRoot));
	gui.Controls.SetHidden(ui.Input.VirtualKeyboard.IdRoot, !ui.ShowKeyboard);
	cnst llc::s1_t workspaceTop = ui.Tab == 4 ? 204 : 174;
	cnst llc::s1_t workspaceBottom = (llc::s1_t)(workspaceSize.y - 36 - (ui.ShowKeyboard ? 230 : 0));
	for(llc::u2_t i = 0; i < ui.Rows.size(); ++i) {
		if_fail_fe(llsPlace(gui, ui.Rows[i], 10, (llc::s1_t)(workspaceTop + 30 * i), (llc::s1_t)(workspaceSize.x - 20), 28));
	}
	ui.VisibleRows = llc::min(ui.Rows.size(), (llc::u2_t)llc::max(1, (workspaceBottom - workspaceTop) / 30));
	ui.VisibleLines = llc::min((llc::u2_t)ui.Lines.size(), (llc::u2_t)llc::max(1, (inspectorSize.y - 50) / 19));
	ui.Columns = llc::max(1U, (llc::u2_t)(inspectorSize.x - 24) / gui.Fonts[gui.SelectedFont]->CharSize.x);
	for(llc::u2_t i = 0; i < ui.Lines.size(); ++i)
		if_fail_fe(llsPlace(gui, ui.Lines[i], 10, (llc::s1_t)(8 + 19 * i), (llc::s1_t)(inspectorSize.x - 20), 19));
	if_fail_fe(llsPlace(gui, ui.Benchmark, (llc::s1_t)(inspectorSize.x - 154), 8, 144, 28));
	if_fail_fe(llsPlace(gui, ui.ListPrev, 10, (llc::s1_t)(workspaceSize.y - 32), 90, 26));
	if_fail_fe(llsPlace(gui, ui.ListNext, 106, (llc::s1_t)(workspaceSize.y - 32), 90, 26));
	if_fail_fe(llsPlace(gui, ui.PagePrev, 10, (llc::s1_t)(inspectorSize.y - 32), 110, 26));
	if_fail_fe(llsPlace(gui, ui.PageNext, 126, (llc::s1_t)(inspectorSize.y - 32), 110, 26));
	ui.LastSize = size;
	rtrn llsLines(ui);
}

sttc llc::err_t llsViewportsUpdate(SLLSGUI & ui, bool doubleClick) {
	auto & gui = *ui.Framework.GUI;
	auto & input = *ui.Framework.RootWindow.Input;
	llc::u2_t hovered = ui.Viewports.size();
	bool resized = false;
	cnst llc::n2s1_t minimums[LLS_GUI_VIEWPORT_COUNT] = {{620, 90}, {300, 300}, {360, 300}};
	for(llc::u0_t i = 0; i < ui.Viewports.size(); ++i) {
		auto & viewport = ui.Viewports[i];
		cnst llc::cid_t title = viewport.IdControls[llc::VIEWPORT_CONTROL_TITLE];
		cnst llc::cid_t close = viewport.IdControls[llc::VIEWPORT_CONTROL_CLOSE];
		cnst llc::cid_t resize = viewport.IdControls[llc::VIEWPORT_CONTROL_RESIZE_BOTTOM_RIGHT];
		if(llc::viewportIsPressed(gui, viewport)) if_fail_fe(llc::viewportActivate(gui, ui.Host, ui.Viewports, i));
		if(gui.Controls.IsAction(close)) gui.Controls.SetHidden(viewport.IdControl, true);
		if(gui.Controls.IsPressed(title) && (input.MouseCurrent.Deltas.x || input.MouseCurrent.Deltas.y)) {
			gui.Controls.Placement[viewport.IdControl].Area.Offset += {(llc::s1_t)input.MouseCurrent.Deltas.x, (llc::s1_t)input.MouseCurrent.Deltas.y};
			if_fail_fe(llc::controlMetricsInvalidate(gui, viewport.IdControl));
		}
		if(!viewport.Folded && gui.Controls.IsPressed(resize) && (input.MouseCurrent.Deltas.x || input.MouseCurrent.Deltas.y)) {
			auto & root = gui.Controls.Placement[viewport.IdControl];
			auto & client = gui.Controls.Placement[viewport.IdControls[llc::VIEWPORT_CONTROL_TARGET]];
			cnst llc::n2s1_t previous = root.Area.Size;
			root.Area.Size.x = llc::max(minimums[i].x, (llc::s1_t)(root.Area.Size.x + input.MouseCurrent.Deltas.x));
			root.Area.Size.y = llc::max(minimums[i].y, (llc::s1_t)(root.Area.Size.y + input.MouseCurrent.Deltas.y));
			client.Area.Size.x += root.Area.Size.x - previous.x;
			client.Area.Size.y += root.Area.Size.y - previous.y;
			viewport.UnfoldedSize = root.Area.Size;
			resized = true;
			if_fail_fe(llc::controlMetricsInvalidate(gui, viewport.IdControl));
		}
		if(gui.Controls.IsHovered(title)) hovered = i;
	}
	if(doubleClick && hovered < ui.Viewports.size()) if_fail_fe(llc::viewportFold(gui, ui.Viewports[hovered]));
	if(resized) { if_fail_fe(llsLayout(ui)); ui.Dirty = true; }
	rtrn 0;
}

sttc llc::err_t llsExplorerCreate(SLLSGUI & ui, llc::vcst_t path) {
	auto & gui = *ui.Framework.GUI;
	cnst llc::u2_t cascade = ui.Explorers.size();
	llc::cid_t root = llc::CID_INVALID;
	if_fail_fe(llsRootCreate<EXPLORER_VIEWPORT>(ui, root, "EXPLORER", {{(llc::s1_t)(72 + 28 * (cascade % 8)), (llc::s1_t)(86 + 24 * (cascade % 6))}, {590, 650}}));
	cnst llc::u2_t properties = (llc::u2_t)root + 1;
	if(root >= (llc::cid_t)ui.ExplorerState.size()) {
		if_fail_fe(ui.ExplorerState.resize(properties));
		if_fail_fe(ui.ExplorerText.resize(properties));
		if_fail_fe(ui.ExplorerOffset.resize(properties));
	}
	if_fail_fe(ui.ExplorerText[root].resize(LLS_EXPLORER_ROWS));
	ui.ExplorerOffset[root] = 0;
	if_fail_fe(llsExplorerOpen(ui.ExplorerState[root], path));
	if_fail_fe(ui.Explorers.push_back(root));
	cnst llc::cid_t target = llsChild(gui, root, LLS_VIEWPORT_Target);
	cnst llc::cid_t pathId = llsChild(gui, target, EXPLORER_VIEWPORT_Path);
	cnst llc::cid_t summaryId = llsChild(gui, target, EXPLORER_VIEWPORT_Summary);
	gui.Controls.Draw[pathId].NoBorder = gui.Controls.Draw[pathId].NoClient = true;
	gui.Controls.Draw[summaryId].NoBorder = gui.Controls.Draw[summaryId].NoClient = true;
	gui.Controls.Text[pathId].Align = llc::ALIGN_CENTER_LEFT;
	gui.Controls.Text[summaryId].Align = llc::ALIGN_TOP_LEFT;
	if_fail_fe(llc::controlTextSet(gui, llsChild(gui, target, EXPLORER_VIEWPORT_Previous), "< Page"));
	if_fail_fe(llc::controlTextSet(gui, llsChild(gui, target, EXPLORER_VIEWPORT_Next), "Page >"));
	if_fail_fe(llc::controlTextSet(gui, summaryId, "Click: expand. Double-click: new Explorer / launch."));
	if_fail_fe(llsExplorerLayout(gui, root));
	if_fail_fe(llc::controlMetricsInvalidate(gui, root));
	rtrn llsRootActivate(ui, root);
}

sttc llc::err_t llsExplorerUpdate(SApplication & app, bool doubleClick) {
	auto & ui = *app.UI;
	auto & gui = *ui.Framework.GUI;
	cnst llc::u2_t count = ui.Explorers.size();
	for(llc::u2_t iRoot = 0; iRoot < count; ++iRoot) {
		cnst llc::cid_t root = ui.Explorers[iRoot];
		if_fail_fe(llsRootUpdate(ui, root, {420, 600}, llsExplorerLayout));
		if(gui.Controls.IsHidden(root)) continue;
		cnst llc::cid_t target = llsChild(gui, root, LLS_VIEWPORT_Target);
		SExplorer & explorer = ui.ExplorerState[root];
		llc::u2_t & offset = ui.ExplorerOffset[root];
		if(gui.Controls.IsAction(llsChild(gui, target, EXPLORER_VIEWPORT_Previous)))
			offset = offset > LLS_EXPLORER_ROWS ? offset - LLS_EXPLORER_ROWS : 0;
		if(gui.Controls.IsAction(llsChild(gui, target, EXPLORER_VIEWPORT_Next)) && offset + LLS_EXPLORER_ROWS < explorer.Rows.size())
			offset += LLS_EXPLORER_ROWS;
		for(llc::u2_t row = 0; row < LLS_EXPLORER_ROWS; ++row) {
			cnst llc::cid_t rowId = llsExplorerRow(gui, root, row);
			cnst bool activated = gui.Controls.IsAction(rowId) || (doubleClick && gui.Controls.IsHovered(rowId));
			if(!activated || offset + row >= explorer.Rows.size()) continue;
			if_fail_fe(llsRootActivate(ui, root));
			if_fail_fe(llsExplorerSelect(explorer, offset + row));
			if(offset >= explorer.Rows.size()) offset = 0;
			if(doubleClick && gui.Controls.IsHovered(rowId) && explorer.SelectedPath.size()) {
				llc::string selectedPath = explorer.SelectedPath;
				if(explorer.SelectedFolder) { if_fail_fe(llsExplorerCreate(ui, selectedPath)); }
				else {
					cnst llc::err_t queued = llsServiceQueue(app, "launch", selectedPath);
					if(llc::failed(queued)) {
						llc::lock_guard guard(app.Lock);
						if_fail_fe(app.ServiceResponse.clear());
						if_fail_fe(::llc::append_string(app.ServiceResponse, "A service control operation is already running."));
						++app.Revision;
					}
				}
			}
			break;
		}
	}
	rtrn 0;
}

sttc llc::err_t llsHelpSelect(SLLSGUI & ui, llc::u2_t topic) {
	if_true_fe(topic >= ui.Paths.size());
	ui.HelpSelected = topic;
	ui.HelpLineOffset = 0;
	auto & gui = *ui.Framework.GUI;
	cnst llc::cid_t target = llsChild(gui, ui.Help, LLS_VIEWPORT_Target);
	cnst llc::s1_t width = gui.Controls.Placement[target].Area.Size.x;
	cnst llc::s1_t topicWidth = llc::min((llc::s1_t)280, (llc::s1_t)(width / 3));
	cnst llc::s1_t contentWidth = llc::max((llc::s1_t)1, (llc::s1_t)(width - topicWidth - 24));
	cnst llc::u1_t font = gui.Controls.Text[llsHelpLine(gui, ui.Help, 0)].FontSelected;
	cnst llc::u2_t characterCount = (llc::u2_t)contentWidth / gui.Fonts[font]->CharSize.x;
	ui.HelpColumns = characterCount > 1 ? characterCount - 1 : 1;
	rtrn llsTextLines(help(ui.Paths[topic], true), ui.HelpColumns, ui.HelpLines);
}

sttc llc::err_t llsHelpCreate(SLLSGUI & ui) {
	auto & gui = *ui.Framework.GUI;
	if(ui.Help != llc::CID_INVALID) rtrn llsRootActivate(ui, ui.Help);
	if_fail_fe(llsRootCreate<HELP_VIEWPORT>(ui, ui.Help, "HELP", {{138, 92}, {920, 650}}));
	cnst llc::cid_t target = llsChild(gui, ui.Help, LLS_VIEWPORT_Target);
	for(llc::u2_t line = 0; line < LLS_HELP_LINES; ++line) {
		cnst llc::cid_t id = llsHelpLine(gui, ui.Help, line);
		gui.Controls.Draw[id].NoBorder = gui.Controls.Draw[id].NoClient = true;
		gui.Controls.Text[id].Align = llc::ALIGN_CENTER_LEFT;
	}
	if_fail_fe(llc::controlTextSet(gui, llsChild(gui, target, HELP_VIEWPORT_TopicPrevious), "< Topics"));
	if_fail_fe(llc::controlTextSet(gui, llsChild(gui, target, HELP_VIEWPORT_TopicNext), "Topics >"));
	if_fail_fe(llc::controlTextSet(gui, llsChild(gui, target, HELP_VIEWPORT_PagePrevious), "< Page"));
	if_fail_fe(llc::controlTextSet(gui, llsChild(gui, target, HELP_VIEWPORT_PageNext), "Page >"));
	if_fail_fe(llsHelpLayout(gui, ui.Help));
	if(ui.Paths.size()) { if_fail_fe(llsHelpSelect(ui, 0)); }
	if_fail_fe(llc::controlMetricsInvalidate(gui, ui.Help));
	rtrn llsRootActivate(ui, ui.Help);
}

sttc llc::err_t llsHelpUpdate(SLLSGUI & ui) {
	if(ui.Help == llc::CID_INVALID) rtrn 0;
	if_fail_fe(llsRootUpdate(ui, ui.Help, {660, 600}, llsHelpLayout));
	auto & gui = *ui.Framework.GUI;
	if(gui.Controls.IsHidden(ui.Help)) rtrn 0;
	cnst llc::cid_t target = llsChild(gui, ui.Help, LLS_VIEWPORT_Target);
	cnst llc::s1_t width = gui.Controls.Placement[target].Area.Size.x;
	cnst llc::s1_t topicWidth = llc::min((llc::s1_t)280, (llc::s1_t)(width / 3));
	cnst llc::s1_t contentWidth = llc::max((llc::s1_t)1, (llc::s1_t)(width - topicWidth - 24));
	cnst llc::u1_t font = gui.Controls.Text[llsHelpLine(gui, ui.Help, 0)].FontSelected;
	cnst llc::u2_t characterCount = (llc::u2_t)contentWidth / gui.Fonts[font]->CharSize.x;
	cnst llc::u2_t columns = characterCount > 1 ? characterCount - 1 : 1;
	if(columns != ui.HelpColumns) { if_fail_fe(llsHelpSelect(ui, ui.HelpSelected)); }
	if(gui.Controls.IsAction(llsChild(gui, target, HELP_VIEWPORT_TopicPrevious)))
		ui.HelpTopicOffset = ui.HelpTopicOffset > LLS_HELP_TOPICS ? ui.HelpTopicOffset - LLS_HELP_TOPICS : 0;
	if(gui.Controls.IsAction(llsChild(gui, target, HELP_VIEWPORT_TopicNext)) && ui.HelpTopicOffset + LLS_HELP_TOPICS < ui.Paths.size())
		ui.HelpTopicOffset += LLS_HELP_TOPICS;
	for(llc::u2_t row = 0; row < LLS_HELP_TOPICS; ++row) {
		cnst llc::u2_t topic = ui.HelpTopicOffset + row;
		cnst llc::cid_t id = llsHelpTopic(gui, ui.Help, row);
		if(topic >= ui.Paths.size() || !gui.Controls.IsAction(id)) continue;
		if_fail_fe(llsRootActivate(ui, ui.Help));
		if_fail_fe(llsHelpSelect(ui, topic));
		break;
	}
	if(gui.Controls.IsAction(llsChild(gui, target, HELP_VIEWPORT_PagePrevious)))
		ui.HelpLineOffset = ui.HelpLineOffset > LLS_HELP_LINES ? ui.HelpLineOffset - LLS_HELP_LINES : 0;
	if(gui.Controls.IsAction(llsChild(gui, target, HELP_VIEWPORT_PageNext)) && ui.HelpLineOffset + LLS_HELP_LINES < ui.HelpLines.size())
		ui.HelpLineOffset += LLS_HELP_LINES;
	rtrn 0;
}

sttc llc::err_t llsToolbarUpdate(SApplication & app) {
	auto & ui = *app.UI;
	auto & gui = *ui.Framework.GUI;
	cnst auto & buttons = gui.Controls.Children[ui.Toolbar];
	for(llc::u2_t i = 0; i < llc::get_value_count<LLS_TOOLBAR>(); ++i) {
		if(!gui.Controls.IsAction(buttons[i])) continue;
		switch((LLS_TOOLBAR)i) {
		case LLS_TOOLBAR_Explorer:
			{
				llc::vcst_t path;
				if_fail_fe(llc::jsonExpressionResolve("explorer.root", ui.Layout.Reader, 0, path));
				if_fail_fe(llsExplorerCreate(ui, path));
			}
			break;
		case LLS_TOOLBAR_Help:
			if_fail_fe(llsHelpCreate(ui));
			break;
		case LLS_TOOLBAR_Processes: if_fail_fe(llsRootActivate(ui, ui.Processes)); break;
		case LLS_TOOLBAR_Services : if_fail_fe(llsRootActivate(ui, ui.Services )); break;
		default: if_true_fe(true);
		}
	}
	bool explorerVisible = false;
	for(cnst llc::cid_t root : ui.Explorers) explorerVisible |= !gui.Controls.IsHidden(root);
	gui.Controls.SetSelected(buttons[LLS_TOOLBAR_Explorer ], explorerVisible);
	gui.Controls.SetSelected(buttons[LLS_TOOLBAR_Help     ], ui.Help != llc::CID_INVALID && !gui.Controls.IsHidden(ui.Help));
	gui.Controls.SetSelected(buttons[LLS_TOOLBAR_Processes], !gui.Controls.IsHidden(ui.Processes));
	gui.Controls.SetSelected(buttons[LLS_TOOLBAR_Services ], !gui.Controls.IsHidden(ui.Services));
	rtrn 0;
}

sttc llc::err_t llsExplorerTextAttach(SLLSGUI & ui) {
	auto & gui = *ui.Framework.GUI;
	for(cnst llc::cid_t root : ui.Explorers) {
		if(gui.Controls.IsHidden(root)) continue;
		cnst llc::cid_t target = llsChild(gui, root, LLS_VIEWPORT_Target);
		SExplorer & explorer = ui.ExplorerState[root];
		llc::u2_t & offset = ui.ExplorerOffset[root];
		if(offset >= explorer.Rows.size()) offset = 0;
		if_fail_fe(llc::controlTextSet(gui, llsChild(gui, target, EXPLORER_VIEWPORT_Path), explorer.SelectedPath));
		for(llc::u2_t row = 0; row < LLS_EXPLORER_ROWS; ++row) {
			cnst llc::cid_t rowId = llsExplorerRow(gui, root, row);
			cnst llc::u2_t entry = offset + row;
			cnst bool visible = entry < explorer.Rows.size();
			gui.Controls.SetHidden(rowId, !visible);
			gui.Controls.SetSelected(rowId, visible && explorer.Selected == entry);
			if_fail_fe(ui.ExplorerText[root][row].clear());
			if(!visible) continue;
			if_fail_fe(llsExplorerLabel(explorer, entry, ui.ExplorerText[root][row]));
			if_fail_fe(llc::controlTextSet(gui, rowId, ui.ExplorerText[root][row]));
		}
		gui.Controls.SetDisabled(llsChild(gui, target, EXPLORER_VIEWPORT_Previous), offset == 0);
		gui.Controls.SetDisabled(llsChild(gui, target, EXPLORER_VIEWPORT_Next), offset + LLS_EXPLORER_ROWS >= explorer.Rows.size());
	}
	rtrn 0;
}

sttc llc::err_t llsExplorerTextDetach(SLLSGUI & ui) {
	auto & gui = *ui.Framework.GUI;
	for(cnst llc::cid_t root : ui.Explorers) {
		cnst llc::cid_t target = llsChild(gui, root, LLS_VIEWPORT_Target);
		if_fail_fe(llc::controlTextSet(gui, llsChild(gui, target, EXPLORER_VIEWPORT_Path), {}));
		for(llc::u2_t row = 0; row < LLS_EXPLORER_ROWS; ++row)
			if_fail_fe(llc::controlTextSet(gui, llsExplorerRow(gui, root, row), {}));
	}
	rtrn 0;
}

sttc llc::err_t llsHelpTextAttach(SLLSGUI & ui) {
	if(ui.Help == llc::CID_INVALID || ui.Framework.GUI->Controls.IsHidden(ui.Help)) rtrn 0;
	auto & gui = *ui.Framework.GUI;
	cnst llc::cid_t target = llsChild(gui, ui.Help, LLS_VIEWPORT_Target);
	if(ui.HelpTopicOffset >= ui.Paths.size()) ui.HelpTopicOffset = 0;
	if(ui.HelpLineOffset >= ui.HelpLines.size()) ui.HelpLineOffset = 0;
	for(llc::u2_t row = 0; row < LLS_HELP_TOPICS; ++row) {
		cnst llc::cid_t id = llsHelpTopic(gui, ui.Help, row);
		cnst llc::u2_t topic = ui.HelpTopicOffset + row;
		cnst bool visible = topic < ui.Paths.size();
		gui.Controls.SetHidden(id, !visible);
		gui.Controls.SetSelected(id, visible && topic == ui.HelpSelected);
		if(visible) { if_fail_fe(llc::controlTextSet(gui, id, ui.Paths[topic].size() ? ui.Paths[topic] : llc::vcst_t{"Help / index"})); }
	}
	for(llc::u2_t row = 0; row < LLS_HELP_LINES; ++row) {
		cnst llc::cid_t id = llsHelpLine(gui, ui.Help, row);
		cnst llc::u2_t line = ui.HelpLineOffset + row;
		cnst bool visible = line < ui.HelpLines.size();
		gui.Controls.SetHidden(id, !visible);
		if(visible) { if_fail_fe(llc::controlTextSet(gui, id, ui.HelpLines[line])); }
	}
	gui.Controls.SetDisabled(llsChild(gui, target, HELP_VIEWPORT_TopicPrevious), ui.HelpTopicOffset == 0);
	gui.Controls.SetDisabled(llsChild(gui, target, HELP_VIEWPORT_TopicNext), ui.HelpTopicOffset + LLS_HELP_TOPICS >= ui.Paths.size());
	gui.Controls.SetDisabled(llsChild(gui, target, HELP_VIEWPORT_PagePrevious), ui.HelpLineOffset == 0);
	gui.Controls.SetDisabled(llsChild(gui, target, HELP_VIEWPORT_PageNext), ui.HelpLineOffset + LLS_HELP_LINES >= ui.HelpLines.size());
	rtrn 0;
}

sttc llc::err_t llsHelpTextDetach(SLLSGUI & ui) {
	if(ui.Help == llc::CID_INVALID) rtrn 0;
	auto & gui = *ui.Framework.GUI;
	for(llc::u2_t row = 0; row < LLS_HELP_TOPICS; ++row)
		if_fail_fe(llc::controlTextSet(gui, llsHelpTopic(gui, ui.Help, row), {}));
	for(llc::u2_t row = 0; row < LLS_HELP_LINES; ++row)
		if_fail_fe(llc::controlTextSet(gui, llsHelpLine(gui, ui.Help, row), {}));
	rtrn 0;
}

sttc llc::err_t llsFeatureTextAttach(SApplication & app) {
	auto & ui = *app.UI;
	auto & gui = *ui.Framework.GUI;
	cnst llc::cid_t processTarget = llsChild(gui, ui.Processes, LLS_VIEWPORT_Target);
	llc::u2_t row = 0;
	for(cnst SDashboardJob & job : app.Jobs) {
		cnst bool active = !job.Historical && job.ProcessStarted && !job.ProcessFinished && !job.Detached && job.State != "completed";
		if(!active) continue;
		if(row >= llc::get_value_count<PROCESS_VIEWPORT>()) break;
		cnst llc::cid_t id = gui.Controls.Children[processTarget][row++];
		gui.Controls.SetHidden(id, false);
		llc::vcst_t text = job.Id;
		if(!text.size()) text = job.Summary;
		if_fail_fe(llc::controlTextSet(gui, id, text));
	}
	if(!row) {
		cnst llc::cid_t id = gui.Controls.Children[processTarget][row++];
		gui.Controls.SetHidden(id, false);
		if_fail_fe(llc::controlTextSet(gui, id, "No active broker processes"));
	}
	for(; row < llc::get_value_count<PROCESS_VIEWPORT>(); ++row) {
		cnst llc::cid_t id = gui.Controls.Children[processTarget][row];
		gui.Controls.SetHidden(id, true);
		if_fail_fe(llc::controlTextSet(gui, id, {}));
	}
	cnst llc::cid_t serviceTarget = llsChild(gui, ui.Services, LLS_VIEWPORT_Target);
	llc::vcst_t identity = app.ServiceIdentity, response = app.ServiceResponse;
	if(!identity.size()) identity = "Not reported";
	if(!response.size()) response = "No pending service operation";
	if_fail_fe(llc::controlTextSet(gui, llsChild(gui, serviceTarget, SERVICE_VIEWPORT_ServiceValue   ), app.Service));
	if_fail_fe(llc::controlTextSet(gui, llsChild(gui, serviceTarget, SERVICE_VIEWPORT_ConnectionValue), app.Connection));
	if_fail_fe(llc::controlTextSet(gui, llsChild(gui, serviceTarget, SERVICE_VIEWPORT_IdentityValue  ), identity));
	if_fail_fe(llc::controlTextSet(gui, llsChild(gui, serviceTarget, SERVICE_VIEWPORT_ResponseValue  ), response));
	if_fail_fe(llsExplorerTextAttach(ui));
	rtrn llsHelpTextAttach(ui);
}

sttc llc::err_t llsFeatureTextDetach(SLLSGUI & ui) {
	auto & gui = *ui.Framework.GUI;
	if_fail_fe(llsHelpTextDetach(ui));
	if_fail_fe(llsExplorerTextDetach(ui));
	cnst llc::cid_t processTarget = llsChild(gui, ui.Processes, LLS_VIEWPORT_Target);
	for(llc::cid_t id : gui.Controls.Children[processTarget]) if_fail_fe(llc::controlTextSet(gui, id, {}));
	cnst llc::cid_t serviceTarget = llsChild(gui, ui.Services, LLS_VIEWPORT_Target);
	if_fail_fe(llc::controlTextSet(gui, llsChild(gui, serviceTarget, SERVICE_VIEWPORT_ServiceValue   ), {}));
	if_fail_fe(llc::controlTextSet(gui, llsChild(gui, serviceTarget, SERVICE_VIEWPORT_ConnectionValue), {}));
	if_fail_fe(llc::controlTextSet(gui, llsChild(gui, serviceTarget, SERVICE_VIEWPORT_IdentityValue  ), {}));
	rtrn llc::controlTextSet(gui, llsChild(gui, serviceTarget, SERVICE_VIEWPORT_ResponseValue), {});
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
			else { if_fail_fe(::llc::append_string(ui.Detail, "LCP CONSOLE\n\nChoose a template to load its JSON into the edit box. Edit it if needed, then press Enter or Send JSON.")); }
		}
		else if(ui.Tab == 6) {
			if_fail_fe(llc::append_strings(ui.Detail, "DASHBOARD\nSERVICE / ", app.Service, "\n", app.Connection,
				"\nSession: ", llc::str(app.SessionId), "\nSnapshot age: ",
				llc::str(app.LastSnapshot ? ((llc::u3_t)llc::timeCurrentInUs() - app.LastSnapshot) / 1000000 : 0), " s"));
			if(app.ServiceResponse.size()) { if_fail_fe(llc::append_strings(ui.Detail, "\n\nSERVICE CONTROL\n", app.ServiceResponse)); }
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
		if(!ui.Dirty && ui.Revision == app.Revision) rtrn 0;
		ui.Revision = app.Revision;
		if_fail_fe(ui.Matches.clear());
		for(llc::u2_t i = 0; i < ui.Explorer.Rows.size(); ++i) { if_fail_fe(ui.Matches.push_back(i)); }
		ui.Selected = ui.Explorer.Selected;
		if_fail_fe(llsDetailClear(ui));
		ui.Detail = ui.Explorer.Summary;
		if(app.ServiceResponse.size()) { if_fail_fe(llc::append_strings(ui.Detail, "\n\nSERVICE CONTROL\n", app.ServiceResponse)); }
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
		if(job.State == "interrupted") { if_fail_fe(::llc::append_string(ui.Detail, "Interrupted / outcome unknown. Not replayed.\n")); }
		if(job.State == "completed") {
			if_fail_fe(llc::append_strings(ui.Detail, "Operation status ", llc::str(job.Result.Status), "   Exit "));
			if(job.Result.ExitKnown) { if_fail_fe(llc::append_strings(ui.Detail, llc::str(job.Result.ExitCode))); }
			else { if_fail_fe(::llc::append_string(ui.Detail, "unknown")); }
		}
		if(job.Detached) { if_fail_fe(::llc::append_string(ui.Detail, "\nLaunch only: completion is not tracked.")); }
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
			if(ui.Tab == 3 || ui.Tab == 5) {
				if_fail_fe(llc::append_strings(ui.RowText[i], job.Summary, "  PID ", llc::str(job.PID)));
			}
			else {
				if_fail_fe(llc::append_strings(ui.RowText[i], job.Id, "  ", job.State, "  PID ", llc::str(job.PID)));
			}
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
	ui.Host = llc::controlCreate(gui);
	if_fail_fe(ui.Host);
	gui.Controls.Draw[ui.Host].NoBorder = true;
	gui.Controls.Draw[ui.Host].NoClient = true;
	gui.Controls.Placement[ui.Host].Area.Size = window.Size.s1();
	gui.Controls.Constraints[ui.Host].AttachSizeToControl = {ui.Host, ui.Host};
	if_fail_fe(llsToolbarCreate(ui));
	if_fail_fe(llsViewportCreate(ui, LLS_GUI_VIEWPORT_SESSION, "SESSION / SERVICE", {{18, 12}, {1144, 118}}));
	if_fail_fe(llsViewportCreate(ui, LLS_GUI_VIEWPORT_WORKSPACE, "LLS WORKSPACE", {{18, 144}, {390, 594}}));
	if_fail_fe(llsViewportCreate(ui, LLS_GUI_VIEWPORT_INSPECTOR, "INSPECTOR", {{420, 144}, {742, 594}}));
	if_fail_fe(llsViewportArrange(ui));
	if_fail_fe(llsRootCreate<PROCESS_VIEWPORT>(ui, ui.Processes, "PROCESSES", {{18, 202}, {680, 536}}));
	if_fail_fe(llsRootCreate<SERVICE_VIEWPORT>(ui, ui.Services, "SERVICES", {{710, 202}, {452, 536}}));
	cnst llc::cid_t sessionClient = ui.Viewports[LLS_GUI_VIEWPORT_SESSION].IdControls[llc::VIEWPORT_CONTROL_TARGET];
	cnst llc::cid_t workspaceClient = ui.Viewports[LLS_GUI_VIEWPORT_WORKSPACE].IdControls[llc::VIEWPORT_CONTROL_TARGET];
	cnst llc::cid_t inspectorClient = ui.Viewports[LLS_GUI_VIEWPORT_INSPECTOR].IdControls[llc::VIEWPORT_CONTROL_TARGET];
	if_fail_fe(llsControl(gui, ui.Title, app.Title, true, sessionClient));
	if_fail_fe(llsControl(gui, ui.Statistics, "", true, sessionClient));
	if_fail_fe(llsControl(gui, ui.Hint, "Type to filter. Select a result to inspect. Enter opens the first match. Mouse wheel scrolls details.", true));
	if_fail_fe(llsControlPool(gui, ui.Tabs, ui.TabText.size(), ui.Family, false, workspaceClient));
	for(llc::u2_t i = 0; i < ui.Tabs.size(); ++i) { if_fail_fe(llc::controlTextSet(gui, ui.Tabs[i], ui.TabText[i])); }
	if_fail_fe(llsControl(gui, ui.Exit, "Exit dashboard", false, sessionClient));
	if_fail_fe(llsControl(gui, ui.Restart, "Restart dashboard", false, sessionClient));
	if_fail_fe(llsControl(gui, ui.StopService, "Stop service", false, sessionClient));
	if_fail_fe(llsControl(gui, ui.Benchmark, "Benchmark", false, inspectorClient));
	if_fail_fe(llsControl(gui, ui.Keyboard, "Keyboard", false, workspaceClient));
	if_fail_fe(llsControl(gui, ui.OpenPath, "Open path", false, workspaceClient));
	if_fail_fe(llsControl(gui, ui.ListPrev, "< List", false, workspaceClient));
	if_fail_fe(llsControl(gui, ui.ListNext, "List >", false, workspaceClient));
	if_fail_fe(llsControl(gui, ui.PagePrev, "< Page", false, inspectorClient));
	if_fail_fe(llsControl(gui, ui.PageNext, "Page >", false, inspectorClient));
	if_fail_fe(ui.RowText.resize(16));
	if_fail_fe(llsControlPool(gui, ui.Rows, 16, ui.Family, false, workspaceClient));
	if_fail_fe(llsControlPool(gui, ui.Lines, 40, {{}, {}}, true, inspectorClient));
	if_fail_fe(llc::inputBoxCreate(ui.Input, gui, workspaceClient));
	ui.Input.MaxLength = 4096;
	ui.Input.Edit(gui, true);
	for(auto & control : gui.Controls.Draw) {
		control.UseNewPalettes = true;
		for(llc::u2_t state = 0; state < llc::GUI_CONTROL_PALETTE_COUNT; ++state) control.Palettes[state] = (llc::s1_t)state;
	}
	if_fail_fe(helpPaths(ui.Paths, true));
	if_fail_fe(llsLayout(ui));
	if_fail_fe(llsSessionView(app));
	gui.Controls.SetHidden(ui.Viewports[LLS_GUI_VIEWPORT_WORKSPACE].IdControl, true);
	gui.Controls.SetHidden(ui.Viewports[LLS_GUI_VIEWPORT_INSPECTOR].IdControl, true);
	gui.Controls.SetHidden(ui.Hint, true);
	if_fail_fe(llsRootActivate(ui, ui.Processes));
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
	bool doubleClick = false;
	if_fail_fe(llsDoubleClick(window.EventQueue, doubleClick));
	if_fail_fe(llsViewportsUpdate(ui, doubleClick));
	if_fail_fe(llsToolbarUpdate(app));
	if_fail_fe(llsRootUpdate(ui, ui.Processes, {320, 240}, llsProcessLayout));
	if_fail_fe(llsRootUpdate(ui, ui.Services, {320, 240}, llsServiceLayout));
	if_fail_fe(llsExplorerUpdate(app, doubleClick));
	if_fail_fe(llsHelpUpdate(ui));
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
	bool serviceControlBusy = false;
	{
		llc::lock_guard guard(app.Lock);
		serviceControlBusy = app.ServiceCommandPending || app.ServiceCommandBusy;
	}
	cnst llc::cid_t serviceStop = llsChild(gui, llsChild(gui, ui.Services, LLS_VIEWPORT_Target), SERVICE_VIEWPORT_Stop);
	gui.Controls.SetDisabled(ui.StopService, serviceControlBusy);
	gui.Controls.SetDisabled(serviceStop, serviceControlBusy);
	if(gui.Controls.IsAction(ui.StopService) || gui.Controls.IsAction(serviceStop)) {
		cnst llc::err_t queued = llsServiceQueue(app, "shutdown");
		if(llc::failed(queued)) {
			llc::lock_guard guard(app.Lock);
			if_fail_fe(app.ServiceResponse.clear());
			if_fail_fe(::llc::append_string(app.ServiceResponse, "A service control operation is already running."));
			++app.Revision;
		}
		ui.Tab = 6;
		ui.Selected = (llc::u2_t)-1;
		ui.ListOffset = ui.LineOffset = 0;
		ui.Dirty = true;
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
			if_fail_fe(::llc::append_string(app.CommandResponse, "Invalid command JSON or command already running."));
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
		if((gui.Controls.IsAction(ui.Rows[i]) || (doubleClick && gui.Controls.IsHovered(ui.Rows[i]))) && i + ui.ListOffset < ui.Matches.size()) {
			if(ui.Tab == 4) {
				if_fail_fe(llsTextRelease(ui, ui.Breadcrumbs));
				if_fail_fe(llsExplorerSelect(ui.Explorer, i + ui.ListOffset));
				if(doubleClick && gui.Controls.IsHovered(ui.Rows[i]) && !ui.Explorer.SelectedFolder && ui.Explorer.SelectedPath.size()) {
					cnst llc::err_t queued = llsServiceQueue(app, "launch", ui.Explorer.SelectedPath);
					if(llc::failed(queued)) {
						llc::lock_guard guard(app.Lock);
						if_fail_fe(app.ServiceResponse.clear());
						if_fail_fe(::llc::append_string(app.ServiceResponse, "A service control operation is already running."));
						++app.Revision;
					}
				}
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
		? llc::vcst_t{"PATH (not search): click folders to expand; double-click lls.exe or lrt.exe to start LLS."}
		: llc::vcst_t{"Type to filter. Click paths in details to browse. Open path uses the text box. Mouse wheel scrolls."}));
	ui.Refresh += ui.Framework.FrameInfo.Seconds.LastFrame;
	if(ui.Dirty || ui.Refresh >= .2) { if_fail_fe(llsSessionView(app)); ui.Refresh = 0; }
	cnst llc::cid_t workspaceClient = ui.Viewports[LLS_GUI_VIEWPORT_WORKSPACE].IdControls[llc::VIEWPORT_CONTROL_TARGET];
	if_fail_fe(llsControlPool(gui, ui.Breadcrumbs, ui.Explorer.Crumbs.size(), ui.Family, false, workspaceClient));
	cnst llc::s1_t workspaceWidth = gui.Controls.Placement[workspaceClient].Area.Size.x;
	llc::s1_t crumbX = 10;
	for(llc::u2_t i = 0; i < ui.Breadcrumbs.size(); ++i) {
		cnst bool exists = ui.Tab == 4 && i < ui.Explorer.Crumbs.size();
		gui.Controls.SetHidden(ui.Breadcrumbs[i], !exists);
		if(!exists) continue;
		llc::vcst_t path = ui.Explorer.Crumbs[i], name = path;
		while(name.size() > 3 && name[name.size() - 1] == '/') name = {name.begin(), name.size() - 1};
		if(i) { if_fail_fe(llc::pathFilename(name, name)); }
		cnst llc::s1_t width = (llc::s1_t)llc::min(220U, 20U + name.size() * 8);
		gui.Controls.SetHidden(ui.Breadcrumbs[i], crumbX + width > workspaceWidth - 10);
		if_fail_fe(llc::controlTextSet(gui, ui.Breadcrumbs[i], name));
		if_fail_fe(llsPlace(gui, ui.Breadcrumbs[i], crumbX, 174, width, 26));
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
	{
		llc::lock_guard guard(app.Lock);
		if_fail_fe(llsFeatureTextAttach(app));
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
		if_fail_fe(llsFeatureTextDetach(ui));
	}
	llc::sleep(window.NoDraw ? 50 : 8);
	rtrn 0;
}
