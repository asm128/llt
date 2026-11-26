#include <cfloat>
#include "llc_server.h"

#include "llc_apod_serialize.h"
#include "llc_app_impl.h"
#include "llc_args.h"
#include "llc_encoding.h"
#include "llc_file.h"
#include "llc_cstring.h"
#include "llc_framework.h"
#include "llc_gui_control_list.h"
#include "llc_json_expression.h"
#include "llc_runtime.h"
#include "llc_system_key.h"
#include "llc_string_compose.h"
#include "llc_process_broker.h"
#include "llc_chrono.h"
#include "llc_deflate.h"
#include "llc_parse.h"
#include <climits>
#include <algorithm>
#include <filesystem>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#if defined(_WIN32)
#include <Windows.h>
#include <TlHelp32.h>
#include <winsvc.h>
#endif

stxp ::llc::u2_t DESKTOP_VISIBLE_ROWS = 20;
stxp ::llc::vcst_c SCREENSHOT_PATH_DEFAULT = "d:/enterprise/logs/screenshots";
stxp ::llc::u2_t DESKTOP_FILE_MAX_BYTES = 1024 * 1024;
stxp ::llc::u2_t SEARCH_INDEX_MAGIC = 0x3249424C;
stxp ::llc::u2_t SEARCH_SHARD_MAGIC = 0x3349424C;
stxp ::llc::u2_t SEARCH_PARTIAL_MAGIC = 0x3449424C;
stxp ::llc::u2_t SEARCH_CHARS_MAGIC = 0x3549424C;
stxp ::llc::u2_t SEARCH_DEFLATE_MAGIC = 0x3649424C;
stxp ::llc::u2_t SEARCH_GRAM_MAGIC = 0x3A49424C;
stxp ::llc::u2_t SEARCH_INDEX_MAX_BYTES = 512U * 1024U * 1024U;
stxp ::llc::u2_t SEARCH_SHARD_MAX_WORDS = 200000;
stxp ::llc::u2_t SEARCH_SHARD_MAX_FILES = 1024;

stct SGramRange { ::llc::u2_t Gram = 0, Start = 0, Count = 0; };
stct SGramSection { ::llc::u2_t Offset = 0, Count = 0; };
stct SGramDiskHeader {
	::llc::u2_t Magic = SEARCH_GRAM_MAGIC, Shard = 0, Paths = 0;
	::llc::u3_t Generation = 0;
	SGramSection Ranges[3] = {}, Files[3] = {};
	SGramSection Buckets[3][256] = {};
};
stct SGramRoute {
	::llc::u2_t Paths = 0;
	::llc::apod<SGramRange> Two = {}, Three = {}, Four = {};
	::llc::au1_t FilesTwo = {}, FilesThree = {}, FilesFour = {};
};

stxp ::llc::u0_t SAMPLE_GUI_THEME_COUNT = 3;
stxp ::llc::u0_t PALETTE_SHADE_COUNT = 16;

sttc ::llc::SColorBGRA sampleGUIThemeColor(::llc::u0_t theme) nxpt {
	stxp ::llc::SColorBGRA colors[SAMPLE_GUI_THEME_COUNT] =
		{ {248, 208,  72}
		, { 32, 176, 248}
		, {232, 232, 232}
		};
	rtrn colors[theme % SAMPLE_GUI_THEME_COUNT];
}

sttc ::llc::SColorBGRA sampleGUIShade(::llc::SColorBGRA color, ::llc::u0_t shade) nxpt {
	shade = ::llc::min(shade, (::llc::u0_t)(PALETTE_SHADE_COUNT - 1));
	rtrn color * ((PALETTE_SHADE_COUNT - (::llc::f2_t)shade) / PALETTE_SHADE_COUNT);
}

sttc ::llc::SColorBGRA sampleGUIDesaturate(::llc::SColorBGRA color) nxpt {
	cnst ::llc::u0_t gray = (::llc::u0_t)(((::llc::u2_t)color.r + color.g + color.b) / 3);
	rtrn
		{ (::llc::u0_t)((color.b + (::llc::u1_t)gray * 2) / 3)
		, (::llc::u0_t)((color.g + (::llc::u1_t)gray * 2) / 3)
		, (::llc::u0_t)((color.r + (::llc::u1_t)gray * 2) / 3)
		};
}

sttc ::llc::u0_t sampleGUIStateShade(::llc::u0_t state) nxpt {
	stxp ::llc::u0_t shades[::llc::GUI_CONTROL_PALETTE_COUNT] = {8, 10, 5, 11, 4, 7, 2, 7, 1, 13};
	rtrn shades[state % ::llc::GUI_CONTROL_PALETTE_COUNT];
}

sttc ::llc::u0_t sampleGUIStateBorderOffset(::llc::u0_t state) nxpt {
	stxp ::llc::u0_t offsets[::llc::GUI_CONTROL_PALETTE_COUNT] = {5, 1, 7, 3, 0, 1, 7, 3, 5, 5};
	rtrn offsets[state % ::llc::GUI_CONTROL_PALETTE_COUNT];
}

sttc ::llc::SColorBGRA sampleGUITextColor(::llc::SColorBGRA background) nxpt {
	cnst ::llc::u2_t luminance = (::llc::u2_t)background.r * 299 + (::llc::u2_t)background.g * 587 + (::llc::u2_t)background.b * 114;
	rtrn ::sampleGUIShade({255, 255, 255}, luminance > 150000 ? 15 : 1);
}

sttc ::llc::err_t sampleGUIResourcesCreate(::llc::SGUI & gui) {
	if_true_fe(0 == gui.Colors.create());
	if_fail_fe(gui.Colors->Palettes.resize(SAMPLE_GUI_THEME_COUNT * ::llc::GUI_CONTROL_PALETTE_COUNT));
	::llc::view<::llc::astatic<::llc::bgra, ::llc::UI_CONTROL_AREA_COUNT>> palettes = gui.Colors->Palettes;
	if_fail_fe(palettes.enumerate([&](::llc::u2_t iPalette, ::llc::astatic<::llc::bgra, ::llc::UI_CONTROL_AREA_COUNT> & palette) -> ::llc::err_t {
		cnst ::llc::u0_t iTheme = (::llc::u0_t)(iPalette / ::llc::GUI_CONTROL_PALETTE_COUNT);
		cnst ::llc::u0_t iState = (::llc::u0_t)(iPalette % ::llc::GUI_CONTROL_PALETTE_COUNT);
		::llc::SColorBGRA tone = ::sampleGUIThemeColor(iTheme);
		if(iState == ::llc::GUI_CONTROL_PALETTE_DISABLED || iState == ::llc::GUI_CONTROL_PALETTE_SELECTED_DISABLED)
			tone = ::sampleGUIDesaturate(tone);
		cnst ::llc::u0_t shade = ::sampleGUIStateShade(iState);
		cnst ::llc::SColorBGRA base = ::sampleGUIShade(tone, shade);
		cnst ::llc::u0_t borderOffset = ::sampleGUIStateBorderOffset(iState);
		cnst ::llc::u0_t borderShade = shade > 7
			? (::llc::u0_t)(shade > borderOffset ? shade - borderOffset : 0)
			: (::llc::u0_t)::llc::min((::llc::u2_t)shade + borderOffset, (::llc::u2_t)PALETTE_SHADE_COUNT - 1);
		cnst ::llc::SColorBGRA borderColor = ::sampleGUIShade(tone, borderShade);
		::llc::view<::llc::bgra> areas = palette;
		if_fail_fe(areas.fill(::sampleGUIShade(tone, (::llc::u0_t)::llc::min((::llc::u2_t)shade + 5, (::llc::u2_t)PALETTE_SHADE_COUNT - 1))));
		palette[::llc::UI_CONTROL_AREA_BACKGROUND] = ::sampleGUIShade(tone, (::llc::u0_t)::llc::min((::llc::u2_t)shade + 6, (::llc::u2_t)PALETTE_SHADE_COUNT - 1));
		palette[::llc::UI_CONTROL_AREA_CLIENT] = base;
		palette[::llc::UI_CONTROL_AREA_BORDER_LEFT] = borderColor;
		palette[::llc::UI_CONTROL_AREA_BORDER_TOP] = borderColor;
		palette[::llc::UI_CONTROL_AREA_BORDER_RIGHT] = borderColor;
		palette[::llc::UI_CONTROL_AREA_BORDER_BOTTOM] = borderColor;
		palette[::llc::UI_CONTROL_AREA_TEXT_BACKGROUND] = ::sampleGUIShade(tone, (::llc::u0_t)::llc::min((::llc::u2_t)shade + 2, (::llc::u2_t)PALETTE_SHADE_COUNT - 1));
		palette[::llc::UI_CONTROL_AREA_TEXT_FACE] = ::sampleGUITextColor(base);
		rtrn 0;
	}));
	::llc::SRasterFontManager fonts = {};
	if_fail_fe(::llc::rasterFontDefaults(fonts));
	gui.Fonts = fonts.Fonts;
	rtrn 0;
}

sttc void sampleGUIControlThemeApply(::llc::SGUI & gui, ::llc::cid_t idControl, ::llc::u0_t theme) nxpt {
	if(::llc::controlInvalid(gui, idControl))
		rtrn;
	::llc::SControlDraw & draw = gui.Controls.Draw[idControl];
	draw.UseNewPalettes = true;
	::llc::view<::llc::s1_t> palettes = draw.Palettes;
	(void)palettes.enumerate([theme](::llc::u2_t iState, ::llc::s1_t & palette) -> ::llc::err_t {
		palette = (::llc::s1_t)((theme % SAMPLE_GUI_THEME_COUNT) * ::llc::GUI_CONTROL_PALETTE_COUNT + iState);
		rtrn 0;
	});
}

sttc ::llc::err_t sampleGUIViewportDoubleClick(cnst ::llc::TQueueSystemEvent & events, bool & doubleClick) {
	doubleClick = false;
	if_fail_fe(events.for_each([&doubleClick](cnst ::llc::pobj<::llc::SEventSystem> & eventSystem) -> ::llc::err_t {
		if(0 == eventSystem.get_ref() || eventSystem->Type != ::llc::SYSTEM_EVENT_Mouse)
			rtrn 0;
		::llc::SEView<::llc::EVENT_MOUSE> eventMouse = {};
		if_fail_fe(eventSystem->ExtractChild(eventMouse));
		if(eventMouse.Type != ::llc::EVENT_MOUSE_DoubleClick)
			rtrn 0;
		::llc::vcu0_t input = eventMouse.Data;
		::llc::u0_t button = 0;
		if_fail_fe(::llc::loadPOD(input, button));
		if_true_fef(input.size(), "Unexpected mouse double-click payload tail:%u.", input.size());
		if(0 == button)
			doubleClick = true;
		rtrn 0;
	}));
	rtrn 0;
}

sttc bool sampleGUIViewportPressed(cnst SSampleGUIViewport & app, cnst ::llc::SViewport & viewport) nxpt {
	rtrn app.GUI.Controls.IsPressed(viewport.IdControl)
		|| app.GUI.Controls.IsPressed(viewport.IdControls[::llc::VIEWPORT_CONTROL_TITLE])
		|| app.GUI.Controls.IsPressed(viewport.IdControls[::llc::VIEWPORT_CONTROL_TARGET])
		;
}

sttc ::llc::err_t sampleGUIViewportActivate(SSampleGUIViewport & app, ::llc::u0_t iViewport) {
	if_true_fe(iViewport >= app.Viewports.size());
	cnst ::llc::cid_t idRoot = app.Viewports[iViewport].IdControl;
	::llc::acid & desktopChildren = app.GUI.Controls.Children[app.Desktop];
	if(0 == desktopChildren.size() || desktopChildren[desktopChildren.size() - 1] != idRoot) {
		cnst ::llc::err_t iChild = desktopChildren.find(idRoot);
		if(iChild >= 0)
			if_fail_fe(desktopChildren.remove((::llc::u2_t)iChild));
		if_fail_fe(desktopChildren.push_back(idRoot));
	}
	::llc::view<::llc::SViewport> viewports = app.Viewports;
	if_fail_fe(viewports.enumerate([&app, iViewport](::llc::u2_t iOther, ::llc::SViewport & viewport) -> ::llc::err_t {
		cnst bool active = iOther == iViewport;
		app.GUI.Controls.SetSelected(viewport.IdControl, active);
		app.GUI.Controls.SetSelected(viewport.IdControls[::llc::VIEWPORT_CONTROL_TITLE], active);
		rtrn 0;
	}));
	rtrn 0;
}

sttc ::llc::err_t sampleGUIViewportCreate(SSampleGUIViewport & app, ::llc::u0_t iViewport, ::llc::vcst_t title, cnst ::llc::rect2s1_t & area) {
	if_true_fe(iViewport >= app.Viewports.size());
	::llc::SViewport & viewport = app.Viewports[iViewport];
	if_fail_fe(::llc::guiCreateViewport(app.GUI, viewport, app.Desktop, app.ViewportActions[iViewport], 1 == iViewport, 1 == iViewport, 1 == iViewport));
	::llc::SControlPlacement & root = app.GUI.Controls.Placement[viewport.IdControl];
	root.Area = area;
	root.Border = {3, 3, 3, 3};
	root.Margin = {1, 1, 1, 1};
	cnst ::llc::cid_t idTitle = viewport.IdControls[::llc::VIEWPORT_CONTROL_TITLE];
	cnst ::llc::cid_t idClient = viewport.IdControls[::llc::VIEWPORT_CONTROL_TARGET];
	app.GUI.Controls.Text[idTitle].Text = title;
	app.GUI.Controls.Text[idTitle].FontSelected = 7;
	app.GUI.Controls.Placement[idTitle].Border = {2, 2, 2, 2};
	app.GUI.Controls.Placement[idClient].Area.Size = {(::llc::s1_t)(area.Size.x - 8), (::llc::s1_t)(area.Size.y - 32)};
	app.GUI.Controls.Placement[idClient].Border = {1, 1, 1, 1};
	::sampleGUIControlThemeApply(app.GUI, viewport.IdControl, iViewport);
	::llc::view<::llc::cid_t> viewportControls = viewport.IdControls;
	if_fail_fe(viewportControls.for_each([&app, iViewport](cnst ::llc::cid_t & id) -> ::llc::err_t { ::sampleGUIControlThemeApply(app.GUI, id, iViewport); rtrn 0; }));
	::llc::view<::llc::cid_t> actions = app.ViewportActions[iViewport];
	if_fail_fe(actions.for_each([&app, iViewport](cnst ::llc::cid_t & id) -> ::llc::err_t { ::sampleGUIControlThemeApply(app.GUI, id, iViewport); rtrn 0; }));
	app.UnfoldedSize[iViewport] = area.Size;
	rtrn ::llc::controlMetricsInvalidate(app.GUI, viewport.IdControl);
}

sttc ::llc::err_t sampleGUIViewportFold(SSampleGUIViewport & app, ::llc::u0_t iViewport) {
	if_true_fe(iViewport >= app.Viewports.size());
	::llc::SViewport & viewport = app.Viewports[iViewport];
	::llc::SControlPlacement & root = app.GUI.Controls.Placement[viewport.IdControl];
	cnst ::llc::cid_t idClient = viewport.IdControls[::llc::VIEWPORT_CONTROL_TARGET];
	if(app.Folded[iViewport]) {
		root.Area.Size = app.UnfoldedSize[iViewport];
		app.GUI.Controls.SetHidden(idClient, false);
	}
	else {
		app.UnfoldedSize[iViewport] = root.Area.Size;
		root.Area.Size.y = 30;
		app.GUI.Controls.SetHidden(idClient, true);
	}
	app.Folded[iViewport] = false == app.Folded[iViewport];
	rtrn ::llc::controlMetricsInvalidate(app.GUI, viewport.IdControl);
}

sttc ::llc::err_t sampleGUIViewportPerformanceUpdate(SSampleGUIViewport & app, double secondsLastFrame) {
	if(secondsLastFrame <= 0)
		rtrn 0;
	cnst ::llc::err_t textLength = ::llc::sprintf_s(app.PerformanceText.Storage, "FPS %.2f    Frame %.2f ms", 1.0 / secondsLastFrame, secondsLastFrame * 1000.0);
	if_fail_fe(textLength);
	app.GUI.Controls.Text[app.Performance].Text = {app.PerformanceText.Storage, (::llc::u2_t)textLength};
	app.GUI.Controls.SetUpdated(app.Performance, false);
	rtrn 0;
}

sttc ::llc::err_t desktopListCreate(SSampleGUIViewport & app, ::llc::u0_t iViewport, ::llc::acid & controls) {
	::llc::vcst_t labels[DESKTOP_VISIBLE_ROWS] = {};
	::llc::SControlPlacement row = {};
	row.Area.Size = {0, 21};
	row.Border = {1, 1, 1, 1};
	row.Margin = {3, 1, 3, 1};
	cnst ::llc::cid_t parent = app.Viewports[iViewport].IdControls[::llc::VIEWPORT_CONTROL_TARGET];
	if_fail_fe(::llc::guiCreateControlList(app.GUI, ::llc::DIRECTION_DOWN, {labels, DESKTOP_VISIBLE_ROWS}, parent, row, ::llc::ALIGN_CENTER_LEFT, controls));
	rtrn controls.for_each([&app, iViewport](cnst ::llc::cid_t & id) -> ::llc::err_t {
		app.GUI.Controls.Text[id].FontSelected = 7;
		::sampleGUIControlThemeApply(app.GUI, id, iViewport);
		rtrn 0;
	});
}

sttc ::llc::err_t indexResolveTerms(::llc::view<cnst ::llc::SWordIndices> words, ::llc::vcst_t query, ::llc::u2_t itemCount, ::llc::au2_t & results) {
	if_fail_fe(results.clear());
	::llc::aobj<::llc::vcst_t> terms = {};
	if_fail_fef(::llc::split(query, ' ', terms), "Query:%.*s.", LLC_VIEW_ARGS(query));
	::llc::au0_t present = {};
	bool first = true;
	rtrn terms.for_each([&](cnst ::llc::vcst_t & term) -> ::llc::err_t {
		if(!term.size()) rtrn 0;
		::llc::au2_t candidates = {}, retained = {};
		if_fail_fef(::llc::wordDictResolve(words, term, itemCount, candidates, {}), "Term:%.*s.", LLC_VIEW_ARGS(term));
		if(first) { results = ::std::move(candidates); first = false; rtrn 0; }
		if_fail_fef(present.resize(itemCount), "Term:%.*s. Items:%u.", LLC_VIEW_ARGS(term), itemCount);
		if_fail_fe(present.fill(0));
		cnst ::llc::err_t candidatesMarked = candidates.for_each([&](cnst ::llc::u2_t & index) -> ::llc::err_t {
			if_true_fef(index >= itemCount, "Term:%.*s. Candidate:%u. Items:%u.", LLC_VIEW_ARGS(term), index, itemCount);
			present[index] = 1;
			rtrn 0;
		});
		if_fail_fef(candidatesMarked, "Term:%.*s.", LLC_VIEW_ARGS(term));
		cnst ::llc::err_t resultsFiltered = results.for_each([&](cnst ::llc::u2_t & index) -> ::llc::err_t {
			if_true_fef(index >= itemCount, "Term:%.*s. Result:%u. Items:%u.", LLC_VIEW_ARGS(term), index, itemCount);
			if(present[index])
				if_fail_fe(retained.push_back(index));
			rtrn 0;
		});
		if_fail_fef(resultsFiltered, "Term:%.*s.", LLC_VIEW_ARGS(term));
		results = ::std::move(retained);
		rtrn 0;
	});
}

sttc ::llc::err_t inventoryRowsRefresh(SSampleGUIViewport & app, ::llc::acid & controls, cnst ::llc::aobj<::llc::string> & names, ::llc::view<cnst ::llc::SWordIndices> words, ::llc::u2_t offset) {
	if_fail_fe(controls.for_each([&app](cnst ::llc::cid_t & id) -> ::llc::err_t { app.GUI.Controls.SetHidden(id, true); rtrn 0; }));
	::llc::au2_t matches = {};
	if(app.SearchInput.Text.size())
		if_fail_fe(::indexResolveTerms(words, app.SearchInput.Text, names.size(), matches));
	::llc::u2_t seen = 0, written = 0, iName = 0;
	rtrn names.for_each([&](cnst ::llc::string & name) -> ::llc::err_t {
		cnst ::llc::u2_t index = iName++;
		if(app.SearchInput.Text.size() && ::llc::find(index, ::llc::vcu2_t{matches.begin(), matches.size()}) < 0) rtrn 0;
		if(seen++ < offset || written >= controls.size()) rtrn 0;
		cnst ::llc::cid_t id = controls[written++];
		::llc::vcst_t label = name;
		if_fail_fe(label.slice(label, 0, ::llc::min(label.size(), ::llc::guiViewportTextColumns(app.GUI, id))));
		app.GUI.Controls.Text[id].Text = label;
		app.GUI.Controls.SetHidden(id, false);
		rtrn 0;
	});
}

sttc ::llc::err_t inventoryListCreate(SSampleGUIViewport & app, ::llc::u0_t iViewport, ::llc::acid & controls) {
	::llc::vcst_t labels[10] = {};
	::llc::SControlPlacement row = {};
	row.Area.Size = {0, 21};
	row.Border = {1, 1, 1, 1};
	row.Margin = {3, 1, 3, 1};
	cnst ::llc::cid_t parent = app.Viewports[iViewport].IdControls[::llc::VIEWPORT_CONTROL_TARGET];
	if_fail_fe(::llc::guiCreateControlList(app.GUI, ::llc::DIRECTION_DOWN, {labels, ::llc::size(labels)}, parent, row, ::llc::ALIGN_CENTER_LEFT, controls));
	rtrn controls.for_each([&app, iViewport](cnst ::llc::cid_t & id) -> ::llc::err_t {
		app.GUI.Controls.Text[id].FontSelected = 7;
		::sampleGUIControlThemeApply(app.GUI, id, iViewport);
		rtrn 0;
	});
}

sttc ::llc::err_t previewRowsRefresh(SSampleGUIViewport & app) {
	::llc::u2_t i = 0;
	rtrn app.PreviewControls.for_each([&app, &i](cnst ::llc::cid_t & id) -> ::llc::err_t {
		cnst bool visible = i < app.PreviewLines.size();
		app.GUI.Controls.SetHidden(id, !visible);
		if(visible) {
			::llc::vcst_t text = app.PreviewLines[i];
			if_fail_fe(text.slice(text, 0, ::llc::min(text.size(), ::llc::guiViewportTextColumns(app.GUI, id))));
			app.GUI.Controls.Text[id].Text = text;
			if_fail_fe(app.GUI.Controls.TextState[id].resize(text.size(), ::llc::STextState{}));
		}
		++i;
		rtrn 0;
	});
}

sttc ::llc::err_t previewFile(SSampleGUIViewport & app, ::llc::vcst_t path) {
	if_fail_fe(app.PreviewControls.for_each([&app](cnst ::llc::cid_t & id) -> ::llc::err_t { rtrn ::llc::controlTextSet(app.GUI, id, {}); }));
	if_fail_fe(app.PreviewLines.clear());
	if_fail_fe(app.PreviewBytes.clear());
	cnst ::llc::err_t loaded = ::llc::fileToMemory(path, app.PreviewBytes, 8192);
	if(::llc::failed(loaded))
		if_fail_fef(app.PreviewLines.push_back("Preview unavailable"), "Path:%.*s.", LLC_VIEW_ARGS(path));
	if(!::llc::failed(loaded) && app.PreviewBytes.find('\0') >= 0)
		if_fail_fef(app.PreviewLines.push_back("Binary file"), "Path:%.*s.", LLC_VIEW_ARGS(path));
	if(!::llc::failed(loaded) && app.PreviewBytes.find('\0') < 0)
		if_fail_fef(app.PreviewBytes.for_each([](char & ch) -> ::llc::err_t { if(ch == '\t') ch = ' '; rtrn 0; }), "Path:%.*s.", LLC_VIEW_ARGS(path));
	if(!::llc::failed(loaded) && app.PreviewBytes.find('\0') < 0)
		if_fail_fef(::llc::split(::llc::vcst_t{app.PreviewBytes.begin(), app.PreviewBytes.size()}, '\n', app.PreviewLines), "Path:%.*s.", LLC_VIEW_ARGS(path));
	app.GUI.Controls.Text[app.Viewports[2].IdControls[::llc::VIEWPORT_CONTROL_TITLE]].Text = "FILE PREVIEW";
	rtrn ::previewRowsRefresh(app);
}

sttc ::llc::err_t consoleRowsRefresh(SSampleGUIViewport & app) {
	cnst ::llc::u2_t first = app.ConsoleLines.size() > app.ConsoleControls.size() ? app.ConsoleLines.size() - app.ConsoleControls.size() : 0;
	::llc::view<::llc::cid_t> controls = app.ConsoleControls;
	rtrn controls.enumerate([&app, first](::llc::u2_t i, ::llc::cid_t & id) -> ::llc::err_t {
		cnst bool visible = first + i < app.ConsoleLines.size();
		app.GUI.Controls.SetHidden(id, !visible);
		if(visible) {
			::llc::vcst_t text = app.ConsoleLines[first + i];
			cnst ::llc::u2_t columns = ::llc::guiViewportTextColumns(app.GUI, id);
			if_fail_fe(text.slice(text, 0, ::llc::min(text.size(), columns)));
			app.GUI.Controls.Text[id].Text = text;
			if_fail_fe(app.GUI.Controls.TextState[id].resize(text.size(), ::llc::STextState{}));
		}
		rtrn 0;
	});
}

sttc ::llc::err_t consoleWrite(SSampleGUIViewport & app, ::llc::vcst_t message) {
	::llc::string & line = app.ConsoleLines[app.ConsoleLines.push_back({})];
	if_fail_fe(::llc::append_strings(line, message));
	if(app.ConsoleLines.size() > 512)
		if_fail_fe(app.ConsoleLines.remove(0));
	rtrn ::consoleRowsRefresh(app);
}

sttc ::llc::err_t consoleOutputAppend(SSampleGUIViewport & app, ::llc::vcu0_t output) {
	::llc::vcst_t remaining = {(cnst char*)output.begin(), ::llc::min(output.size(), (::llc::u2_t)(64 * 1024))};
	while(remaining.size()) {
		::llc::vcst_t line = {}, next = {};
		cnst ::llc::err_t found = ::llc::split('\n', remaining, line, next);
		if(line.size() && line[line.size() - 1] == '\r')
			if_fail_fe(line.slice(line, 0, line.size() - 1));
		if_fail_fe(::consoleWrite(app, line));
		remaining = found < 0 ? ::llc::vcst_t{} : next;
	}
	rtrn 0;
}

sttc ::llc::err_t consoleBrokerRun(SSampleGUIViewport & app, ::llc::vcst_t command) {
	if(app.PendingConsole)
		rtrn ::consoleWrite(app, "Previous command is still running.");
	app.PendingConsole = ::std::make_shared<SConsoleWork>();
	::llc::SProcessBrokerJob & job = app.PendingConsole->Job;
#if defined(_WIN32)
	char systemDirectory[MAX_PATH] = {};
	cnst UINT directoryLength = ::GetSystemDirectoryA(systemDirectory, ::llc::size(systemDirectory));
	if_zero_fef(directoryLength && directoryLength < ::llc::size(systemDirectory), "GetSystemDirectoryA size:%u.", directoryLength);
	if_fail_fe(::llc::pathNameCompose(::llc::vcst_t{systemDirectory, directoryLength}, "cmd.exe", job.CommandLineApp));
	job.CommandLine = "cmd.exe /Q /D";
#else
	job.CommandLineApp = "/bin/sh";
	job.CommandLine = "sh -s";
#endif
	::llc::aobj<::llc::vcst_t> environmentViews = {};
	if_fail_fe(::llc::viewsFromEnvp(environmentViews, environ));
	::llc::as0_t environment = {};
	if_fail_fe(::llc::environmentBlock(environment, environmentViews));
	if_fail_fe(job.Request.append(::llc::vcu0_t{(cnst ::llc::u0_t*)environment.begin(), environment.size()}));
#if defined(_WIN32)
	::llc::string directoryCommand = {};
	if_fail_fef(::llc::append_strings(directoryCommand, "cd /D \"", app.FolderPath, "\"\r\n"), "Console folder:%.*s.", LLC_VIEW_ARGS(app.FolderPath));
	if_fail_fef(job.Request.append(::llc::vcu0_t{(cnst ::llc::u0_t*)directoryCommand.begin(), directoryCommand.size()}), "Console folder:%.*s.", LLC_VIEW_ARGS(app.FolderPath));
#endif
	if_fail_fe(job.Request.append(::llc::vcu0_t{(cnst ::llc::u0_t*)command.begin(), command.size()}));
#if defined(_WIN32)
	stxp char tail[] = "\r\nexit\r\n";
#else
	stxp char tail[] = "\nexit\n";
#endif
	if_fail_fe(job.Request.append(::llc::vcu0_t{(cnst ::llc::u0_t*)tail, ::llc::size(tail) - 1}));
	::std::shared_ptr<SConsoleWork> work = app.PendingConsole;
	::std::thread([work] {
		cnst ::llc::err_t result = ::llc::processBrokerRun(work->Job);
		work->Status.store(::llc::failed(result) ? -1 : 1, ::std::memory_order_release);
	}).detach();
	::llc::string prompt = {};
	if_fail_fe(::llc::append_strings(prompt, app.FolderPath, "> ", command));
	if_fail_fe(::consoleWrite(app, prompt));
	rtrn ::consoleWrite(app, "Command running...");
}

template<typename TCallback> sttc ::llc::err_t processList(TCallback && callback) {
#if defined(_WIN32)
	cnst HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if_true_fef(snapshot == INVALID_HANDLE_VALUE, "CreateToolhelp32Snapshot error:%u.", ::GetLastError());
	PROCESSENTRY32 entry = {};
	entry.dwSize = sizeof(entry);
	::llc::err_t result = 0;
	if(::Process32First(snapshot, &entry))
		do {
			result = callback(::llc::vcst_t{entry.szExeFile, (::llc::u2_t)-1}, entry.th32ProcessID);
		} while(!::llc::failed(result) && ::Process32Next(snapshot, &entry));
	::CloseHandle(snapshot);
	if_fail_fef(result, "Process snapshot callback error:%i.", result);
	rtrn 0;
#elif defined(__linux__)
	::llc::SPathTree entries = {};
	if_fail_fef(::llc::pathList(entries, "/proc", [](bool isFolder, ::llc::vcst_c &) -> ::llc::err_t { rtrn isFolder ? 2 : 0; }), "%s", "Process directory:/proc.");
	::llc::u2_t iEntry = 0;
	rtrn entries.Name.for_each([&](cnst ::llc::vcst_t & name) -> ::llc::err_t {
		cnst bool isFolder = entries.Info[iEntry++].Folder;
		if(!isFolder) rtrn 0;
		::llc::u2_t pid = 0;
		cnst ::llc::err_t digits = ::llc::parseIntegerDecimal(name, pid);
		if(digits != (::llc::err_t)name.size()) rtrn 0;
		::llc::string folder = {}, commPath = {}, comm = {};
		if_fail_fef(::llc::pathNameCompose("/proc", name, folder), "PID:%u.", pid);
		if_fail_fef(::llc::pathNameCompose(folder, "comm", commPath), "PID:%u.", pid);
		if(::llc::failed(::llc::fileToMemory(commPath, comm, 256))) rtrn 0;
		::llc::vcst_t processName = comm;
		::llc::trim(processName, processName);
		if(!processName.size()) rtrn 0;
		rtrn callback(processName, pid);
	});
#else
	(void)callback;
	if_true_fef(true, "%s", "Process listing unsupported on this platform.");
	rtrn 0;
#endif
}

template<typename TCallback> sttc ::llc::err_t serviceList(TCallback && callback) {
#if defined(_WIN32)
	cnst SC_HANDLE manager = ::OpenSCManagerA(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
	if_null_fef(manager, "OpenSCManager error:%u.", ::GetLastError());
	DWORD needed = 0, count = 0, resume = 0;
	::EnumServicesStatusExA(manager, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, nullptr, 0, &needed, &count, &resume, nullptr);
	::llc::au0_t buffer = {};
	cnst ::llc::err_t resized = buffer.resize(needed);
	if(::llc::failed(resized)) { ::CloseServiceHandle(manager); rtrn resized; }
	::llc::err_t result = 0;
	if(needed && ::EnumServicesStatusExA(manager, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, buffer.begin(), needed, &needed, &count, &resume, nullptr)) {
		cnst ENUM_SERVICE_STATUS_PROCESSA * entries = reinterpret_cast<cnst ENUM_SERVICE_STATUS_PROCESSA *>(buffer.begin());
		result = ::llc::view<cnst ENUM_SERVICE_STATUS_PROCESSA>{entries, count}.for_each([&callback](cnst ENUM_SERVICE_STATUS_PROCESSA & entry) -> ::llc::err_t {
			cnst DWORD state = entry.ServiceStatusProcess.dwCurrentState;
			cnst ::llc::vcst_t label = state == SERVICE_RUNNING ? "RUN  " : state == SERVICE_STOPPED ? "STOP " : state == SERVICE_PAUSED ? "PAUSE" : "PEND ";
			rtrn callback(::llc::vcst_t{entry.lpDisplayName, (::llc::u2_t)-1}, label);
		});
	}
	else {
		cnst DWORD error = ::GetLastError();
		::CloseServiceHandle(manager);
		if_true_fef(true, "EnumServicesStatusEx error:%u.", error);
	}
	::CloseServiceHandle(manager);
	if_fail_fef(result, "Service snapshot callback error:%i.", result);
	rtrn 0;
#elif defined(__linux__)
	::llc::SProcessBrokerJob job = {};
	job.CommandLineApp = "/usr/bin/systemctl";
	job.CommandLine = "systemctl list-units --type=service --all --no-legend --plain --no-pager";
	if_fail_fe(job.Request.push_back(0));
	if_fail_fe(job.Request.push_back(0));
	cnst ::llc::err_t listed = ::llc::processBrokerRun(job);
	if(::llc::failed(listed) || job.ExitCode) rtrn 0;
	::llc::aobj<::llc::vcst_t> lines = {};
	if_fail_fe(::llc::split(::llc::vcst_t{(cnst char*)job.Response.begin(), job.Response.size()}, '\n', lines));
	rtrn lines.for_each([&](cnst ::llc::vcst_t & line) -> ::llc::err_t {
		::llc::aobj<::llc::vcst_t> fields = {};
		if_fail_fe(::llc::split(line, ' ', fields));
		::llc::vcst_t unit = {}, state = {};
		::llc::u2_t iField = 0;
		if_fail_fe(fields.for_each([&](cnst ::llc::vcst_t & field) -> ::llc::err_t {
			if(!field.size()) rtrn 0;
			if(iField == 0) unit = field;
			if(iField == 2) state = field;
			++iField;
			rtrn 0;
		}));
		if(!unit.size() || !state.size()) rtrn 0;
		rtrn callback(unit, state == "active" ? ::llc::vcst_t{"RUN  "} : state == "inactive" ? ::llc::vcst_t{"STOP "} : ::llc::vcst_t{"PEND "});
	});
#else
	(void)callback;
	if_true_fef(true, "%s", "Service listing unsupported on this platform.");
	rtrn 0;
#endif
}

sttc ::llc::err_t processSnapshot(::llc::aobj<::llc::string> & names) {
	rtrn ::processList([&names](::llc::vcst_t processName, ::llc::u2_t pid) -> ::llc::err_t {
		::llc::string & name = names[names.push_back({})];
		::llc::astsc_t<32> number = {};
		if_fail_fef(::llc::sprintf_s(number.Storage, "%u", pid), "Process:%.*s. PID:%u.", LLC_VIEW_ARGS(processName), pid);
		rtrn ::llc::append_strings(name, processName, " #", number.Storage);
	});
}

sttc ::llc::err_t serviceSnapshot(::llc::aobj<::llc::string> & names) {
	rtrn ::serviceList([&names](::llc::vcst_t displayName, ::llc::vcst_t state) -> ::llc::err_t {
		::llc::string & name = names[names.push_back({})];
		rtrn ::llc::append_strings(name, state, displayName);
	});
}

sttc ::llc::err_t inventoryIndexBuild(::llc::aobj<::llc::SWordIndices> & words, cnst ::llc::aobj<::llc::string> & names) {
	if_fail_fe(words.clear());
	rtrn ::llc::getWordDict(words, names.size(), [&names](::llc::u2_t iSource, ::llc::u2_t & iItem, ::llc::aobj<::llc::vcst_t> & views) -> ::llc::err_t {
		iItem = iSource;
		rtrn views.push_back(names[iSource]);
	}, {});
}

sttc ::llc::err_t inventorySnapshotRefresh(SSampleGUIViewport & app) {
	if_fail_fe(app.ProcessControls.for_each([&app](cnst ::llc::cid_t & id) -> ::llc::err_t { rtrn ::llc::controlTextSet(app.GUI, id, {}); }));
	if_fail_fe(app.ServiceControls.for_each([&app](cnst ::llc::cid_t & id) -> ::llc::err_t { rtrn ::llc::controlTextSet(app.GUI, id, {}); }));
	if_fail_fe(app.ProcessNames.clear());
	if_fail_fe(app.ServiceNames.clear());
	if_fail_fe(::processSnapshot(app.ProcessNames));
	if_fail_fe(::serviceSnapshot(app.ServiceNames));
	if_fail_fe(::inventoryIndexBuild(app.ProcessWords, app.ProcessNames));
	if_fail_fe(::inventoryIndexBuild(app.ServiceWords, app.ServiceNames));
	app.ProcessOffset = ::llc::min(app.ProcessOffset, app.ProcessNames.size() > app.ProcessControls.size() ? app.ProcessNames.size() - app.ProcessControls.size() : 0);
	app.ServiceOffset = ::llc::min(app.ServiceOffset, app.ServiceNames.size() > app.ServiceControls.size() ? app.ServiceNames.size() - app.ServiceControls.size() : 0);
	if_fail_fe(::inventoryRowsRefresh(app, app.ProcessControls, app.ProcessNames, app.ProcessWords, app.ProcessOffset));
	rtrn ::inventoryRowsRefresh(app, app.ServiceControls, app.ServiceNames, app.ServiceWords, app.ServiceOffset);
}

sttc ::llc::err_t desktopFolderRowsRefresh(SSampleGUIViewport & app) {
	::llc::view<::llc::cid_t> controls = app.FolderControls;
	rtrn controls.enumerate([&app](::llc::u2_t i, ::llc::cid_t & id) -> ::llc::err_t {
		cnst ::llc::u2_t entry = i ? app.FolderOffset + i - 1 : 0;
		cnst bool visible = 0 == i || entry < app.FolderEntries->Name.size();
		app.GUI.Controls.SetHidden(id, !visible);
		if(!visible)
			rtrn 0;
		::llc::vcst_t label = 0 == i ? ::llc::vcst_t{".."} : app.FolderEntries->Name[entry];
		if_fail_fe(label.slice(label, 0, ::llc::min(label.size(), ::llc::guiViewportTextColumns(app.GUI, id))));
		app.GUI.Controls.Text[id].Text = label;
		::sampleGUIControlThemeApply(app.GUI, id, 0 == i ? 2 : app.FolderEntries->Info[entry].Folder ? 1 : 0);
		app.GUI.Controls.SetSelected(id, i && entry == app.SelectedFileEntry);
		rtrn 0;
	});
}

sttc ::llc::err_t desktopFileRowsRefresh(SSampleGUIViewport & app) {
	::llc::view<::llc::cid_t> controls = app.FileControls;
	rtrn controls.enumerate([&app](::llc::u2_t i, ::llc::cid_t & id) -> ::llc::err_t {
		cnst ::llc::u2_t line = app.FileOffset + i;
		cnst bool visible = line < app.TextRows.size() || (0 == i && 0 == app.TextRows.size());
		app.GUI.Controls.SetHidden(id, !visible);
		if(visible) {
			::llc::vcst_t text = app.TextRows.size() ? app.TextRows[line].Text : ::llc::vcst_t{"Select a file"};
			if(!app.WordWrap && app.FileColumnOffset < text.size())
				if_fail_fe(text.slice(text, app.FileColumnOffset));
			if(!app.WordWrap && app.FileColumnOffset >= text.size())
				text = {};
			if_fail_fe(text.slice(text, 0, ::llc::min(text.size(), ::llc::guiViewportTextColumns(app.GUI, id))));
			app.GUI.Controls.Text[id].Text = text;
			::llc::apod<::llc::STextState> & states = app.GUI.Controls.TextState[id];
			if_fail_fe(states.resize(text.size(), ::llc::STextState{}));
		if_fail_fe(states.for_each([&app](::llc::STextState & state) -> ::llc::err_t {
			state.Flag = ::llc::setSelected(state.Flag, state.Mask, app.SelectAll);
			state.Flag = ::llc::setSwapColors(state.Flag, state.Mask, app.SelectAll);
			rtrn 0;
		}));
		}
		rtrn 0;
	});
}

sttc ::llc::err_t desktopTextRowsRebuild(SSampleGUIViewport & app) {
	app.VisibleColumns = ::llc::guiViewportTextColumns(app.GUI, app.FileControls[0]);
	app.FileLineWidthMax = 0;
	if_fail_fe(app.FileLines.for_each([&app](cnst ::llc::vcst_t & line) -> ::llc::err_t {
		app.FileLineWidthMax = ::llc::max(app.FileLineWidthMax, line.size());
		rtrn 0;
	}));
	if_fail_fe(::llc::guiViewportTextRowsBuild(app.FileLines, app.VisibleColumns, app.WordWrap, app.TextRows));
	app.FileOffset = ::llc::min(app.FileOffset, app.TextRows.size() > DESKTOP_VISIBLE_ROWS ? app.TextRows.size() - DESKTOP_VISIBLE_ROWS : 0);
	rtrn ::desktopFileRowsRefresh(app);
}

sttc ::llc::err_t desktopFolderOpen(SSampleGUIViewport & app, ::llc::vcst_t path) {
	::llc::pobj<::llc::SPathTree> entries = {};
	::llc::SPathTree * newEntries = entries.create();
	if_null_fef(newEntries, "Folder:%.*s.", LLC_VIEW_ARGS(path));
	if(::llc::failed(::llc::pathList(*newEntries, path, [] (bool folder, ::llc::vcst_c &) -> ::llc::err_t { rtrn folder ? 2 : 0; }))) {
		::llc::string message = {};
		if_fail_fef(::llc::append_strings(message, "Cannot open directory: ", path), "Folder:%.*s.", LLC_VIEW_ARGS(path));
		rtrn ::consoleWrite(app, message);
	}
	app.FolderPath = path;
	app.FolderEntries = entries;
	app.SelectedFileEntry = (::llc::u2_t)-1;
	if_fail_fe(app.SelectedFile.clear());
	if_fail_fe(app.PreviewControls.for_each([&app](cnst ::llc::cid_t & id) -> ::llc::err_t { rtrn ::llc::controlTextSet(app.GUI, id, {}); }));
	if_fail_fe(app.PreviewLines.clear());
	if_fail_fe(app.PreviewBytes.clear());
	if_fail_fe(::previewRowsRefresh(app));
	app.FolderOffset = 0;
	if_fail_fe(::desktopFolderRowsRefresh(app));
	::llc::vcst_t folderName = {};
	if_fail_fe(::llc::pathFilename(app.FolderPath, folderName));
	app.GUI.Controls.Text[app.Viewports[0].IdControls[::llc::VIEWPORT_CONTROL_TITLE]].Text = folderName.size() ? folderName : ::llc::vcst_t{"ROOT"};
	if_fail_fe(app.FileControls.for_each([&app](cnst ::llc::cid_t & id) -> ::llc::err_t { rtrn ::llc::controlTextSet(app.GUI, id, {}); }));
	if_fail_fe(app.TextRows.clear());
	if_fail_fe(app.FileLines.clear());
	if_fail_fe(app.FileBytes.clear());
	if_fail_fe(app.FileDisplay.clear());
	if_fail_fe(app.OpenFile.clear());
	app.FileSize = app.FileChunkOffset = 0;
	app.FileChunkBinary = false;
	app.FileOffset = 0;
	app.FileColumnOffset = 0;
	app.SelectAll = false;
	app.GUI.Controls.SetDisabled(app.ViewportActions[1][::llc::GUI_VIEWPORT_ACTION_SAVE], true);
	app.GUI.Controls.SetDisabled(app.ViewportActions[1][::llc::GUI_VIEWPORT_ACTION_SELECT_ALL], true);
	app.GUI.Controls.Text[app.Viewports[1].IdControls[::llc::VIEWPORT_CONTROL_TITLE]].Text = "FILE CONTENT";
	rtrn ::desktopFileRowsRefresh(app);
}

sttc ::llc::err_t desktopFileChunkLoad(SSampleGUIViewport & app, ::llc::u3_t chunkOffset, ::llc::u3_t cursorOffset) {
	if_fail_fe(app.FileControls.for_each([&app](cnst ::llc::cid_t & id) -> ::llc::err_t { rtrn ::llc::controlTextSet(app.GUI, id, {}); }));
	if_fail_fe(app.TextRows.clear());
	if_fail_fe(app.FileLines.clear());
	if_fail_fe(app.FileBytes.clear());
	if_fail_fe(app.FileDisplay.clear());
	app.FileChunkOffset = chunkOffset;
	app.FileChunkBinary = false;
	if(app.FileSize)
		if_fail_fef(::llc::fileToMemory(app.OpenFile, app.FileBytes, DESKTOP_FILE_MAX_BYTES, chunkOffset), "File:%.*s. Offset:%llu.", LLC_VIEW_ARGS(app.OpenFile), chunkOffset);
	::llc::vcst_t remaining = app.FileBytes;
	if(0 == app.FileSize)
		if_fail_fe(app.FileLines.push_back(::llc::vcst_t{"Empty file"}));
	if(remaining.find('\0') >= 0) {
		app.FileChunkBinary = true;
		if_fail_fe(::llc::hexEncode(::llc::vcu0_t{(cnst ::llc::u0_t*)remaining.begin(), ::llc::min(remaining.size(), (::llc::u2_t)4096)}, app.FileDisplay));
		remaining = app.FileDisplay;
		if_fail_fe(app.FileLines.push_back(::llc::vcst_t{"Binary file: hex preview"}));
	}
	if(chunkOffset && !app.FileChunkBinary) {
		cnst ::llc::err_t lineEnd = remaining.find('\n');
		if(lineEnd >= 0)
			if_fail_fe(remaining.slice(remaining, (::llc::u2_t)lineEnd + 1));
	}
	while(remaining.size()) {
		::llc::vcst_t line = {}, next = {};
		cnst ::llc::err_t found = ::llc::split('\n', remaining, line, next);
		if(line.size() && line[line.size() - 1] == '\r')
			if_fail_fe(line.slice(line, 0, line.size() - 1));
		if_fail_fe(app.FileLines.push_back(line));
		remaining = found < 0 ? ::llc::vcst_t{} : next;
	}
	app.FileOffset = 0;
	app.FileColumnOffset = 0;
	app.SelectAll = false;
	app.GUI.Controls.SetDisabled(app.ViewportActions[1][::llc::GUI_VIEWPORT_ACTION_SAVE], app.FileSize != app.FileBytes.size());
	app.GUI.Controls.SetDisabled(app.ViewportActions[1][::llc::GUI_VIEWPORT_ACTION_SELECT_ALL], false);
	::llc::vcst_t fileName = {};
	if_fail_fe(::llc::pathFilename(app.OpenFile, fileName));
	cnst ::llc::err_t titleLength = ::llc::sprintf_s(app.FileTitle.Storage, "%.*s @ %llu / %llu", (int)::llc::min(fileName.size(), (::llc::u2_t)48), fileName.begin(), chunkOffset, app.FileSize);
	if_fail_fe(titleLength);
	app.GUI.Controls.Text[app.Viewports[1].IdControls[::llc::VIEWPORT_CONTROL_TITLE]].Text = {app.FileTitle.Storage, (::llc::u2_t)titleLength};
	if_fail_fe(::desktopTextRowsRebuild(app));
	if(cursorOffset > chunkOffset && !app.FileChunkBinary && app.TextRows.size()) {
		::llc::u2_t target = 0;
		::llc::view<::llc::SGUIViewportTextRow> rows = app.TextRows;
		if_fail_fe(rows.enumerate([&](::llc::u2_t i, ::llc::SGUIViewportTextRow & row) -> ::llc::err_t {
			cnst ::llc::u3_t position = chunkOffset + (row.Text.begin() - app.FileBytes.begin());
			if(position <= cursorOffset) target = i;
			rtrn 0;
		}));
		app.FileOffset = target > DESKTOP_VISIBLE_ROWS / 2 ? target - DESKTOP_VISIBLE_ROWS / 2 : 0;
	}
	rtrn ::desktopFileRowsRefresh(app);
}

sttc ::llc::err_t desktopFileOpen(SSampleGUIViewport & app, ::llc::vcst_t path) {
	app.OpenFile = path;
	cnst ::llc::s3_t fileLength = ::llc::fileSize(app.OpenFile);
	if_true_fef(fileLength < 0, "File:%.*s.", LLC_VIEW_ARGS(path));
	app.FileSize = (::llc::u3_t)fileLength;
	rtrn ::desktopFileChunkLoad(app, 0, 0);
}

sttc ::llc::err_t desktopFileChunkShift(SSampleGUIViewport & app, bool down) {
	if(!app.OpenFile.size() || app.FileSize <= DESKTOP_FILE_MAX_BYTES || !app.FileBytes.size()) rtrn 0;
	if(down && app.FileChunkOffset + app.FileBytes.size() >= app.FileSize) rtrn 0;
	if(!down && 0 == app.FileChunkOffset) rtrn 0;
	if(down && app.FileOffset + 2 * DESKTOP_VISIBLE_ROWS < app.TextRows.size()) rtrn 0;
	if(!down && app.FileOffset > DESKTOP_VISIBLE_ROWS) rtrn 0;
	cnst ::llc::u3_t cursor = down
		? app.FileChunkOffset + app.FileBytes.size() - ::llc::min(app.FileBytes.size(), (::llc::u2_t)4096)
		: app.FileChunkOffset + ::llc::min(app.FileBytes.size(), (::llc::u2_t)4096);
	cnst ::llc::u3_t newOffset = cursor > DESKTOP_FILE_MAX_BYTES / 2 ? cursor - DESKTOP_FILE_MAX_BYTES / 2 : 0;
	if(newOffset == app.FileChunkOffset) rtrn 0;
	rtrn ::desktopFileChunkLoad(app, newOffset, cursor);
}

sttc ::llc::err_t fileResultsRefresh(SSampleGUIViewport & app) {
	::llc::u2_t row = 0;
	rtrn app.FileResultControls.for_each([&](cnst ::llc::cid_t & id) -> ::llc::err_t {
		cnst ::llc::u2_t result = app.ResultOffset + row++;
		cnst bool visible = result < app.FileResults.size() && app.FileResults[result] < app.IndexPaths.size();
		app.GUI.Controls.SetHidden(id, !visible);
		if(visible) {
			::llc::vcst_t label = app.IndexPaths[app.FileResults[result]];
			if(label.size() > app.IndexRoot.size())
				if_fail_fe(label.slice(label, app.IndexRoot.size() + (app.IndexRoot[app.IndexRoot.size() - 1] == '/' ? 0 : 1)));
			if_fail_fe(label.slice(label, 0, ::llc::min(label.size(), ::llc::guiViewportTextColumns(app.GUI, id))));
			app.GUI.Controls.Text[id].Text = label;
			app.GUI.Controls.SetSelected(id, app.SelectedFile == app.IndexPaths[app.FileResults[result]]);
		}
		else {
			if_fail_fe(::llc::controlTextSet(app.GUI, id, {}));
		}
		rtrn 0;
	});
}

sttc ::llc::err_t fileResultsResolve(SSampleGUIViewport & app) {
	cnst ::llc::s3_t started = ::llc::timeCurrentInUs();
	if_fail_fe(app.FileResults.clear());
	app.ResultOffset = 0;
	if(app.IndexShardStarts.size()) {
		if(app.PendingSearch) app.PendingSearch->Cancel.store(true, ::std::memory_order_relaxed);
		app.SearchQueued = app.SearchInput.Text;
		if_fail_fe(::fileResultsRefresh(app));
		app.GUI.Controls.Text[app.SearchTiming].Text = app.SearchQueued.size() ? ::llc::vcst_t{"Searching saved index..."} : ::llc::vcst_t{"File matches 0"};
		rtrn 0;
	}
	if(app.IndexReady && app.SearchInput.Text.size())
		if_fail_fef(::indexResolveTerms(app.IndexWords, app.SearchInput.Text, app.IndexPaths.size(), app.FileResults), "Search:%.*s.", LLC_VIEW_ARGS(app.SearchInput.Text));
	if_fail_fe(::fileResultsRefresh(app));
	cnst ::llc::err_t textLength = ::llc::sprintf_s(app.SearchTimingText.Storage, "File matches %u | query %lld us", app.FileResults.size(), ::llc::timeCurrentInUs() - started);
	if_fail_fe(textLength);
	app.GUI.Controls.Text[app.SearchTiming].Text = {app.SearchTimingText.Storage, (::llc::u2_t)textLength};
	rtrn 0;
}

sttc ::llc::err_t fileIndexVisit(SFileIndexWork & index, ::llc::vcst_t folder, bool root) {
	::llc::SPathTree entries = {};
	cnst ::llc::err_t listed = ::llc::pathList(entries, folder, [&index] (bool isFolder, ::llc::vcst_c &) -> ::llc::err_t {
		if_true_fef(index.Cancel.load(::std::memory_order_relaxed), "%s", "Directory indexing cancelled.");
		rtrn isFolder ? 2 : 0;
	});
	if(::llc::failed(listed)) {
		if(index.Cancel.load(::std::memory_order_relaxed)) rtrn listed;
		if(root) rtrn listed;
		++index.SkippedFolders;
		rtrn 0;
	}
	::llc::u2_t iEntry = 0;
	rtrn entries.Name.for_each([&](cnst ::llc::vcst_t & name) -> ::llc::err_t {
		if_true_fef(index.Cancel.load(::std::memory_order_relaxed), "Folder:%.*s.", LLC_VIEW_ARGS(folder));
		cnst bool isFolder = entries.Info[iEntry++].Folder;
		::llc::string path = {};
		if_fail_fef(::llc::pathNameCompose(folder, name, path), "Entry:%.*s.", LLC_VIEW_ARGS(name));
		if(!isFolder) {
			if_fail_fef(index.Paths.push_back(path), "Path:%.*s.", LLC_VIEW_ARGS(path));
			index.Discovered.store(index.Paths.size(), ::std::memory_order_relaxed);
			rtrn 0;
		}
#if defined(_WIN32)
		cnst DWORD attributes = ::GetFileAttributesA(path.begin());
		if(attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
			++index.SkippedFolders;
			rtrn 0;
		}
#endif
		rtrn ::fileIndexVisit(index, path, false);
	});
}

sttc ::llc::err_t fileIndexWordsAppend(SFileIndexWork & index, ::std::unordered_map<::std::string, ::llc::u2_t> & lookup, ::llc::u2_t currentPath, ::llc::vcst_t path) {
	if_fail_fef(::llc::getWordList(index.Cache.ItemWords, index.Cache.ItemViews, {}), "Path:%.*s.", LLC_VIEW_ARGS(path));
	rtrn index.Cache.ItemWords.for_each([&](cnst ::llc::string & normalized) -> ::llc::err_t {
			::llc::vcst_t word = normalized;
			if(!word.size()) rtrn 0;
			if(word[word.size() - 1] == ',' || word[word.size() - 1] == '.')
				if_fail_fe(word.slice(word, 0, word.size() - 1));
			if(!word.size()) rtrn 0;
			::std::string key(word.begin(), word.size());
			cnst auto found = lookup.find(key);
			if(found != lookup.end()) {
				::llc::au2_t & postings = index.Words[found->second].Indices;
				if(!postings.size() || postings[postings.size() - 1] != currentPath)
					if_fail_fef(postings.push_back(currentPath), "Path:%.*s. Word:%.*s.", LLC_VIEW_ARGS(path), LLC_VIEW_ARGS(word));
				rtrn 0;
			}
			cnst ::llc::err_t iWord = index.Words.push_back({word});
			if_fail_fef(iWord, "Path:%.*s. Word:%.*s.", LLC_VIEW_ARGS(path), LLC_VIEW_ARGS(word));
			if_fail_fe(index.Words[iWord].Indices.push_back(currentPath));
			lookup.emplace(::std::move(key), (::llc::u2_t)iWord);
			rtrn 0;
		});
	}

sttc ::llc::err_t fileIndexBuild(SFileIndexWork & index, SFileIndexWork * global = nullptr, ::llc::u2_t base = 0, ::llc::u2_t maxWords = 0, ::llc::u2_t * processed = nullptr, cnst ::llc::function<::llc::err_t(SFileIndexWork &)> & onFlush = {}) {
	if(!global && !index.Paths.size())
		if_fail_fef(::fileIndexVisit(index, index.Root, true), "Index directory:%.*s.", LLC_VIEW_ARGS(index.Root));
	::std::unordered_map<::std::string, ::llc::u2_t> lookup = {};
	::llc::u2_t iPath = 0, countedWords = 0;
	cnst auto flushIfFull = [&]() -> ::llc::err_t {
		if(maxWords && index.Words.size() >= maxWords && onFlush) {
			if(global) global->IndexedWords.fetch_add(index.Words.size() - countedWords, ::std::memory_order_relaxed);
			if_fail_fe(onFlush(index));
			if_fail_fe(index.Words.clear());
			lookup.clear();
			countedWords = 0;
		}
		rtrn 0;
	};
	while(iPath < index.Paths.size()) {
		cnst ::llc::u2_t stop = iPath + 1;
		cnst ::llc::err_t indexed = index.Paths.for_each([&](cnst ::llc::string & path) -> ::llc::err_t {
		if_true_fef((global ? global : &index)->Cancel.load(::std::memory_order_relaxed), "Path:%.*s.", LLC_VIEW_ARGS(path));
		cnst ::llc::u2_t currentPath = iPath++;
		if_fail_fe(index.Cache.ItemViews.clear());
		::llc::vcst_t relativePath = path;
		if(path.size() > index.Root.size())
			if_fail_fef(relativePath.slice(relativePath, index.Root.size() + (index.Root[index.Root.size() - 1] == '/' ? 0 : 1)), "Path:%.*s. Root:%.*s.", LLC_VIEW_ARGS(path), LLC_VIEW_ARGS(index.Root));
		if_fail_fef(index.Cache.ItemViews.push_back(relativePath), "Path:%.*s.", LLC_VIEW_ARGS(path));
		if_fail_fef(::fileIndexWordsAppend(index, lookup, currentPath, path), "Path:%.*s.", LLC_VIEW_ARGS(path));
		if_fail_fef(flushIfFull(), "Path:%.*s.", LLC_VIEW_ARGS(path));
		cnst ::llc::s3_t length = ::llc::fileSize(path);
		if(length < 0) ++index.Unreadable;
		::llc::u3_t offset = 0;
		while(offset < (::llc::u3_t)::llc::max((::llc::s3_t)0, length)) {
			if_true_fef((global ? global : &index)->Cancel.load(::std::memory_order_relaxed), "Path:%.*s. Offset:%llu.", LLC_VIEW_ARGS(path), offset);
			if_fail_fe(index.Bytes.clear());
			cnst ::llc::u2_t chunk = (::llc::u2_t)::llc::min(offset ? (::llc::u3_t)1024 * 1024 : (::llc::u3_t)8192, (::llc::u3_t)length - offset);
			cnst ::llc::err_t readResult = ::llc::fileToMemory(path, index.Bytes, chunk, offset);
			if(::llc::failed(readResult)) {
				if(offset) ++index.Truncated;
				else ++index.Unreadable;
				offset = (::llc::u3_t)length;
			}
			else {
				index.BytesRead.fetch_add(index.Bytes.size(), ::std::memory_order_relaxed);
				if(global) global->BytesRead.fetch_add(index.Bytes.size(), ::std::memory_order_relaxed);
				cnst char zero = 0;
				if(::llc::vcst_t{index.Bytes.begin(), index.Bytes.size()}.find(zero) >= 0) {
					++index.SkippedBinary;
					offset = (::llc::u3_t)length;
				}
				else {
					if_fail_fe(index.Bytes.for_each([](char & ch) -> ::llc::err_t { if((::llc::u0_t)ch <= ' ') ch = ' '; rtrn 0; }));
					if_fail_fe(index.Cache.ItemViews.clear());
					if_fail_fef(::llc::split(::llc::vcst_t{index.Bytes.begin(), index.Bytes.size()}, ' ', index.Cache.ItemViews), "Path:%.*s. Offset:%llu.", LLC_VIEW_ARGS(path), offset);
					if_fail_fef(::fileIndexWordsAppend(index, lookup, currentPath, path), "Path:%.*s. Offset:%llu.", LLC_VIEW_ARGS(path), offset);
					offset += index.Bytes.size();
					if(offset < (::llc::u3_t)length)
						offset -= ::llc::min(index.Bytes.size(), (::llc::u2_t)128);
					if_fail_fef(flushIfFull(), "Path:%.*s. Offset:%llu.", LLC_VIEW_ARGS(path), offset);
				}
			}
		}
		index.IndexedWords.store(index.Words.size(), ::std::memory_order_relaxed);
		index.Processed.store(currentPath + 1, ::std::memory_order_relaxed);
		if(global) {
			global->IndexedWords.fetch_add(index.Words.size() - countedWords, ::std::memory_order_relaxed);
			global->Processed.store(base + currentPath + 1, ::std::memory_order_relaxed);
			countedWords = index.Words.size();
		}
		if_fail_fef(flushIfFull(), "Path:%.*s.", LLC_VIEW_ARGS(path));
		rtrn 0;
		}, iPath, stop);
		if_fail_fef(indexed, "Index directory:%.*s. Base:%u.", LLC_VIEW_ARGS(index.Root), base);
	}
	if(processed) *processed = iPath;
	rtrn 0;
}

sttc ::llc::err_t fileIndexPublish(::llc::vcu0_t output, ::llc::vcst_t databasePath) {
	if_true_fef(output.size() > SEARCH_INDEX_MAX_BYTES, "Database:%.*s. Bytes:%u.", LLC_VIEW_ARGS(databasePath), output.size());
	::llc::string temporary = {};
	if_fail_fe(::llc::append_strings(temporary, databasePath, ".tmp"));
	if_fail_fef(::llc::fileFromMemory(temporary, output), "Database temporary:%.*s.", LLC_VIEW_ARGS(temporary));
#if defined(_WIN32)
	if(!::MoveFileExA(temporary.begin(), databasePath.begin(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		cnst DWORD error = ::GetLastError();
		::llc::fileDelete(temporary);
		if_true_fef(true, "Database:%.*s. MoveFileEx error:%u.", LLC_VIEW_ARGS(databasePath), error);
	}
#else
	if(::rename(temporary.begin(), databasePath.begin())) {
		::llc::fileDelete(temporary);
		if_true_fef(true, "Database:%.*s. rename failed.", LLC_VIEW_ARGS(databasePath));
	}
#endif
	rtrn 0;
}

sttc ::llc::err_t fileIndexSave(cnst SFileIndexWork & index, ::llc::vcst_t databasePath, bool compress = false) {
	::llc::au0_t output = {};
	if_fail_fef(::llc::savePOD(output, SEARCH_INDEX_MAGIC), "Database:%.*s.", LLC_VIEW_ARGS(databasePath));
	if_fail_fe(::llc::saveView(output, ::llc::vcst_t{index.Root.begin(), index.Root.size()}));
	if_fail_fe(::llc::savePOD(output, index.Paths.size()));
	if_fail_fe(index.Paths.for_each([&output](cnst ::llc::string & path) -> ::llc::err_t {
		rtrn ::llc::saveView(output, ::llc::vcst_t{path.begin(), path.size()});
	}));
	if_fail_fe(::llc::savePOD(output, index.Words.size()));
	if_fail_fe(index.Words.for_each([&output](cnst ::llc::SWordIndices & word) -> ::llc::err_t {
		if_fail_fe(::llc::saveView(output, ::llc::vcst_t{word.Text.begin(), word.Text.size()}));
		rtrn ::llc::saveView(output, ::llc::vcu2_t{word.Indices.begin(), word.Indices.size()});
	}));
	if(!compress) rtrn ::fileIndexPublish(output, databasePath);
	::llc::au0_t packed = {}, wrapped = {};
	if_fail_fef(::llc::arrayDeflate(output, packed), "Compress shard:%.*s. Bytes:%u.", LLC_VIEW_ARGS(databasePath), output.size());
	if_fail_fe(::llc::savePOD(wrapped, SEARCH_DEFLATE_MAGIC));
	if_fail_fe(::llc::savePOD(wrapped, output.size()));
	if_fail_fe(::llc::saveView(wrapped, ::llc::vcu0_t{packed.begin(), packed.size()}));
	rtrn ::fileIndexPublish(wrapped.size() < output.size() ? ::llc::vcu0_t{wrapped.begin(), wrapped.size()} : ::llc::vcu0_t{output.begin(), output.size()}, databasePath);
}

sttc ::llc::err_t fileIndexShardPath(::llc::vcst_t databasePath, ::llc::u3_t generation, ::llc::u2_t iShard, ::llc::string & path) {
	::llc::astsc_t<64> suffix = {};
	cnst ::llc::err_t count = ::llc::sprintf_s(suffix.Storage, ".%llu.%u", generation, iShard);
	if_fail_fef(count, "Database:%.*s. Generation:%llu. Shard:%u.", LLC_VIEW_ARGS(databasePath), generation, iShard);
	rtrn ::llc::append_strings(path, databasePath, ::llc::vcst_t{suffix.Storage, (::llc::u2_t)count});
}

sttc ::llc::err_t fileIndexCharsPath(::llc::vcst_t databasePath, ::llc::u3_t generation, ::llc::string & path) {
	::llc::astsc_t<64> suffix = {};
	cnst ::llc::err_t count = ::llc::sprintf_s(suffix.Storage, ".chars.%llu", generation);
	if_fail_fef(count, "Database:%.*s. Generation:%llu.", LLC_VIEW_ARGS(databasePath), generation);
	rtrn ::llc::append_strings(path, databasePath, ::llc::vcst_t{suffix.Storage, (::llc::u2_t)count});
}

sttc ::llc::err_t fileIndexGramPath(::llc::vcst_t databasePath, ::llc::u3_t generation, ::llc::u2_t iShard, ::llc::string & path) {
	::llc::astsc_t<64> suffix = {};
	cnst ::llc::err_t count = ::llc::sprintf_s(suffix.Storage, ".gram.%llu.%u", generation, iShard);
	if_fail_fef(count, "Database:%.*s. Generation:%llu. Shard:%u.", LLC_VIEW_ARGS(databasePath), generation, iShard);
	rtrn ::llc::append_strings(path, databasePath, ::llc::vcst_t{suffix.Storage, (::llc::u2_t)count});
}

sttc ::llc::err_t fileIndexRetire(::llc::vcst_t databasePath, ::llc::u3_t generation, ::llc::u2_t count) {
	::llc::u2_t iShard = 0;
	while(iShard < count) {
		::llc::string path = {};
		if_fail_fef(::fileIndexShardPath(databasePath, generation, iShard++, path), "Retire index:%.*s. Generation:%llu.", LLC_VIEW_ARGS(databasePath), generation);
		if_fail_fef(::llc::fileDelete(path), "Retire shard:%.*s.", LLC_VIEW_ARGS(path));
		::llc::string gramPath = {};
		if_fail_fe(::fileIndexGramPath(databasePath, generation, iShard - 1, gramPath));
		if(::llc::fileSize(gramPath) > 0)
			if_fail_fef(::llc::fileDelete(gramPath), "Retire gram route:%.*s.", LLC_VIEW_ARGS(gramPath));
	}
	::llc::string charsPath = {};
	if_fail_fe(::fileIndexCharsPath(databasePath, generation, charsPath));
	if(::llc::fileSize(charsPath) > 0)
		if_fail_fef(::llc::fileDelete(charsPath), "Retire character route:%.*s.", LLC_VIEW_ARGS(charsPath));
	rtrn 0;
}

stct SFileShardRollback {
	::llc::vcst_t DatabasePath = {};
	::llc::u3_t Generation = 0;
	::llc::u2_t Saved = 0;
	::llc::u2_t CheckpointSaved = 0;
	bool Committed = false;
	~SFileShardRollback() {
		if(Committed) rtrn;
		::llc::u2_t iShard = CheckpointSaved;
		while(iShard < Saved) {
			::llc::string path = {};
			if(!::llc::failed(::fileIndexShardPath(DatabasePath, Generation, iShard, path)))
				(void)::llc::fileDelete(path);
			::llc::string gramPath = {};
			if(!::llc::failed(::fileIndexGramPath(DatabasePath, Generation, iShard++, gramPath)) && ::llc::fileSize(gramPath) > 0)
				(void)::llc::fileDelete(gramPath);
		}
	}
};

sttc ::llc::err_t fileIndexManifestSave(cnst SFileIndexWork & index, ::llc::vcst_t databasePath, bool partial = false) {
	::llc::au0_t output = {};
	if_fail_fe(::llc::savePOD(output, partial ? SEARCH_PARTIAL_MAGIC : SEARCH_SHARD_MAGIC));
	if_fail_fe(::llc::saveView(output, ::llc::vcst_t{index.Root.begin(), index.Root.size()}));
	if_fail_fe(::llc::savePOD(output, index.Paths.size()));
	if_fail_fe(index.Paths.for_each([&output](cnst ::llc::string & path) -> ::llc::err_t {
		rtrn ::llc::saveView(output, ::llc::vcst_t{path.begin(), path.size()});
	}));
	if_fail_fe(::llc::savePOD(output, index.ShardGeneration));
	if_fail_fe(::llc::saveView(output, ::llc::vcu2_t{index.ShardStarts.begin(), index.ShardStarts.size()}));
	if(partial) if_fail_fe(::llc::savePOD(output, index.CompletedPaths));
	rtrn ::fileIndexPublish(output, databasePath);
}

sttc ::llc::err_t fileIndexLoad(SFileIndexWork & index, ::llc::vcst_t databasePath, bool loadPaths = true) {
	cnst ::llc::s3_t fileLength = ::llc::fileSize(databasePath);
	if_true_fef(fileLength <= 0 || fileLength > SEARCH_INDEX_MAX_BYTES, "Database:%.*s. Bytes:%lld.", LLC_VIEW_ARGS(databasePath), fileLength);
	::llc::au0_t data = {};
	if_fail_fef(::llc::fileToMemory(databasePath, data, SEARCH_INDEX_MAX_BYTES), "Database:%.*s.", LLC_VIEW_ARGS(databasePath));
	::llc::vcu0_t input = data;
	::llc::au0_t inflated = {};
	::llc::u2_t magic = 0, pathCount = 0, wordCount = 0;
	if_fail_fe(::llc::loadPOD(input, magic));
	if(magic == SEARCH_DEFLATE_MAGIC) {
		::llc::u2_t rawBytes = 0;
		::llc::vcu0_t packed = {};
		if_fail_fe(::llc::loadPOD(input, rawBytes));
		if_fail_fe(::llc::loadView(input, packed));
		if_true_fef(!rawBytes || rawBytes > SEARCH_INDEX_MAX_BYTES || input.size(), "Compressed shard:%.*s. Raw:%u. Tail:%u.", LLC_VIEW_ARGS(databasePath), rawBytes, input.size());
		if_fail_fef(::llc::arrayInflate(packed, inflated), "Decompress shard:%.*s.", LLC_VIEW_ARGS(databasePath));
		if_true_fef(inflated.size() != rawBytes, "Decompressed shard:%.*s. Expected:%u. Actual:%u.", LLC_VIEW_ARGS(databasePath), rawBytes, inflated.size());
		input = inflated;
		if_fail_fe(::llc::loadPOD(input, magic));
	}
	if_true_fef(magic != SEARCH_INDEX_MAGIC && magic != SEARCH_SHARD_MAGIC && magic != SEARCH_PARTIAL_MAGIC, "Database:%.*s. Magic:%u.", LLC_VIEW_ARGS(databasePath), magic);
	::llc::vcst_t root = {};
	if_fail_fe(::llc::loadView(input, root));
	index.Root = root;
	if_fail_fe(::llc::loadPOD(input, pathCount));
	index.StoredPaths = pathCount;
	if_true_fef(pathCount > 1000000 || pathCount > input.size(), "Database:%.*s. Paths:%u. Bytes:%u.", LLC_VIEW_ARGS(databasePath), pathCount, input.size());
	if(loadPaths) {
		if_fail_fe(index.Paths.resize(pathCount));
		if_fail_fe(index.Paths.for_each([&input](::llc::string & path) -> ::llc::err_t {
			::llc::vcst_t view = {};
			if_fail_fe(::llc::loadView(input, view));
			path = view;
			rtrn 0;
		}));
	}
	else {
		::llc::u2_t iPath = 0;
		while(iPath++ < pathCount) {
			::llc::vcst_t view = {};
			if_fail_fef(::llc::loadView(input, view), "Database:%.*s. Path:%u.", LLC_VIEW_ARGS(databasePath), iPath - 1);
		}
	}
	if(magic == SEARCH_SHARD_MAGIC || magic == SEARCH_PARTIAL_MAGIC) {
		if_fail_fe(::llc::loadPOD(input, index.ShardGeneration));
		::llc::vcu2_t starts = {};
		if_fail_fe(::llc::loadView(input, starts));
		if_true_fef(starts.size() == 0 || starts.size() > 1000000 || starts[0] != 0, "Database:%.*s. Shards:%u. Paths:%u.", LLC_VIEW_ARGS(databasePath), starts.size(), pathCount);
		::llc::u2_t prior = (::llc::u2_t)-1;
		cnst ::llc::err_t startsValid = starts.for_each([&](cnst ::llc::u2_t & start) -> ::llc::err_t {
			if_true_fef((prior != (::llc::u2_t)-1 && start < prior) || start >= pathCount, "Database:%.*s. Shard start:%u. Prior:%u. Paths:%u.", LLC_VIEW_ARGS(databasePath), start, prior, pathCount);
			prior = start;
			rtrn 0;
		});
		if_fail_fef(startsValid, "Database:%.*s.", LLC_VIEW_ARGS(databasePath));
		if_fail_fe(index.ShardStarts.append(starts));
		if(magic == SEARCH_PARTIAL_MAGIC) {
			if_fail_fe(::llc::loadPOD(input, index.CompletedPaths));
			if_true_fef(index.CompletedPaths == 0 || index.CompletedPaths > pathCount || starts[starts.size() - 1] >= index.CompletedPaths, "Database:%.*s. Completed:%u. Paths:%u.", LLC_VIEW_ARGS(databasePath), index.CompletedPaths, pathCount);
		}
		else index.CompletedPaths = pathCount;
		if_true_fef(input.size(), "Database:%.*s. Trailing bytes:%u.", LLC_VIEW_ARGS(databasePath), input.size());
		rtrn 0;
	}
	if_fail_fe(::llc::loadPOD(input, wordCount));
	if_true_fef(wordCount > 10000000 || wordCount > input.size(), "Database:%.*s. Words:%u. Bytes:%u.", LLC_VIEW_ARGS(databasePath), wordCount, input.size());
	if_fail_fe(index.Words.resize(wordCount));
	cnst ::llc::err_t wordsLoaded = index.Words.for_each([&input, pathCount](::llc::SWordIndices & word) -> ::llc::err_t {
		::llc::vcst_t text = {};
		::llc::vcu2_t postings = {};
		if_fail_fe(::llc::loadView(input, text));
		if_fail_fe(::llc::loadView(input, postings));
		word.Text = text;
		cnst ::llc::err_t postingsValid = postings.for_each([pathCount](cnst ::llc::u2_t & iPath) -> ::llc::err_t {
			if_true_fef(iPath >= pathCount, "Database posting:%u. Paths:%u.", iPath, pathCount);
			rtrn 0;
		});
		if_fail_fef(postingsValid, "Database paths:%u.", pathCount);
		rtrn word.Indices.append(postings);
	});
	if_fail_fef(wordsLoaded, "Database:%.*s.", LLC_VIEW_ARGS(databasePath));
	if_true_fef(input.size(), "Database:%.*s. Trailing bytes:%u.", LLC_VIEW_ARGS(databasePath), input.size());
	rtrn 0;
}

sttc ::llc::err_t fileIndexCharsAccumulate(SFileIndexWork & index, cnst SFileIndexWork & shard, ::llc::u2_t base) {
	rtrn shard.Words.for_each([&](cnst ::llc::SWordIndices & word) -> ::llc::err_t {
		bool seen[256] = {};
		rtrn word.Text.for_each([&](cnst char & character) -> ::llc::err_t {
			cnst ::llc::u0_t ch = (::llc::u0_t)character;
			if(seen[ch]) rtrn 0;
			seen[ch] = true;
			rtrn word.Indices.for_each([&](cnst ::llc::u2_t & local) -> ::llc::err_t {
				if_true_fef(base + local >= index.Paths.size(), "Route base:%u. Local:%u. Paths:%u.", base, local, index.Paths.size());
				index.Chars[(base + local) * 4 + ch / 64] |= (::llc::u3_t)1 << (ch % 64);
				rtrn 0;
			});
		});
	});
}

sttc ::llc::err_t fileIndexCharsSave(cnst SFileIndexWork & index, ::llc::vcst_t databasePath) {
	if_true_fef(!index.CompletedPaths || index.CompletedPaths > index.Paths.size() || index.Chars.size() != index.Paths.size() * 4, "Route completed:%u. Paths:%u. Words:%u.", index.CompletedPaths, index.Paths.size(), index.Chars.size());
	::llc::au0_t output = {};
	if_fail_fe(::llc::savePOD(output, SEARCH_CHARS_MAGIC));
	if_fail_fe(::llc::savePOD(output, index.ShardGeneration));
	if_fail_fe(::llc::savePOD(output, index.CompletedPaths));
	if_fail_fe(::llc::savePOD(output, index.Paths.size()));
	if_fail_fe(::llc::saveView(output, ::llc::vcu3_t{index.Chars.begin(), index.CompletedPaths * 4}));
	::llc::string path = {};
	if_fail_fe(::fileIndexCharsPath(databasePath, index.ShardGeneration, path));
	rtrn ::fileIndexPublish(output, path);
}

sttc ::llc::err_t fileIndexCharsLoad(SFileIndexWork & index, ::llc::vcst_t databasePath) {
	::llc::string path = {};
	if_fail_fe(::fileIndexCharsPath(databasePath, index.ShardGeneration, path));
	::llc::au0_t data = {};
	if_fail_fef(::llc::fileToMemory(path, data, SEARCH_INDEX_MAX_BYTES), "Route:%.*s.", LLC_VIEW_ARGS(path));
	::llc::vcu0_t input = data;
	::llc::u2_t magic = 0, completed = 0, paths = 0;
	::llc::u3_t generation = 0;
	::llc::vcu3_t chars = {};
	if_fail_fe(::llc::loadPOD(input, magic));
	if_fail_fe(::llc::loadPOD(input, generation));
	if_fail_fe(::llc::loadPOD(input, completed));
	if_fail_fe(::llc::loadPOD(input, paths));
	if_fail_fe(::llc::loadView(input, chars));
	if_true_fef(magic != SEARCH_CHARS_MAGIC || generation != index.ShardGeneration || completed != index.CompletedPaths || paths != index.Paths.size() || (chars.size() != completed * 4 && chars.size() != paths * 4) || input.size(), "Route:%.*s. Generation:%llu. Completed:%u. Paths:%u. Words:%u. Tail:%u.", LLC_VIEW_ARGS(path), generation, completed, paths, chars.size(), input.size());
	if_fail_fe(index.Chars.clear());
	if_fail_fe(index.Chars.append(chars));
	if_fail_fe(index.Chars.resize(paths * 4));
	::llc::view<::llc::u3_t> tail = {index.Chars.begin() + chars.size(), index.Chars.size() - chars.size()};
	rtrn tail.fill(0);
}

sttc ::llc::err_t fileIndexCharsEnsure(SFileIndexWork & index, ::llc::vcst_t databasePath) {
	if(!index.ShardStarts.size()) rtrn 0;
	::llc::string path = {};
	if_fail_fe(::fileIndexCharsPath(databasePath, index.ShardGeneration, path));
	if(::llc::fileSize(path) > 0 && !::llc::failed(::fileIndexCharsLoad(index, databasePath))) rtrn 0;
	if_fail_fe(index.Chars.resize(index.Paths.size() * 4));
	if_fail_fe(index.Chars.fill(0));
	cnst ::llc::err_t rebuilt = index.ShardStarts.enumerate([&](::llc::u2_t iShard, cnst ::llc::u2_t & base) -> ::llc::err_t {
		::llc::string shardPath = {};
		if_fail_fe(::fileIndexShardPath(databasePath, index.ShardGeneration, iShard, shardPath));
		SFileIndexWork shard = {};
		if_fail_fef(::fileIndexLoad(shard, shardPath, false), "Route shard:%.*s.", LLC_VIEW_ARGS(shardPath));
		rtrn ::fileIndexCharsAccumulate(index, shard, base);
	});
	if_fail_fef(rebuilt, "Route index:%.*s.", LLC_VIEW_ARGS(databasePath));
	rtrn ::fileIndexCharsSave(index, databasePath);
}

sttc ::llc::err_t fileIndexGramBuild(cnst SFileIndexWork & shard, SGramRoute & route) {
	route.Paths = shard.StoredPaths ? shard.StoredPaths : shard.Paths.size();
	if_true_fef(!route.Paths || route.Paths > SEARCH_SHARD_MAX_FILES, "Gram shard paths:%u.", route.Paths);
	::std::unordered_set<::llc::u3_t> pairs2 = {}, pairs3 = {}, pairs4 = {};
	if_fail_fe(shard.Words.for_each([&](cnst ::llc::SWordIndices & word) -> ::llc::err_t {
		rtrn word.Text.enumerate([&](::llc::u2_t i, cnst char &) -> ::llc::err_t {
			if(i + 1 >= word.Text.size()) rtrn 0;
			cnst ::llc::u2_t gram2 = ((::llc::u0_t)word.Text[i] << 8) | (::llc::u0_t)word.Text[i + 1];
			cnst bool has3 = i + 2 < word.Text.size();
			cnst ::llc::u2_t gram3 = has3 ? (gram2 << 8) | (::llc::u0_t)word.Text[i + 2] : 0;
			cnst bool has4 = i + 3 < word.Text.size();
			cnst ::llc::u2_t gram4 = has4 ? (gram3 << 8) | (::llc::u0_t)word.Text[i + 3] : 0;
			rtrn word.Indices.for_each([&](cnst ::llc::u2_t & file) -> ::llc::err_t {
				if_true_fef(file >= route.Paths, "Gram shard file:%u. Paths:%u.", file, route.Paths);
				pairs2.insert(((::llc::u3_t)gram2 << 32) | file);
				if(has3) pairs3.insert(((::llc::u3_t)gram3 << 32) | file);
				if(has4) pairs4.insert(((::llc::u3_t)gram4 << 32) | file);
				rtrn 0;
			});
		});
	}));
	cnst auto tableBuild = [&](cnst ::std::unordered_set<::llc::u3_t> & pairs, ::llc::apod<SGramRange> & ranges, ::llc::au1_t & files) -> ::llc::err_t {
		if_true_fef(pairs.size() > 0xFFFFFFFFULL, "Gram pairs:%llu.", (::llc::u3_t)pairs.size());
		::llc::au3_t sorted = {};
		if_fail_fe(sorted.resize((::llc::u2_t)pairs.size()));
		auto it = pairs.begin();
		if_fail_fe(sorted.for_each([&](::llc::u3_t & pair) -> ::llc::err_t { pair = *it++; rtrn 0; }));
		::std::sort(sorted.begin(), sorted.end());
		rtrn sorted.for_each([&](cnst ::llc::u3_t & pair) -> ::llc::err_t {
			cnst ::llc::u2_t gram = (::llc::u2_t)(pair >> 32);
			cnst ::llc::u2_t file = (::llc::u2_t)pair;
			if(!ranges.size() || ranges[ranges.size() - 1].Gram != gram)
				if_fail_fe(ranges.push_back({gram, files.size(), 0}));
			if_fail_fe(files.push_back((::llc::u1_t)file));
			++ranges[ranges.size() - 1].Count;
			rtrn 0;
		});
	};
	if_fail_fe(tableBuild(pairs2, route.Two, route.FilesTwo));
	if_fail_fe(tableBuild(pairs3, route.Three, route.FilesThree));
	rtrn tableBuild(pairs4, route.Four, route.FilesFour);
}

sttc ::llc::err_t fileIndexGramSave(cnst SGramRoute & route, ::llc::vcst_t databasePath, ::llc::u3_t generation, ::llc::u2_t iShard) {
	SGramDiskHeader header = {};
	header.Shard = iShard;
	header.Paths = route.Paths;
	header.Generation = generation;
	::llc::u3_t offset = sizeof(header);
	cnst auto sectionSet = [&](SGramSection & section, ::llc::u2_t count, ::llc::u2_t width) -> ::llc::err_t {
		if_true_fef(offset + (::llc::u3_t)count * width > SEARCH_INDEX_MAX_BYTES, "Gram shard:%u. Count:%u. Width:%u.", iShard, count, width);
		section = {(::llc::u2_t)offset, count};
		offset += (::llc::u3_t)count * width;
		rtrn 0;
	};
	if_fail_fe(sectionSet(header.Ranges[0], route.Two.size(), sizeof(SGramRange)));
	if_fail_fe(sectionSet(header.Ranges[1], route.Three.size(), sizeof(SGramRange)));
	if_fail_fe(sectionSet(header.Ranges[2], route.Four.size(), sizeof(SGramRange)));
	if_fail_fe(sectionSet(header.Files[0], route.FilesTwo.size(), sizeof(::llc::u1_t)));
	if_fail_fe(sectionSet(header.Files[1], route.FilesThree.size(), sizeof(::llc::u1_t)));
	if_fail_fe(sectionSet(header.Files[2], route.FilesFour.size(), sizeof(::llc::u1_t)));
	cnst auto bucketsSet = [&](::llc::view<cnst SGramRange> ranges, ::llc::u2_t slot) -> ::llc::err_t {
		return ranges.enumerate([&](::llc::u2_t i, cnst SGramRange & range) -> ::llc::err_t {
			cnst ::llc::u2_t first = range.Gram >> (8 * (slot + 1));
			SGramSection & bucket = header.Buckets[slot][first];
			if(!bucket.Count) bucket.Offset = i;
			++bucket.Count;
			rtrn 0;
		});
	};
	if_fail_fe(bucketsSet(route.Two, 0));
	if_fail_fe(bucketsSet(route.Three, 1));
	if_fail_fe(bucketsSet(route.Four, 2));
	::llc::au0_t output = {};
	if_fail_fe(::llc::savePOD(output, header));
	if_fail_fe(output.append(::llc::view<cnst SGramRange>{route.Two.begin(), route.Two.size()}.cu8()));
	if_fail_fe(output.append(::llc::view<cnst SGramRange>{route.Three.begin(), route.Three.size()}.cu8()));
	if_fail_fe(output.append(::llc::view<cnst SGramRange>{route.Four.begin(), route.Four.size()}.cu8()));
	if_fail_fe(output.append(::llc::vcu1_t{route.FilesTwo.begin(), route.FilesTwo.size()}.cu8()));
	if_fail_fe(output.append(::llc::vcu1_t{route.FilesThree.begin(), route.FilesThree.size()}.cu8()));
	if_fail_fe(output.append(::llc::vcu1_t{route.FilesFour.begin(), route.FilesFour.size()}.cu8()));
	::llc::string path = {};
	if_fail_fe(::fileIndexGramPath(databasePath, generation, iShard, path));
	rtrn ::fileIndexPublish(output, path);
}

sttc ::llc::err_t fileIndexGramHeaderLoad(SGramDiskHeader & header, ::llc::vcst_t databasePath, ::llc::u3_t generation, ::llc::u2_t iShard) {
	::llc::string path = {};
	if_fail_fe(::fileIndexGramPath(databasePath, generation, iShard, path));
	::llc::au0_t data = {};
	if_fail_fef(::llc::fileToMemory(path, data, sizeof(header)), "Gram route:%.*s.", LLC_VIEW_ARGS(path));
	if_true_fef(data.size() != sizeof(header), "Gram route:%.*s. Header bytes:%u.", LLC_VIEW_ARGS(path), data.size());
	::llc::vcu0_t input = data;
	if_fail_fe(::llc::loadPOD(input, header));
	if_true_fef(header.Magic != SEARCH_GRAM_MAGIC || header.Generation != generation || header.Shard != iShard || !header.Paths || header.Paths > SEARCH_SHARD_MAX_FILES, "Gram route:%.*s. Magic:%u. Generation:%llu. Shard:%u. Paths:%u.", LLC_VIEW_ARGS(path), header.Magic, header.Generation, header.Shard, header.Paths);
	cnst ::llc::s3_t fileBytes = ::llc::fileSize(path);
	if_true_fef(fileBytes < (::llc::s3_t)sizeof(header) || fileBytes > SEARCH_INDEX_MAX_BYTES, "Gram route:%.*s. Bytes:%lld.", LLC_VIEW_ARGS(path), fileBytes);
	cnst SGramSection sections[] = {header.Ranges[0], header.Ranges[1], header.Ranges[2], header.Files[0], header.Files[1], header.Files[2]};
	cnst ::llc::view<cnst SGramSection> sectionView = {sections, ::llc::size(sections)};
	::llc::u3_t expected = sizeof(header);
	if_fail_fe(sectionView.enumerate([&](::llc::u2_t i, cnst SGramSection & section) -> ::llc::err_t {
		cnst ::llc::u2_t width = i < 3 ? sizeof(SGramRange) : sizeof(::llc::u1_t);
		if_true_fef(section.Offset != expected || expected + (::llc::u3_t)section.Count * width > (::llc::u3_t)fileBytes, "Gram route:%.*s. Section:%u. Offset:%u. Count:%u. Expected:%llu.", LLC_VIEW_ARGS(path), i, section.Offset, section.Count, expected);
		expected += (::llc::u3_t)section.Count * width;
		rtrn 0;
	}));
	if_true_fef(expected != (::llc::u3_t)fileBytes, "Gram route:%.*s. Expected:%llu. Bytes:%lld.", LLC_VIEW_ARGS(path), expected, fileBytes);
	rtrn 0;
}

sttc ::llc::err_t fileIndexGramEnsure(SGramDiskHeader & header, ::llc::vcst_t databasePath, ::llc::u3_t generation, ::llc::u2_t iShard) {
	::llc::string gramPath = {};
	if_fail_fe(::fileIndexGramPath(databasePath, generation, iShard, gramPath));
	if(::llc::fileSize(gramPath) > 0 && !::llc::failed(::fileIndexGramHeaderLoad(header, databasePath, generation, iShard))) rtrn 0;
	::llc::string shardPath = {};
	if_fail_fe(::fileIndexShardPath(databasePath, generation, iShard, shardPath));
	SFileIndexWork shard = {};
	if_fail_fef(::fileIndexLoad(shard, shardPath, false), "Gram shard:%.*s.", LLC_VIEW_ARGS(shardPath));
	SGramRoute route = {};
	if_fail_fef(::fileIndexGramBuild(shard, route), "Gram shard:%.*s.", LLC_VIEW_ARGS(shardPath));
	if_fail_fe(::fileIndexGramSave(route, databasePath, generation, iShard));
	rtrn ::fileIndexGramHeaderLoad(header, databasePath, generation, iShard);
}

sttc ::llc::err_t fileIndexGramResolve(cnst SGramDiskHeader & header, ::llc::vcst_t databasePath, ::llc::u2_t gram, ::llc::u2_t length, ::llc::au2_t & matches) {
	cnst ::llc::u2_t slot = length - 2;
	if_true_fef(slot >= ::llc::size(header.Ranges), "Gram length:%u.", length);
	cnst SGramSection & rangeSection = header.Ranges[slot], & fileSection = header.Files[slot];
	cnst SGramSection & bucket = header.Buckets[slot][(gram >> (8 * (slot + 1))) & 0xFF];
	if(!bucket.Count) rtrn 0;
	if_true_fef(bucket.Offset > rangeSection.Count || bucket.Count > rangeSection.Count - bucket.Offset, "Gram:%u. Bucket:%u. Count:%u. Ranges:%u.", gram, bucket.Offset, bucket.Count, rangeSection.Count);
	::llc::string path = {};
	if_fail_fe(::fileIndexGramPath(databasePath, header.Generation, header.Shard, path));
	::llc::au0_t bytes = {};
	if_fail_fef(::llc::fileToMemory(path, bytes, bucket.Count * sizeof(SGramRange), rangeSection.Offset + bucket.Offset * sizeof(SGramRange)), "Gram ranges:%.*s. Length:%u.", LLC_VIEW_ARGS(path), length);
	if_true_fef(bytes.size() != bucket.Count * sizeof(SGramRange), "Gram ranges:%.*s. Bytes:%u. Count:%u.", LLC_VIEW_ARGS(path), bytes.size(), bucket.Count);
	::llc::view<cnst SGramRange> ranges = {(cnst SGramRange*)bytes.begin(), bucket.Count};
	cnst SGramRange * found = ::std::lower_bound(ranges.begin(), ranges.end(), gram, [](cnst SGramRange & range, ::llc::u2_t value) { rtrn range.Gram < value; });
	if(found == ranges.end() || found->Gram != gram) rtrn 0;
	if_true_fef(found->Start > fileSection.Count || found->Count > fileSection.Count - found->Start, "Gram:%u. Start:%u. Count:%u. Files:%u.", gram, found->Start, found->Count, fileSection.Count);
	if(!found->Count) rtrn 0;
	::llc::au0_t postings = {};
	if_fail_fef(::llc::fileToMemory(path, postings, found->Count * sizeof(::llc::u1_t), fileSection.Offset + found->Start * sizeof(::llc::u1_t)), "Gram postings:%.*s. Gram:%u.", LLC_VIEW_ARGS(path), gram);
	if_true_fef(postings.size() != found->Count * sizeof(::llc::u1_t), "Gram postings:%.*s. Bytes:%u. Count:%u.", LLC_VIEW_ARGS(path), postings.size(), found->Count);
	rtrn ::llc::view<cnst ::llc::u1_t>{(cnst ::llc::u1_t*)postings.begin(), found->Count}.for_each([&](cnst ::llc::u1_t & file) -> ::llc::err_t {
		if_true_fef(file >= header.Paths, "Gram:%u. File:%u. Paths:%u.", gram, file, header.Paths);
		rtrn matches.push_back(file);
	});
}

sttc ::llc::err_t fileIndexGramCandidate(::llc::vcst_t databasePath, ::llc::u3_t generation, ::llc::u2_t iShard, ::llc::u2_t firstGram, ::llc::u2_t lastGram, bool & possible) {
	possible = false;
	SGramDiskHeader header = {};
	if_fail_fef(::fileIndexGramEnsure(header, databasePath, generation, iShard), "Candidate shard:%u. First:%u. Last:%u.", iShard, firstGram, lastGram);
	::llc::au2_t first = {}, last = {};
	if_fail_fef(::fileIndexGramResolve(header, databasePath, firstGram, 4, first), "Candidate shard:%u. First:%u.", iShard, firstGram);
	if(!first.size()) rtrn 0;
	if(firstGram == lastGram) { possible = true; rtrn 0; }
	if_fail_fef(::fileIndexGramResolve(header, databasePath, lastGram, 4, last), "Candidate shard:%u. Last:%u.", iShard, lastGram);
	if(!last.size()) rtrn 0;
	if_fail_fe(first.for_each([&](cnst ::llc::u2_t & file) -> ::llc::err_t {
		if(!possible && ::llc::find(file, ::llc::vcu2_t{last.begin(), last.size()}) >= 0) possible = true;
		rtrn 0;
	}));
	rtrn 0;
}

sttc ::llc::err_t fileIndexResolveRaw(::llc::vcst_t shardPath, ::llc::vcst_t term, ::llc::u2_t expectedPaths, ::llc::au2_t & matches) {
	::llc::au0_t data = {}, inflated = {};
	if_fail_fef(::llc::fileToMemory(shardPath, data, SEARCH_INDEX_MAX_BYTES), "Shard:%.*s. Term:%.*s.", LLC_VIEW_ARGS(shardPath), LLC_VIEW_ARGS(term));
	::llc::vcu0_t input = data;
	::llc::u2_t magic = 0;
	if_fail_fe(::llc::loadPOD(input, magic));
	if(magic == SEARCH_DEFLATE_MAGIC) {
		::llc::u2_t rawBytes = 0;
		::llc::vcu0_t packed = {};
		if_fail_fe(::llc::loadPOD(input, rawBytes));
		if_fail_fe(::llc::loadView(input, packed));
		if_true_fef(!rawBytes || rawBytes > SEARCH_INDEX_MAX_BYTES || input.size(), "Shard:%.*s. Term:%.*s. Raw:%u. Tail:%u.", LLC_VIEW_ARGS(shardPath), LLC_VIEW_ARGS(term), rawBytes, input.size());
		if_fail_fef(::llc::arrayInflate(packed, inflated), "Shard:%.*s. Term:%.*s.", LLC_VIEW_ARGS(shardPath), LLC_VIEW_ARGS(term));
		if_true_fef(inflated.size() != rawBytes, "Shard:%.*s. Term:%.*s. Inflated:%u. Expected:%u.", LLC_VIEW_ARGS(shardPath), LLC_VIEW_ARGS(term), inflated.size(), rawBytes);
		input = inflated;
		if_fail_fe(::llc::loadPOD(input, magic));
	}
	if_true_fef(magic != SEARCH_INDEX_MAGIC, "Shard:%.*s. Term:%.*s. Magic:%u.", LLC_VIEW_ARGS(shardPath), LLC_VIEW_ARGS(term), magic);
	::llc::vcst_t root = {};
	::llc::u2_t pathCount = 0, wordCount = 0;
	if_fail_fe(::llc::loadView(input, root));
	if_fail_fe(::llc::loadPOD(input, pathCount));
	if_true_fef(!pathCount || pathCount > SEARCH_SHARD_MAX_FILES || pathCount > expectedPaths, "Shard:%.*s. Term:%.*s. Paths:%u. Remaining:%u.", LLC_VIEW_ARGS(shardPath), LLC_VIEW_ARGS(term), pathCount, expectedPaths);
	::llc::u2_t iPath = 0;
	while(iPath++ < pathCount) {
		::llc::vcst_t unused = {};
		if_fail_fef(::llc::loadView(input, unused), "Shard:%.*s. Term:%.*s. Path:%u.", LLC_VIEW_ARGS(shardPath), LLC_VIEW_ARGS(term), iPath - 1);
	}
	if_fail_fe(::llc::loadPOD(input, wordCount));
	if_true_fef(wordCount > 10000000 || wordCount > input.size(), "Shard:%.*s. Term:%.*s. Words:%u. Bytes:%u.", LLC_VIEW_ARGS(shardPath), LLC_VIEW_ARGS(term), wordCount, input.size());
	::llc::au0_t seen = {};
	if_fail_fe(seen.resize(pathCount));
	if_fail_fe(seen.fill(0));
	::llc::u2_t iWord = 0;
	while(iWord++ < wordCount) {
		::llc::vcst_t text = {};
		::llc::vcu2_t postings = {};
		if_fail_fef(::llc::loadView(input, text), "Shard:%.*s. Term:%.*s. Word:%u.", LLC_VIEW_ARGS(shardPath), LLC_VIEW_ARGS(term), iWord - 1);
		if_fail_fef(::llc::loadView(input, postings), "Shard:%.*s. Term:%.*s. Word:%u.", LLC_VIEW_ARGS(shardPath), LLC_VIEW_ARGS(term), iWord - 1);
		if(text.find(term) < 0) continue;
		if_fail_fe(postings.for_each([&](cnst ::llc::u2_t & local) -> ::llc::err_t {
			if_true_fef(local >= pathCount, "Shard:%.*s. Term:%.*s. Posting:%u. Paths:%u.", LLC_VIEW_ARGS(shardPath), LLC_VIEW_ARGS(term), local, pathCount);
			seen[local] = 1;
			rtrn 0;
		}));
	}
	if_true_fef(input.size(), "Shard:%.*s. Term:%.*s. Tail:%u.", LLC_VIEW_ARGS(shardPath), LLC_VIEW_ARGS(term), input.size());
	rtrn seen.enumerate([&](::llc::u2_t local, cnst ::llc::u0_t & found) -> ::llc::err_t {
		if(found) if_fail_fe(matches.push_back(local));
		rtrn 0;
	});
}

sttc ::llc::err_t fileIndexBuildSharded(SFileIndexWork & index, ::llc::vcst_t databasePath) {
	::llc::u3_t priorGeneration = 0;
	::llc::u2_t priorShards = 0;
	::llc::string partialPath = {};
	if_fail_fe(::llc::append_strings(partialPath, databasePath, ".partial"));
	if(::llc::fileSize(databasePath) > 0) {
		SFileIndexWork prior = {};
		if(!::llc::failed(::fileIndexLoad(prior, databasePath))) {
			priorGeneration = prior.ShardGeneration;
			priorShards = prior.ShardStarts.size();
		}
	}
	if(::llc::fileSize(partialPath) > 0) {
		SFileIndexWork partial = {};
		if_fail_fef(::fileIndexLoad(partial, partialPath), "Resume index:%.*s.", LLC_VIEW_ARGS(partialPath));
		if(partial.Root == index.Root && (partial.ShardGeneration != priorGeneration || partial.CompletedPaths < partial.Paths.size())) {
			cnst ::llc::err_t valid = partial.ShardStarts.enumerate([&](::llc::u2_t iShard, cnst ::llc::u2_t &) -> ::llc::err_t {
				::llc::string path = {};
				if_fail_fe(::fileIndexShardPath(databasePath, partial.ShardGeneration, iShard, path));
				if_true_fef(::llc::fileSize(path) <= 0, "Resume shard:%.*s.", LLC_VIEW_ARGS(path));
				rtrn 0;
			});
			if_fail_fef(valid, "Resume index:%.*s.", LLC_VIEW_ARGS(partialPath));
			index.Paths = ::std::move(partial.Paths);
			index.ShardStarts = ::std::move(partial.ShardStarts);
			index.ShardGeneration = partial.ShardGeneration;
			index.CompletedPaths = index.ResumedPaths = partial.CompletedPaths;
			index.Discovered.store(index.Paths.size(), ::std::memory_order_relaxed);
			index.Processed.store(index.CompletedPaths, ::std::memory_order_relaxed);
		}
		else {
			::llc::u2_t iShard = 0;
			while(partial.ShardGeneration != priorGeneration && iShard < partial.ShardStarts.size()) {
				::llc::string path = {};
				if(!::llc::failed(::fileIndexShardPath(databasePath, partial.ShardGeneration, iShard, path)))
					(void)::llc::fileDelete(path);
				::llc::string gramPath = {};
				if(!::llc::failed(::fileIndexGramPath(databasePath, partial.ShardGeneration, iShard++, gramPath)) && ::llc::fileSize(gramPath) > 0)
					(void)::llc::fileDelete(gramPath);
			}
			if(partial.ShardGeneration != priorGeneration) {
				::llc::string charsPath = {};
				if_fail_fe(::fileIndexCharsPath(databasePath, partial.ShardGeneration, charsPath));
				if(::llc::fileSize(charsPath) > 0)
					if_fail_fef(::llc::fileDelete(charsPath), "Discard route:%.*s.", LLC_VIEW_ARGS(charsPath));
			}
			if_fail_fef(::llc::fileDelete(partialPath), "Discard previous partial index:%.*s.", LLC_VIEW_ARGS(partialPath));
		}
	}
	if(!index.ResumedPaths)
		if_fail_fef(::fileIndexVisit(index, index.Root, true), "Index directory:%.*s.", LLC_VIEW_ARGS(index.Root));
	if(index.Paths.size() <= SEARCH_SHARD_MAX_FILES) {
		if_fail_fe(::fileIndexBuild(index));
		index.SaveResult = ::fileIndexSave(index, databasePath);
		if(!::llc::failed(index.SaveResult)) { index.RetireGeneration = priorGeneration; index.RetireShards = priorShards; }
		rtrn 0;
	}
	if(!index.ShardGeneration) index.ShardGeneration = (::llc::u3_t)::llc::timeCurrentInUs();
	if(index.ResumedPaths) {
		if_fail_fef(::fileIndexCharsEnsure(index, databasePath), "Resume route:%.*s.", LLC_VIEW_ARGS(databasePath));
	}
	else {
		if_fail_fe(index.Chars.resize(index.Paths.size() * 4));
		if_fail_fe(index.Chars.fill(0));
	}
	SFileShardRollback rollback = {databasePath, index.ShardGeneration};
	rollback.Saved = rollback.CheckpointSaved = index.ShardStarts.size();
	::llc::u2_t base = index.CompletedPaths;
	while(base < index.Paths.size()) {
		if_true_fef(index.Cancel.load(::std::memory_order_relaxed), "Index directory:%.*s. Path:%u.", LLC_VIEW_ARGS(index.Root), base);
		SFileIndexWork shard = {};
		shard.Root = index.Root;
		cnst ::llc::u2_t end = ::llc::min(index.Paths.size(), base + SEARCH_SHARD_MAX_FILES);
		if_fail_fe(shard.Paths.resize(end - base));
		if_fail_fe(shard.Paths.enumerate([&](::llc::u2_t i, ::llc::string & path) -> ::llc::err_t { path = index.Paths[base + i]; rtrn 0; }));
		cnst ::llc::u2_t savedBefore = rollback.Saved;
		cnst auto saveShard = [&](SFileIndexWork & part) -> ::llc::err_t {
			::llc::string shardPath = {};
			if_fail_fe(::fileIndexShardPath(databasePath, index.ShardGeneration, index.ShardStarts.size(), shardPath));
			if_fail_fef(::fileIndexSave(part, shardPath, true), "Index shard:%.*s. Start:%u.", LLC_VIEW_ARGS(shardPath), base);
			++rollback.Saved;
			if_fail_fef(::fileIndexCharsAccumulate(index, part, base), "Index route:%.*s. Start:%u.", LLC_VIEW_ARGS(databasePath), base);
			SGramRoute gramRoute = {};
			if_fail_fef(::fileIndexGramBuild(part, gramRoute), "Gram shard:%.*s. Start:%u.", LLC_VIEW_ARGS(databasePath), base);
			if_fail_fef(::fileIndexGramSave(gramRoute, databasePath, index.ShardGeneration, index.ShardStarts.size()), "Gram shard:%.*s. Start:%u.", LLC_VIEW_ARGS(databasePath), base);
			rtrn index.ShardStarts.push_back(base);
		};
		::llc::u2_t processed = 0;
		if_fail_fef(::fileIndexBuild(shard, &index, base, SEARCH_SHARD_MAX_WORDS, &processed, saveShard), "Shard start:%u.", base);
		if_true_fef(0 == processed, "Index directory:%.*s. Shard start:%u.", LLC_VIEW_ARGS(index.Root), base);
		if_fail_fe(shard.Paths.resize(processed));
		if(shard.Words.size() || rollback.Saved == savedBefore)
			if_fail_fef(saveShard(shard), "Index directory:%.*s. Start:%u.", LLC_VIEW_ARGS(index.Root), base);
		index.Unreadable += shard.Unreadable;
		index.Truncated += shard.Truncated;
		index.SkippedBinary += shard.SkippedBinary;
		base += processed;
		index.CompletedPaths = base;
		if_fail_fef(::fileIndexManifestSave(index, partialPath, true), "Checkpoint index:%.*s. Completed:%u.", LLC_VIEW_ARGS(partialPath), base);
		rollback.CheckpointSaved = rollback.Saved;
		if_fail_fef(::fileIndexCharsSave(index, databasePath), "Checkpoint route:%.*s. Completed:%u.", LLC_VIEW_ARGS(databasePath), base);
	}
	index.SaveResult = ::fileIndexManifestSave(index, databasePath);
	if_fail_fef(index.SaveResult, "Index manifest:%.*s. Paths:%u. Shards:%u.", LLC_VIEW_ARGS(databasePath), index.Paths.size(), index.ShardStarts.size());
	rollback.Committed = true;
	if(::llc::fileSize(partialPath) > 0)
		(void)::llc::fileDelete(partialPath);
	if(priorGeneration && priorGeneration != index.ShardGeneration) { index.RetireGeneration = priorGeneration; index.RetireShards = priorShards; }
	rtrn 0;
}

sttc ::llc::err_t fileIndexSearchSharded(SFileSearchWork & work) {
	::llc::string letter = work.Query;
	::llc::tolower(letter);
	if(letter.size() == 1 && letter[0] > ' ' && work.Chars && work.Chars->size() == work.TotalPaths * 4) {
		stct SFileCharMask { ::llc::u3_t Bits[4]; };
		::llc::view<cnst SFileCharMask> masks = {(cnst SFileCharMask*)work.Chars->begin(), work.TotalPaths};
		cnst ::llc::u0_t ch = (::llc::u0_t)letter[0];
		::std::lock_guard<::std::mutex> guard(work.ResultsMutex);
		if_fail_fe(masks.enumerate([&](::llc::u2_t iPath, cnst SFileCharMask & mask) -> ::llc::err_t {
			if(mask.Bits[ch / 64] & ((::llc::u3_t)1 << (ch % 64)))
				if_fail_fe(work.Results.push_back(iPath));
			rtrn 0;
		}));
		work.ShardsDone.store(work.ShardStarts.size(), ::std::memory_order_release);
		rtrn 0;
	}
	bool shortTerm = letter.size() >= 2 && letter.size() <= 4;
	bool longTerm = letter.size() > 4;
	::llc::u2_t gram = 0;
	::llc::u2_t firstGram = 0;
	if_fail_fe(letter.enumerate([&](::llc::u2_t i, cnst char & character) -> ::llc::err_t {
		cnst ::llc::u0_t ch = (::llc::u0_t)character;
		shortTerm = shortTerm && ch > ' ' && ch < 0x80;
		longTerm = longTerm && ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9'));
		if(i < 4) firstGram = (firstGram << 8) | ch;
		gram = (gram << 8) | ch;
		rtrn 0;
	}));
	if(shortTerm && work.UseGramRoute) {
		::llc::au0_t seen = {};
		if_fail_fe(seen.resize(work.TotalPaths));
		if_fail_fe(seen.fill(0));
		::llc::u2_t iShard = 0;
		rtrn work.ShardStarts.for_each([&](cnst ::llc::u2_t & base) -> ::llc::err_t {
			if_true_fef(work.Cancel.load(::std::memory_order_relaxed), "Search:%.*s. Shard:%u.", LLC_VIEW_ARGS(work.Query), iShard);
			SGramDiskHeader header = {};
			if_fail_fef(::fileIndexGramEnsure(header, work.DatabasePath, work.ShardGeneration, iShard), "Search:%.*s. Shard:%u.", LLC_VIEW_ARGS(work.Query), iShard);
			if_true_fef(base >= work.TotalPaths || header.Paths > work.TotalPaths - base, "Search:%.*s. Base:%u. Paths:%u. Total:%u.", LLC_VIEW_ARGS(work.Query), base, header.Paths, work.TotalPaths);
			::llc::au2_t matches = {};
			if_fail_fef(::fileIndexGramResolve(header, work.DatabasePath, gram, letter.size(), matches), "Search:%.*s. Shard:%u.", LLC_VIEW_ARGS(work.Query), iShard);
			{
				::std::lock_guard<::std::mutex> guard(work.ResultsMutex);
				if_fail_fe(matches.for_each([&](cnst ::llc::u2_t & local) -> ::llc::err_t {
					cnst ::llc::u2_t global = base + local;
					if(seen[global]) rtrn 0;
					seen[global] = 1;
					rtrn work.Results.push_back(global);
				}));
			}
			work.ShardsDone.store(++iShard, ::std::memory_order_release);
			rtrn 0;
		});
	}
	::llc::aobj<::llc::vcst_t> terms = {};
	if_fail_fef(::llc::split(::llc::vcst_t{work.Query.begin(), work.Query.size()}, ' ', terms), "Search:%.*s.", LLC_VIEW_ARGS(work.Query));
	::llc::u2_t termCount = 0;
	if_fail_fe(terms.for_each([&](cnst ::llc::vcst_t & term) -> ::llc::err_t { termCount += term.size() ? 1 : 0; rtrn 0; }));
	if(!termCount) rtrn 0;
	if_fail_fe(work.TermSeen.resize(termCount));
	if_fail_fe(work.TermSeen.for_each([&](::llc::au0_t & seen) -> ::llc::err_t {
		if_fail_fe(seen.resize(work.TotalPaths));
		rtrn seen.fill(0);
	}));
	if_fail_fe(work.MatchedTerms.resize(work.TotalPaths));
	if_fail_fe(work.MatchedTerms.fill(0));
	::llc::u2_t iShard = 0;
	rtrn work.ShardStarts.for_each([&](cnst ::llc::u2_t & base) -> ::llc::err_t {
		if_true_fef(work.Cancel.load(::std::memory_order_relaxed), "Search:%.*s. Shard:%u.", LLC_VIEW_ARGS(work.Query), iShard);
		if(longTerm && work.UseGramRoute) {
			bool possible = false;
			if_fail_fef(::fileIndexGramCandidate(work.DatabasePath, work.ShardGeneration, iShard, firstGram, gram, possible), "Search:%.*s. Shard:%u.", LLC_VIEW_ARGS(work.Query), iShard);
			if(!possible) { work.ShardsDone.store(++iShard, ::std::memory_order_release); rtrn 0; }
		}
		::llc::string shardPath = {};
		if_fail_fe(::fileIndexShardPath(work.DatabasePath, work.ShardGeneration, iShard, shardPath));
		if(longTerm && work.UseGramRoute) {
			::llc::au2_t matches = {};
			if_true_fef(base >= work.TotalPaths, "Search:%.*s. Shard:%u. Base:%u. Total:%u.", LLC_VIEW_ARGS(work.Query), iShard, base, work.TotalPaths);
			if_fail_fef(::fileIndexResolveRaw(shardPath, letter, work.TotalPaths - base, matches), "Search:%.*s. Shard:%u.", LLC_VIEW_ARGS(work.Query), iShard);
			{
				::std::lock_guard<::std::mutex> guard(work.ResultsMutex);
				if_fail_fe(matches.for_each([&](cnst ::llc::u2_t & local) -> ::llc::err_t {
					cnst ::llc::u2_t global = base + local;
					if(work.TermSeen[0][global]) rtrn 0;
					work.TermSeen[0][global] = 1;
					work.MatchedTerms[global] = 1;
					rtrn work.Results.push_back(global);
				}));
			}
			work.ShardsDone.store(++iShard, ::std::memory_order_release);
			rtrn 0;
		}
		SFileIndexWork shard = {};
		if_fail_fef(::fileIndexLoad(shard, shardPath, false), "Search:%.*s. Shard:%.*s.", LLC_VIEW_ARGS(work.Query), LLC_VIEW_ARGS(shardPath));
		if_true_fef(base >= work.TotalPaths || !shard.StoredPaths || shard.StoredPaths > work.TotalPaths - base, "Shard:%.*s. Start:%u. Paths:%u. Total:%u.", LLC_VIEW_ARGS(shardPath), base, shard.StoredPaths, work.TotalPaths);
		::llc::u2_t iTerm = 0;
		cnst ::llc::err_t resolved = terms.for_each([&](cnst ::llc::vcst_t & term) -> ::llc::err_t {
			if(!term.size()) rtrn 0;
			::llc::au2_t matches = {};
			if_fail_fef(::llc::wordDictResolve(shard.Words, term, shard.StoredPaths, matches, {}), "Search:%.*s. Term:%.*s. Shard:%u.", LLC_VIEW_ARGS(work.Query), LLC_VIEW_ARGS(term), iShard);
			{
				::std::lock_guard<::std::mutex> guard(work.ResultsMutex);
				cnst ::llc::err_t appended = matches.for_each([&](cnst ::llc::u2_t & local) -> ::llc::err_t {
					cnst ::llc::u2_t global = base + local;
					if(work.TermSeen[iTerm][global]) rtrn 0;
					work.TermSeen[iTerm][global] = 1;
					if(++work.MatchedTerms[global] == termCount)
						if_fail_fe(work.Results.push_back(global));
					rtrn 0;
				});
				if_fail_fef(appended, "Search:%.*s. Term:%.*s. Shard:%u.", LLC_VIEW_ARGS(work.Query), LLC_VIEW_ARGS(term), iShard);
			}
			++iTerm;
			rtrn 0;
		});
		if_fail_fef(resolved, "Search:%.*s. Shard:%u.", LLC_VIEW_ARGS(work.Query), iShard);
		work.ShardsDone.store(++iShard, ::std::memory_order_release);
		rtrn 0;
	});
}

::llc::err_t sampleGUIViewportSetup(SSampleGUIViewport & app) {
	app.GUI.SelectedFont = 7;
	app.GUI.ColorModeDefault = ::llc::GUI_COLOR_MODE_3D;
	if_fail_fe(::sampleGUIResourcesCreate(app.GUI));
	app.Desktop = ::llc::controlCreate(app.GUI);
	if_true_fe(app.Desktop < 0);
	app.GUI.Controls.Draw[app.Desktop].NoBorder = true;
	app.GUI.Controls.Draw[app.Desktop].NoClient = true;
	app.GUI.Controls.Constraints[app.Desktop].AttachSizeToControl = {app.Desktop, app.Desktop};
	::sampleGUIControlThemeApply(app.GUI, app.Desktop, 2);
	app.Help = ::llc::controlCreateChild(app.GUI, app.Desktop);
	if_true_fe(app.Help < 0);
	app.GUI.Controls.Placement[app.Help].Area = {{18, 18}, {465, 42}};
	app.GUI.Controls.Placement[app.Help].Border = {2, 2, 2, 2};
	app.GUI.Controls.Placement[app.Help].Margin = {1, 1, 1, 1};
	app.GUI.Controls.Text[app.Help].Text = "BLITTER SEARCH | SELECT FILE, THEN O TO OPEN";
	app.GUI.Controls.Text[app.Help].Align = ::llc::ALIGN_CENTER_LEFT;
	app.GUI.Controls.Text[app.Help].FontSelected = 7;
	::sampleGUIControlThemeApply(app.GUI, app.Help, 2);
	if_fail_fe(::llc::inputBoxCreate(app.SearchInput, app.GUI, app.Desktop));
	app.SearchInput.MaxLength = 512;
	app.SearchInput.Edit(app.GUI, true);
	app.GUI.Controls.SetHidden(app.SearchInput.VirtualKeyboard.IdRoot, true);
	app.GUI.Controls.Placement[app.SearchInput.IdRoot].Area = {{535, 18}, {565, 42}};
	app.GUI.Controls.Placement[app.SearchInput.IdText].Area.Size = {555, 35};
	app.GUI.Controls.Text[app.SearchInput.IdText].Align = ::llc::ALIGN_CENTER_LEFT;
	app.GUI.Controls.Text[app.SearchInput.IdText].FontSelected = 7;
	::sampleGUIControlThemeApply(app.GUI, app.SearchInput.IdText, 2);
	app.SearchTiming = ::llc::controlCreateChild(app.GUI, app.Desktop);
	if_true_fe(app.SearchTiming < 0);
	app.GUI.Controls.Placement[app.SearchTiming].Area = {{535, 65}, {565, 22}};
	app.GUI.Controls.Text[app.SearchTiming].Text = "File matches 0 | query -- us";
	app.GUI.Controls.Text[app.SearchTiming].FontSelected = 7;
	::sampleGUIControlThemeApply(app.GUI, app.SearchTiming, 2);
	app.IndexDirectory = ::llc::controlCreateChild(app.GUI, app.Desktop);
	if_true_fe(app.IndexDirectory < 0);
	app.GUI.Controls.Placement[app.IndexDirectory].Area = {{1120, 18}, {220, 42}};
	app.GUI.Controls.Text[app.IndexDirectory].Text = "INDEX DIRECTORY";
	app.GUI.Controls.Text[app.IndexDirectory].FontSelected = 7;
	::sampleGUIControlThemeApply(app.GUI, app.IndexDirectory, 0);
	if_fail_fe(::sampleGUIViewportCreate(app, 0, "DIRECTORY", {{18, 100}, {260, 570}}));
	if_fail_fe(::sampleGUIViewportCreate(app, 1, "FILE CONTENT", {{290, 100}, {420, 300}}));
	if_fail_fe(::sampleGUIViewportCreate(app, 2, "FILE PREVIEW", {{290, 412}, {420, 258}}));
	if_fail_fe(::sampleGUIViewportCreate(app, 3, "MATCHING FILES", {{722, 100}, {410, 570}}));
	if_fail_fe(::sampleGUIViewportCreate(app, 4, "PROCESSES", {{1144, 100}, {430, 275}}));
	if_fail_fe(::sampleGUIViewportCreate(app, 5, "WINDOWS SERVICES", {{1144, 387}, {430, 283}}));
	if_fail_fe(::sampleGUIViewportCreate(app, 6, "CONSOLE", {{18, 682}, {1556, 160}}));
	app.OpenSelected = ::llc::controlCreateChild(app.GUI, app.Viewports[0].IdControl);
	if_true_fe(app.OpenSelected < 0);
	app.GUI.Controls.Placement[app.OpenSelected].Area = {{214, 4}, {22, 20}};
	app.GUI.Controls.Text[app.OpenSelected].Text = "O";
	app.GUI.Controls.Text[app.OpenSelected].FontSelected = 7;
	::sampleGUIControlThemeApply(app.GUI, app.OpenSelected, 0);
	app.DriveD = ::llc::controlCreateChild(app.GUI, app.Desktop);
	if_true_fe(app.DriveD < 0);
	app.GUI.Controls.Placement[app.DriveD].Area = {{1360, 18}, {120, 42}};
#if defined(_WIN32)
	app.GUI.Controls.Text[app.DriveD].Text = "D: DRIVE";
#else
	app.GUI.Controls.Text[app.DriveD].Text = "ROOT";
#endif
	app.GUI.Controls.Text[app.DriveD].FontSelected = 7;
	::sampleGUIControlThemeApply(app.GUI, app.DriveD, 0);
	if_fail_fe(::desktopListCreate(app, 0, app.FolderControls));
	if_fail_fe(::desktopListCreate(app, 1, app.FileControls));
	if_fail_fe(::inventoryListCreate(app, 2, app.PreviewControls));
	if_fail_fe(::desktopListCreate(app, 3, app.FileResultControls));
	if_fail_fe(app.FileResultControls.for_each([&app](cnst ::llc::cid_t & id) -> ::llc::err_t { app.GUI.Controls.SetHidden(id, true); rtrn 0; }));
	if_fail_fe(::inventoryListCreate(app, 4, app.ProcessControls));
	if_fail_fe(::inventoryListCreate(app, 5, app.ServiceControls));
	{
		::llc::vcst_t labels[4] = {};
		::llc::SControlPlacement row = {};
		row.Area.Size = {0, 21};
		row.Margin = {3, 1, 3, 1};
		cnst ::llc::cid_t target = app.Viewports[6].IdControls[::llc::VIEWPORT_CONTROL_TARGET];
		if_fail_fe(::llc::guiCreateControlList(app.GUI, ::llc::DIRECTION_DOWN, {labels, ::llc::size(labels)}, target, row, ::llc::ALIGN_CENTER_LEFT, app.ConsoleControls));
		if_fail_fe(app.ConsoleControls.for_each([&app](cnst ::llc::cid_t & id) -> ::llc::err_t { app.GUI.Controls.Text[id].FontSelected = 7; rtrn 0; }));
		if_fail_fe(::llc::inputBoxCreate(app.ConsoleInput, app.GUI, target));
		if_fail_fe(app.GUI.Controls.Draw.for_each([](::llc::SControlDraw & draw) -> ::llc::err_t { draw.UseNewPalettes = true; rtrn 0; }));
		app.ConsoleInput.MaxLength = 4096;
		app.ConsoleInput.Edit(app.GUI, true);
		app.GUI.Controls.SetHidden(app.ConsoleInput.VirtualKeyboard.IdRoot, true);
		app.GUI.Controls.Placement[app.ConsoleInput.IdRoot].Area = {{5, 91}, {1532, 26}};
		app.GUI.Controls.Placement[app.ConsoleInput.IdText].Area.Size = {1526, 22};
		app.GUI.Controls.Text[app.ConsoleInput.IdText].Align = ::llc::ALIGN_CENTER_LEFT;
		app.GUI.Controls.Text[app.ConsoleInput.IdText].FontSelected = 7;
		::sampleGUIControlThemeApply(app.GUI, app.ConsoleInput.IdText, 2);
	}
	if_fail_fe(::inventorySnapshotRefresh(app));
	if_fail_fe(::previewRowsRefresh(app));
	if_fail_fe(::consoleWrite(app, "Console ready. Commands: cd PATH, help, pwd, clear."));
	::llc::string initialFolder = {};
	if_fail_fe(::llc::pathAbsolute(".", initialFolder));
	if_fail_fe(::desktopFolderOpen(app, initialFolder));
#if defined(_WIN32)
	char modulePath[MAX_PATH] = {};
	cnst DWORD moduleLength = ::GetModuleFileNameA(nullptr, modulePath, ::llc::size(modulePath));
	if_zero_fef(moduleLength && moduleLength < ::llc::size(modulePath), "GetModuleFileName error:%u. Length:%u.", ::GetLastError(), moduleLength);
	::llc::vcst_t moduleDirectory = {};
	if_fail_fe(::llc::pathDirectory({modulePath, moduleLength}, moduleDirectory));
	if(!app.IndexDatabasePath.size())
		if_fail_fe(::llc::pathNameCompose(moduleDirectory, "llc_server.index", app.IndexDatabasePath));
#else
	if(!app.IndexDatabasePath.size())
		if_fail_fe(::llc::pathAbsolute("llc_server.index", app.IndexDatabasePath));
#endif
	if_fail_fe(::sampleGUIViewportActivate(app, 0));
	app.Performance = ::llc::controlCreate(app.GUI);
	if_true_fe(app.Performance < 0);
	app.GUI.Controls.Placement[app.Performance].Area = {{0, 14}, {360, 36}};
	app.GUI.Controls.Placement[app.Performance].Align = ::llc::ALIGN_CENTER_BOTTOM;
	app.GUI.Controls.Placement[app.Performance].Border = {1, 1, 1, 1};
	app.GUI.Controls.Placement[app.Performance].Margin = {1, 1, 1, 1};
	app.GUI.Controls.Text[app.Performance].Text = "FPS --    Frame -- ms";
	app.GUI.Controls.Text[app.Performance].FontSelected = 7;
	::sampleGUIControlThemeApply(app.GUI, app.Performance, 2);
	app.Exit = ::llc::controlCreate(app.GUI);
	if_true_fe(app.Exit < 0);
	app.GUI.Controls.Placement[app.Exit].Area = {{18, 18}, {96, 36}};
	app.GUI.Controls.Placement[app.Exit].Align = ::llc::ALIGN_BOTTOM_RIGHT;
	app.GUI.Controls.Placement[app.Exit].Border = {2, 2, 2, 2};
	app.GUI.Controls.Placement[app.Exit].Margin = {1, 1, 1, 1};
	app.GUI.Controls.Text[app.Exit].Text = "EXIT";
	app.GUI.Controls.Text[app.Exit].FontSelected = 7;
	::sampleGUIControlThemeApply(app.GUI, app.Exit, 1);
	if_fail_fe(::llc::guiUpdateMetrics(app.GUI, app.Window.Size, false));
	if_fail_fe(::desktopFolderRowsRefresh(app));
	if_fail_fe(::inventoryRowsRefresh(app, app.ProcessControls, app.ProcessNames, app.ProcessWords, app.ProcessOffset));
	if_fail_fe(::inventoryRowsRefresh(app, app.ServiceControls, app.ServiceNames, app.ServiceWords, app.ServiceOffset));
	::llc::string savedPath = app.IndexDatabasePath;
	bool partial = false;
	if(::llc::fileSize(savedPath) <= 0) {
		if_fail_fe(::llc::append_strings(savedPath, ".partial"));
		partial = ::llc::fileSize(savedPath) > 0;
	}
	if(::llc::fileSize(savedPath) > 0) {
		SFileIndexWork saved = {};
		cnst ::llc::err_t loaded = ::fileIndexLoad(saved, savedPath);
		if(::llc::failed(loaded)) {
			if_fail_fe(::consoleWrite(app, "Saved index invalid; index the directory again."));
		}
		else {
			cnst ::llc::err_t shardsValid = saved.ShardStarts.enumerate([&](::llc::u2_t iShard, cnst ::llc::u2_t &) -> ::llc::err_t {
				::llc::string path = {};
				if_fail_fe(::fileIndexShardPath(app.IndexDatabasePath, saved.ShardGeneration, iShard, path));
				if_true_fef(::llc::fileSize(path) <= 0, "Index shard:%.*s.", LLC_VIEW_ARGS(path));
				rtrn 0;
			});
			if_fail_fef(shardsValid, "Index:%.*s.", LLC_VIEW_ARGS(savedPath));
			if_fail_fef(::fileIndexCharsEnsure(saved, app.IndexDatabasePath), "Saved route:%.*s.", LLC_VIEW_ARGS(app.IndexDatabasePath));
			app.IndexRoot = saved.Root;
			app.IndexPaths = ::std::move(saved.Paths);
			app.IndexWords = ::std::move(saved.Words);
			app.IndexShardStarts = ::std::move(saved.ShardStarts);
			app.IndexShardGeneration = saved.ShardGeneration;
			app.IndexChars = ::std::make_shared<::llc::au3_t>(::std::move(saved.Chars));
			app.IndexReady = true;
			app.IndexPartial = partial;
			if(partial) {
				::llc::astsc_t<96> message = {};
				if_fail_fe(::llc::sprintf_s(message.Storage, "Partial index: %u of %u files. Resume with INDEX DIRECTORY.", saved.CompletedPaths, app.IndexPaths.size()));
				if_fail_fe(::consoleWrite(app, message.Storage));
			}
			if_fail_fe(::desktopFolderOpen(app, app.IndexRoot));
			if_fail_fe(::fileResultsResolve(app));
			if(!partial)
				if_fail_fe(::consoleWrite(app, "Saved directory index loaded."));
		}
	}
	rtrn ::consoleRowsRefresh(app);
}

::llc::err_t sampleGUIViewportUpdate(SSampleGUIViewport & app, bool systemRequestedExit, double secondsLastFrame) {
	if(systemRequestedExit || app.Window.Closed || app.Window.Input->KeyDown(::llc::SYSTEM_KEY_Escape))
		rtrn ::llc::APPLICATION_STATE_EXIT;
	if(app.GUI.Controls.IsAction(app.Exit))
		rtrn ::llc::APPLICATION_STATE_EXIT;
	if_fail_fe(::sampleGUIViewportPerformanceUpdate(app, secondsLastFrame));
	if(app.PendingConsole && app.PendingConsole->Status.load(::std::memory_order_acquire)) {
		::std::shared_ptr<SConsoleWork> completed = app.PendingConsole;
		app.PendingConsole.reset();
		if_fail_fe(::consoleOutputAppend(app, completed->Job.Response));
		if_fail_fe(::consoleOutputAppend(app, completed->Job.ErrorOutput));
		if(completed->Status.load(::std::memory_order_relaxed) < 0)
			if_fail_fe(::consoleWrite(app, "Process launch failed."));
	}
	if(app.PendingIndex && app.PendingIndex->Status.load(::std::memory_order_acquire)) {
		::std::shared_ptr<SFileIndexWork> completed = app.PendingIndex;
		app.PendingIndex.reset();
		app.GUI.Controls.Text[app.IndexDirectory].Text = "INDEX DIRECTORY";
		if(completed->Status.load(::std::memory_order_relaxed) < 0) {
			cnst bool cancelled = completed->Cancel.load(::std::memory_order_relaxed);
			if_fail_fe(::consoleWrite(app, cancelled ? ::llc::vcst_t{"Directory indexing cancelled; previous results preserved."} : ::llc::vcst_t{"Directory index failed; previous results preserved."}));
			app.GUI.Controls.Text[app.SearchTiming].Text = cancelled ? ::llc::vcst_t{"Index cancelled"} : ::llc::vcst_t{"Index failed"};
		}
		else {
			if(app.PendingSearch) app.PendingSearch->Cancel.store(true, ::std::memory_order_relaxed);
			if_fail_fe(app.FileResults.clear());
			if_fail_fe(app.FileResultControls.for_each([&app](cnst ::llc::cid_t & id) -> ::llc::err_t { rtrn ::llc::controlTextSet(app.GUI, id, {}); }));
			app.IndexWords = ::std::move(completed->Words);
			app.IndexPaths = ::std::move(completed->Paths);
			app.IndexShardStarts = ::std::move(completed->ShardStarts);
			app.IndexShardGeneration = completed->ShardGeneration;
			app.IndexChars = ::std::make_shared<::llc::au3_t>(::std::move(completed->Chars));
			app.RetireIndexGeneration = completed->RetireGeneration;
			app.RetireIndexShards = completed->RetireShards;
			app.IndexRoot = completed->Root;
			app.IndexUnreadable = completed->Unreadable;
			app.IndexTruncated = completed->Truncated;
			app.IndexSkippedFolders = completed->SkippedFolders;
			app.IndexSkippedBinary = completed->SkippedBinary;
			app.IndexReady = true;
			app.IndexPartial = false;
			if_fail_fe(::fileResultsResolve(app));
			::llc::astsc_t<96> message = {};
			if(completed->ResumedPaths) {
				if_fail_fe(::llc::sprintf_s(message.Storage, "Resumed index after %u files.", completed->ResumedPaths));
				if_fail_fe(::consoleWrite(app, message.Storage));
			}
			if_fail_fe(::llc::sprintf_s(message.Storage, "Files %u indexed in %lld ms.", app.IndexPaths.size(), completed->ElapsedMs));
			if_fail_fe(::consoleWrite(app, message.Storage));
			if_fail_fe(::llc::sprintf_s(message.Storage, "Unreadable %u, binary %u, truncated %u, folders skipped %u.", app.IndexUnreadable, app.IndexSkippedBinary, app.IndexTruncated, app.IndexSkippedFolders));
			if_fail_fe(::consoleWrite(app, message.Storage));
			if(::llc::failed(completed->SaveResult)) {
				if_fail_fe(::consoleWrite(app, "Could not save index; current search still works."));
			}
			else {
				if_fail_fe(::consoleWrite(app, "Index saved for the next launch."));
			}
		}
	}
	if(app.PendingSearch) {
		::std::shared_ptr<SFileSearchWork> search = app.PendingSearch;
		cnst ::llc::u2_t done = search->ShardsDone.load(::std::memory_order_acquire);
		if(!search->Cancel.load(::std::memory_order_relaxed) && (done != app.SearchShardsShown || search->Status.load(::std::memory_order_acquire))) {
			{
				::std::lock_guard<::std::mutex> guard(search->ResultsMutex);
				if_true_fef(search->Results.size() < app.SearchResultsShown, "Search:%.*s. Results:%u. Shown:%u.", LLC_VIEW_ARGS(search->Query), search->Results.size(), app.SearchResultsShown);
				if(search->Results.size() > app.SearchResultsShown)
					if_fail_fe(app.FileResults.append(::llc::vcu2_t{search->Results.begin() + app.SearchResultsShown, search->Results.size() - app.SearchResultsShown}));
				app.SearchResultsShown = search->Results.size();
			}
			app.SearchShardsShown = done;
			if_fail_fe(::fileResultsRefresh(app));
			cnst ::llc::err_t length = ::llc::sprintf_s(app.SearchTimingText.Storage, "File matches %u | %u/%u shards | %lld ms%s", app.FileResults.size(), done, search->ShardStarts.size(), (::llc::timeCurrentInUs() - search->StartedUs) / 1000, app.IndexPartial ? " | PARTIAL INDEX" : "");
			if_fail_fe(length);
			app.GUI.Controls.Text[app.SearchTiming].Text = {app.SearchTimingText.Storage, (::llc::u2_t)length};
		}
		if(search->Status.load(::std::memory_order_acquire)) {
			if(search->Status.load(::std::memory_order_relaxed) < 0 && !search->Cancel.load(::std::memory_order_relaxed))
				if_fail_fe(::consoleWrite(app, "Saved index search failed; reindex the directory."));
			app.PendingSearch.reset();
		}
	}
	if(!app.PendingSearch && app.RetireIndexShards) {
		::llc::string databasePath = app.IndexDatabasePath;
		cnst ::llc::u3_t generation = app.RetireIndexGeneration;
		cnst ::llc::u2_t count = app.RetireIndexShards;
		app.RetireIndexShards = 0;
		::std::thread([databasePath, generation, count] { (void)::fileIndexRetire(databasePath, generation, count); }).detach();
	}
	if(!app.PendingSearch && app.SearchQueued.size() && app.IndexShardStarts.size()) {
		app.PendingSearch = ::std::make_shared<SFileSearchWork>();
		app.PendingSearch->Query = app.SearchQueued;
		app.PendingSearch->DatabasePath = app.IndexDatabasePath;
		app.PendingSearch->ShardStarts = app.IndexShardStarts;
		app.PendingSearch->ShardGeneration = app.IndexShardGeneration;
		app.PendingSearch->Chars = app.IndexChars;
		app.PendingSearch->TotalPaths = app.IndexPaths.size();
		app.PendingSearch->StartedUs = ::llc::timeCurrentInUs();
		app.SearchShardsShown = 0;
		app.SearchResultsShown = 0;
		if_fail_fe(app.SearchQueued.clear());
		::std::shared_ptr<SFileSearchWork> search = app.PendingSearch;
		::std::thread([search] {
			cnst ::llc::err_t result = ::fileIndexSearchSharded(*search);
			search->Status.store(::llc::failed(result) ? -1 : 1, ::std::memory_order_release);
		}).detach();
	}
	if(app.PendingIndex && 0 == (app.InventoryFrame % 30)) {
		cnst ::llc::err_t length = ::llc::sprintf_s(app.IndexProgressText.Storage, "Indexing: %u found, %u read, %u words, %llu MiB", app.PendingIndex->Discovered.load(::std::memory_order_relaxed), app.PendingIndex->Processed.load(::std::memory_order_relaxed), app.PendingIndex->IndexedWords.load(::std::memory_order_relaxed), app.PendingIndex->BytesRead.load(::std::memory_order_relaxed) / (1024 * 1024));
		if_fail_fe(length);
		app.GUI.Controls.Text[app.SearchTiming].Text = {app.IndexProgressText.Storage, (::llc::u2_t)length};
	}
	if(0 == (++app.InventoryFrame % 120))
		if_fail_fe(::inventorySnapshotRefresh(app));
	if_fail_fe(app.ProcessableControls.clear());
	if_fail_fe(::llc::guiGetProcessableControls(app.GUI, app.ProcessableControls));
	if(app.GUI.Controls.IsAction(app.SearchInput.IdText)) app.SearchFocused = true;
	if(app.GUI.Controls.IsAction(app.ConsoleInput.IdText)) app.SearchFocused = false;
	::llc::string searchBefore = app.SearchInput.Text;
	::llc::view<cnst ::llc::pobj<::llc::SEventSystem>> inputEvents = app.Window.EventQueue;
	::llc::TQueueSystemEvent filteredEvents = {};
	cnst ::llc::SInput & input = *app.Window.Input;
	cnst bool chordCurrent = input.KeyboardCurrent.KeyState[::llc::SYSTEM_KEY_Control] && input.KeyboardCurrent.KeyState[::llc::SYSTEM_KEY_Shift] && input.KeyboardCurrent.KeyState['S'];
	cnst bool chordPrevious = input.KeyboardPrevious.KeyState[::llc::SYSTEM_KEY_Control] && input.KeyboardPrevious.KeyState[::llc::SYSTEM_KEY_Shift] && input.KeyboardPrevious.KeyState['S'];
	if(chordCurrent || chordPrevious) {
		::llc::view<::llc::pobj<::llc::SEventSystem>> frameEvents = app.Window.EventQueue;
		if_fail_fe(frameEvents.for_each([&filteredEvents](cnst ::llc::pobj<::llc::SEventSystem> & event) -> ::llc::err_t {
			rtrn event && event->Type == ::llc::SYSTEM_EVENT_Text ? 0 : filteredEvents.push_back(event);
		}));
		inputEvents = filteredEvents;
	}
	cnst ::llc::err_t consoleInput = app.SearchFocused
		? app.SearchInput.Update(app.GUI, app.SearchInput.VirtualKeyboard, inputEvents, app.ProcessableControls)
		: app.ConsoleInput.Update(app.GUI, app.ConsoleInput.VirtualKeyboard, inputEvents, app.ProcessableControls);
	if_fail_fe(consoleInput);
	if(searchBefore != app.SearchInput.Text) {
		app.ProcessOffset = app.ServiceOffset = 0;
		if_fail_fe(::inventoryRowsRefresh(app, app.ProcessControls, app.ProcessNames, app.ProcessWords, 0));
		if_fail_fe(::inventoryRowsRefresh(app, app.ServiceControls, app.ServiceNames, app.ServiceWords, 0));
		if_fail_fe(::fileResultsResolve(app));
	}
	if(app.GUI.Controls.IsAction(app.DriveD)) {
#if defined(_WIN32)
		cnst ::llc::vcst_t drive = "D:/";
#else
		cnst ::llc::vcst_t drive = "/";
#endif
		if_fail_fef(::desktopFolderOpen(app, drive), "Drive:%.*s.", LLC_VIEW_ARGS(drive));
		if_fail_fe(::consoleWrite(app, app.FolderPath));
	}
	if(!app.PendingIndex && app.GUI.Controls.IsAction(app.IndexDirectory)) {
		if_fail_fe(::consoleWrite(app, "Indexing selected directory..."));
		app.PendingIndex = ::std::make_shared<SFileIndexWork>();
		app.PendingIndex->Root = app.FolderPath;
		::std::shared_ptr<SFileIndexWork> work = app.PendingIndex;
		app.GUI.Controls.Text[app.IndexDirectory].Text = "CANCEL INDEX";
		::llc::string databasePath = app.IndexDatabasePath;
		::std::thread([work, databasePath] {
			cnst ::llc::s3_t started = ::llc::timeCurrentInMs();
			cnst ::llc::err_t result = ::fileIndexBuildSharded(*work, databasePath);
			work->ElapsedMs = ::llc::timeCurrentInMs() - started;
			work->Status.store(::llc::failed(result) ? -1 : 1, ::std::memory_order_release);
		}).detach();
		if_fail_fe(::consoleRowsRefresh(app));
	}
	else if(app.PendingIndex && app.GUI.Controls.IsAction(app.IndexDirectory) && !app.PendingIndex->Cancel.load(::std::memory_order_relaxed)) {
		app.PendingIndex->Cancel.store(true, ::std::memory_order_relaxed);
		app.GUI.Controls.Text[app.IndexDirectory].Text = "CANCELLING";
		if_fail_fe(::consoleWrite(app, "Cancelling directory index..."));
	}
	if(!app.SearchFocused && consoleInput == INT_MAX && app.ConsoleInput.Text.size()) {
		::llc::string command = app.ConsoleInput.Text;
		if(command == "clear") {
			if_fail_fe(app.ConsoleLines.clear());
		}
		else if(command == "pwd") {
			if_fail_fe(::consoleWrite(app, app.FolderPath));
		}
		else if(command == "help") {
#if defined(_WIN32)
			if_fail_fe(::consoleWrite(app, "cd D:\\ or cd PATH changes the directory. Search filters processes and services."));
#else
			if_fail_fe(::consoleWrite(app, "cd / or cd PATH changes the directory. Search filters processes and services."));
#endif
		}
		else if(command.size() > 3 && ::llc::vcst_t{command.begin(), 3} == "cd ") {
			::llc::string path = {};
			cnst ::llc::err_t pathResult = ::llc::pathAbsolute({command.begin() + 3, command.size() - 3}, path);
			if(::llc::failed(pathResult)) {
				if_fail_fe(::consoleWrite(app, "Invalid directory path."));
			}
#if defined(_WIN32)
			else {
			cnst DWORD attributes = ::GetFileAttributesA(path.begin());
			if(attributes == INVALID_FILE_ATTRIBUTES || 0 == (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
				if_fail_fe(::consoleWrite(app, "Directory does not exist."));
			}
			else {
				cnst ::llc::err_t openResult = ::desktopFolderOpen(app, path);
				if(::llc::failed(openResult)) {
					if_fail_fe(::consoleWrite(app, "Cannot open directory."));
				}
				else {
					if_fail_fe(::consoleWrite(app, app.FolderPath));
				}
			}
			}
#else
			else {
				cnst ::llc::err_t openResult = ::desktopFolderOpen(app, path);
				if(::llc::failed(openResult)) {
					if_fail_fef(::consoleWrite(app, "Cannot open directory."), "Directory:%.*s.", LLC_VIEW_ARGS(path));
				}
				else {
					if_fail_fe(::consoleWrite(app, app.FolderPath));
				}
			}
#endif
		}
		else
			if_fail_fe(::consoleBrokerRun(app, command));
		if_fail_fe(app.ConsoleInput.SetText(app.GUI, {}));
		if_fail_fe(::consoleRowsRefresh(app));
	}
	bool doubleClick = false;
	if_fail_fe(::sampleGUIViewportDoubleClick(app.Window.EventQueue, doubleClick));
	::llc::u2_t hoveredViewport = app.Viewports.size();
	::llc::view<::llc::SViewport> viewports = app.Viewports;
	if_fail_fe(viewports.enumerate([&app, &hoveredViewport](::llc::u2_t iViewport, ::llc::SViewport & viewport) -> ::llc::err_t {
		cnst ::llc::cid_t idTitle = viewport.IdControls[::llc::VIEWPORT_CONTROL_TITLE];
		cnst ::llc::cid_t idClose = viewport.IdControls[::llc::VIEWPORT_CONTROL_CLOSE];
		cnst ::llc::cid_t idResize = viewport.IdControls[::llc::VIEWPORT_CONTROL_RESIZE_BOTTOM_RIGHT];
		if(::sampleGUIViewportPressed(app, viewport))
			if_fail_fe(::sampleGUIViewportActivate(app, (::llc::u0_t)iViewport));
		if(app.GUI.Controls.IsAction(idClose))
			app.GUI.Controls.SetHidden(viewport.IdControl, true);
		if(app.GUI.Controls.IsPressed(idTitle) && (app.Window.Input->MouseCurrent.Deltas.x || app.Window.Input->MouseCurrent.Deltas.y)) {
			cnst ::llc::n2s1_t delta = {(::llc::s1_t)app.Window.Input->MouseCurrent.Deltas.x, (::llc::s1_t)app.Window.Input->MouseCurrent.Deltas.y};
			app.GUI.Controls.Placement[viewport.IdControl].Area.Offset += delta;
			if_fail_fe(::llc::controlMetricsInvalidate(app.GUI, viewport.IdControl));
		}
		if(app.GUI.Controls.IsPressed(idResize) && (app.Window.Input->MouseCurrent.Deltas.x || app.Window.Input->MouseCurrent.Deltas.y)) {
			cnst ::llc::n2s1_t delta = {(::llc::s1_t)app.Window.Input->MouseCurrent.Deltas.x, (::llc::s1_t)app.Window.Input->MouseCurrent.Deltas.y};
			app.GUI.Controls.Placement[viewport.IdControl].Area.Size += delta;
			app.GUI.Controls.Placement[viewport.IdControls[::llc::VIEWPORT_CONTROL_TARGET]].Area.Size += delta;
			app.UnfoldedSize[iViewport] = app.GUI.Controls.Placement[viewport.IdControl].Area.Size;
			if_fail_fe(::llc::controlMetricsInvalidate(app.GUI, viewport.IdControl));
		}
		if(app.GUI.Controls.IsHovered(idTitle))
			hoveredViewport = iViewport;
		rtrn 0;
	}));
	if(doubleClick && hoveredViewport < app.Viewports.size())
		if_fail_fe(::sampleGUIViewportFold(app, (::llc::u0_t)hoveredViewport));
	cnst ::llc::s2_t wheel = app.Window.Input->MouseCurrent.Deltas.z;
	if(wheel) {
		cnst ::llc::cid_t idFolder = app.Viewports[0].IdControls[::llc::VIEWPORT_CONTROL_TARGET];
		cnst ::llc::cid_t idFile = app.Viewports[1].IdControls[::llc::VIEWPORT_CONTROL_TARGET];
		cnst ::llc::cid_t idProcesses = app.Viewports[4].IdControls[::llc::VIEWPORT_CONTROL_TARGET];
		cnst ::llc::cid_t idServices = app.Viewports[5].IdControls[::llc::VIEWPORT_CONTROL_TARGET];
		cnst ::llc::cid_t idResults = app.Viewports[3].IdControls[::llc::VIEWPORT_CONTROL_TARGET];
		if(::llc::in_range(app.GUI.CursorPos.s1(), app.GUI.Controls.Area[idFolder].Total.Global)) {
			cnst ::llc::u2_t maxOffset = app.FolderEntries->Name.size() > DESKTOP_VISIBLE_ROWS - 1 ? app.FolderEntries->Name.size() - (DESKTOP_VISIBLE_ROWS - 1) : 0;
			app.FolderOffset = wheel > 0 ? (app.FolderOffset > 5 ? app.FolderOffset - 5 : 0) : ::llc::min(maxOffset, app.FolderOffset + 5);
			if_fail_fe(::desktopFolderRowsRefresh(app));
		}
		else if(::llc::in_range(app.GUI.CursorPos.s1(), app.GUI.Controls.Area[idFile].Total.Global)) {
			if(app.Window.Input->KeyboardCurrent.KeyState[::llc::SYSTEM_KEY_Shift]) {
				cnst ::llc::u2_t maxColumn = app.FileLineWidthMax > app.VisibleColumns ? app.FileLineWidthMax - app.VisibleColumns : 0;
				app.FileColumnOffset = wheel > 0 ? (app.FileColumnOffset > 8 ? app.FileColumnOffset - 8 : 0) : ::llc::min(maxColumn, app.FileColumnOffset + 8);
			}
			else {
				cnst ::llc::u2_t maxOffset = app.TextRows.size() > DESKTOP_VISIBLE_ROWS ? app.TextRows.size() - DESKTOP_VISIBLE_ROWS : 0;
				app.FileOffset = wheel > 0 ? (app.FileOffset > 15 ? app.FileOffset - 15 : 0) : ::llc::min(maxOffset, app.FileOffset + 15);
			}
			if_fail_fe(::desktopFileRowsRefresh(app));
			if(!app.Window.Input->KeyboardCurrent.KeyState[::llc::SYSTEM_KEY_Shift])
				if_fail_fe(::desktopFileChunkShift(app, wheel < 0));
		}
		else if(::llc::in_range(app.GUI.CursorPos.s1(), app.GUI.Controls.Area[idProcesses].Total.Global)) {
			cnst ::llc::u2_t maxOffset = app.ProcessNames.size() > app.ProcessControls.size() ? app.ProcessNames.size() - app.ProcessControls.size() : 0;
			app.ProcessOffset = wheel > 0 ? (app.ProcessOffset > 5 ? app.ProcessOffset - 5 : 0) : ::llc::min(maxOffset, app.ProcessOffset + 5);
			if_fail_fe(::inventoryRowsRefresh(app, app.ProcessControls, app.ProcessNames, app.ProcessWords, app.ProcessOffset));
		}
		else if(::llc::in_range(app.GUI.CursorPos.s1(), app.GUI.Controls.Area[idServices].Total.Global)) {
			cnst ::llc::u2_t maxOffset = app.ServiceNames.size() > app.ServiceControls.size() ? app.ServiceNames.size() - app.ServiceControls.size() : 0;
			app.ServiceOffset = wheel > 0 ? (app.ServiceOffset > 5 ? app.ServiceOffset - 5 : 0) : ::llc::min(maxOffset, app.ServiceOffset + 5);
			if_fail_fe(::inventoryRowsRefresh(app, app.ServiceControls, app.ServiceNames, app.ServiceWords, app.ServiceOffset));
		}
		else if(::llc::in_range(app.GUI.CursorPos.s1(), app.GUI.Controls.Area[idResults].Total.Global)) {
			cnst ::llc::u2_t maxOffset = app.FileResults.size() > app.FileResultControls.size() ? app.FileResults.size() - app.FileResultControls.size() : 0;
			app.ResultOffset = wheel > 0 ? (app.ResultOffset > 5 ? app.ResultOffset - 5 : 0) : ::llc::min(maxOffset, app.ResultOffset + 5);
			if_fail_fe(::fileResultsRefresh(app));
		}
	}
	if(app.OpenFile.size() && (app.Window.Input->KeyDown(::llc::SYSTEM_KEY_Next) || app.Window.Input->KeyDown(::llc::SYSTEM_KEY_Prior))) {
		cnst bool down = app.Window.Input->KeyDown(::llc::SYSTEM_KEY_Next);
		cnst ::llc::u2_t maxOffset = app.TextRows.size() > DESKTOP_VISIBLE_ROWS ? app.TextRows.size() - DESKTOP_VISIBLE_ROWS : 0;
		app.FileOffset = down ? ::llc::min(maxOffset, app.FileOffset + DESKTOP_VISIBLE_ROWS) : (app.FileOffset > DESKTOP_VISIBLE_ROWS ? app.FileOffset - DESKTOP_VISIBLE_ROWS : 0);
		if_fail_fe(::desktopFileRowsRefresh(app));
		if_fail_fe(::desktopFileChunkShift(app, down));
	}
	bool folderHandled = false;
	::llc::view<::llc::cid_t> folderControls = app.FolderControls;
	if_fail_fe(folderControls.enumerate([&app, &folderHandled](::llc::u2_t i, ::llc::cid_t & id) -> ::llc::err_t {
		if(folderHandled || !app.GUI.Controls.IsAction(id)) rtrn 0;
		folderHandled = true;
			::llc::string path = {};
			if(0 == i) {
				if_fail_fe(::llc::pathNameCompose(app.FolderPath, "..", path));
				::llc::string parent = {};
				if_fail_fe(::llc::pathAbsolute(path, parent));
				if_fail_fe(::desktopFolderOpen(app, parent));
			}
			else {
				cnst ::llc::u2_t entry = app.FolderOffset + i - 1;
				if_true_fef(entry >= app.FolderEntries->Name.size(), "Folder entry:%u. Count:%u.", entry, app.FolderEntries->Name.size());
				cnst bool folder = app.FolderEntries->Info[entry].Folder;
				if_fail_fe(::llc::pathNameCompose(app.FolderPath, app.FolderEntries->Name[entry], path));
				if(folder) {
					if_fail_fe(::desktopFolderOpen(app, path));
				}
				else {
					app.SelectedFile = path;
					app.SelectedFileEntry = entry;
					if_fail_fe(::desktopFolderRowsRefresh(app));
					if_fail_fef(::previewFile(app, app.SelectedFile), "Selected file:%.*s.", LLC_VIEW_ARGS(app.SelectedFile));
				}
			}
		rtrn 0;
	}));
	if(app.GUI.Controls.IsAction(app.OpenSelected) && app.SelectedFile.size()) {
		if_fail_fe(::desktopFileOpen(app, app.SelectedFile));
		if_fail_fe(::sampleGUIViewportActivate(app, 1));
	}
	::llc::u2_t resultRow = 0;
	if_fail_fe(app.FileResultControls.for_each([&](cnst ::llc::cid_t & id) -> ::llc::err_t {
		cnst ::llc::u2_t result = app.ResultOffset + resultRow++;
		if(result >= app.FileResults.size() || !app.GUI.Controls.IsAction(id)) rtrn 0;
		app.SelectedFile = app.IndexPaths[app.FileResults[result]];
		app.SelectedFileEntry = (::llc::u2_t)-1;
		if_fail_fef(::previewFile(app, app.SelectedFile), "Selected result:%.*s.", LLC_VIEW_ARGS(app.SelectedFile));
		rtrn ::fileResultsRefresh(app);
	}));
	rtrn ::llc::guiUpdateMetrics(app.GUI, app.Window.Size, false);
}

::llc::err_t sampleGUIViewportDraw(SSampleGUIViewport & app, ::llc::img8bgra (&clientImages)[SAMPLE_GUI_VIEWPORT_COUNT], ::llc::u2_t frameIndex, ::llc::g8bgra target) {
	if_fail_fe(target.fill(::llc::SColorBGRA{16, 14, 12}));
	(void)clientImages;
	(void)frameIndex;
	rtrn ::llc::guiDraw(app.GUI, target);
}

#define LLC_SAMPLE_GUI_VIEWPORT_TITLE "LLC Server"

stct SApplication {
	::llc::SFramework	Framework;
	SSampleGUIViewport	Sample;
	::llc::string		ScreenshotDirectory = {};
	::llc::img8bgra	ClientImages[SAMPLE_GUI_VIEWPORT_COUNT] = {};
	::llc::u2_t		FrameIndex = 0;

	SApplication(::llc::SRuntimeValues & runtimeValues)
		: Framework(runtimeValues, {}), Sample(Framework.RootWindow, *Framework.GUI) {
		if_fail_e(::llc::rasterFontDefaults(Sample.Engine.Scene->Graphics->Fonts));
	}
};

::llc::err_t setup(SApplication & app) {
	::llc::vcst_t screenshotPath = SCREENSHOT_PATH_DEFAULT;
	::std::error_code configError = {};
	if(::std::filesystem::exists("llc_config.json", configError) && 0 <= ::llc::jsonFileRead(app.Framework.JSONConfig, "llc_config.json")) {
		::llc::vcst_t configuredPath = {};
		if(0 <= ::llc::jsonExpressionResolve("screenshot_path", app.Framework.JSONConfig.Reader, 0, configuredPath) && configuredPath.size())
			screenshotPath = configuredPath;
	}
	if(::llc::failed(::llc::pathNormalize(screenshotPath, app.ScreenshotDirectory)) || ::llc::failed(::llc::pathCreate(app.ScreenshotDirectory)))
		app.ScreenshotDirectory = SCREENSHOT_PATH_DEFAULT;
	if(::llc::argsOptionIndex(app.Framework.RuntimeValues.EntryPointArgs, "index") >= 0) {
		::llc::vcst_t value = {};
		if_fail_fe(::llc::argsOptionValue(app.Framework.RuntimeValues.EntryPointArgs, "index", value));
		if_zero_fef(value.size(), "Missing --index path. Length:%u.", value.size());
		if_fail_fef(::llc::pathAbsolute(value, app.Sample.IndexDatabasePath), "Index path:%.*s.", LLC_VIEW_ARGS(value));
	}
	::llc::SWindow & window = app.Framework.RootWindow;
	window.Size = {1600, 900};
	if_null_ve(-1, window.BackBuffer.create());
	if_fail_fe(window.BackBuffer->resize(window.Size.u2()));
	if_fail_fe(::llc::mainWindowCreate(window, app.Framework.RuntimeValues.PlatformDetail, window.Input));
	rtrn ::sampleGUIViewportSetup(app.Sample);
}

::llc::err_t cleanup(SApplication & app) {
	rtrn ::llc::mainWindowDestroy(app.Framework.RootWindow);
}

::llc::err_t update(SApplication & app, bool systemRequestedExit) {
	if(systemRequestedExit)
		rtrn ::llc::APPLICATION_STATE_EXIT;
	cnst ::llc::err_t result = ::llc::updateFramework(app.Framework);
	if(result)
		rtrn result;
	cnst ::llc::err_t resultSample = ::sampleGUIViewportUpdate(app.Sample, false, app.Framework.FrameInfo.Seconds.LastFrame);
	if(resultSample)
		rtrn resultSample;
	::llc::GUI_VIEWPORT_ACTION action = ::llc::GUI_VIEWPORT_ACTION_NONE;
	bool resized = false;
	cnst ::llc::cid_t idFile = app.Sample.Viewports[1].IdControls[::llc::VIEWPORT_CONTROL_TARGET];
	if_fail_fe(::llc::guiViewportEvents(app.Sample.GUI, app.Sample.ViewportActions[1], idFile, action, resized));
	if(::llc::GUI_VIEWPORT_ACTION_WRAP == action) {
		app.Sample.WordWrap = !app.Sample.WordWrap;
		app.Sample.GUI.Controls.SetSelected(app.Sample.ViewportActions[1][::llc::GUI_VIEWPORT_ACTION_WRAP], app.Sample.WordWrap);
	}
	if(::llc::GUI_VIEWPORT_ACTION_SELECT_ALL == action)
		app.Sample.SelectAll = true;
	if(::llc::GUI_VIEWPORT_ACTION_SAVE == action && app.Sample.OpenFile.size() && ::llc::fileSize(app.Sample.OpenFile) == app.Sample.FileBytes.size())
		if_fail_fe(::llc::fileFromMemory(app.Sample.OpenFile, app.Sample.FileBytes.cc()));
	if(resized || ::llc::GUI_VIEWPORT_ACTION_WRAP == action || ::llc::GUI_VIEWPORT_ACTION_SELECT_ALL == action || app.Sample.VisibleColumns != ::llc::guiViewportTextColumns(app.Sample.GUI, app.Sample.FileControls[0]))
		if_fail_fe(::desktopTextRowsRebuild(app.Sample));
	if_fail_fe(::llc::windowScreenshotHotkey(app.Framework.RootWindow, app.Framework.RuntimeValues.EntryPointArgs.ProgramName, app.ScreenshotDirectory));
	::llc::sleep(1);
	rtrn resultSample;
}

::llc::err_t draw(SApplication & app) {
	::llc::SWindow & window = app.Framework.RootWindow;
	if(window.NoDraw)
		rtrn 0;
	if_fail_fe(window.BackBuffer->resize(window.Size.u2()));
	rtrn ::sampleGUIViewportDraw(app.Sample, app.ClientImages, app.FrameIndex++, window.BackBuffer->Color.View);
}

LLC_DEFINE_APPLICATION_ENTRY_POINT(SApplication, LLC_SAMPLE_GUI_VIEWPORT_TITLE)
