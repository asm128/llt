// Composed from llc_sample_gui_viewport and llc_sample_gui_components.
#include "llc_nn.h"
#include "llc_framework.h"
#include "llc_app_impl.h"
#include "llc_runtime.h"
#include "llc_gui_viewport.h"
#include "llc_engine.h"
#include "llc_apod_serialize.h"
#include "llc_system_key.h"
#include "llc_bitmap_target.h"
#include "llc_minmax.h"
#include "llc_png.h"
#include "llc_file.h"
#include <cmath>
#ifdef LLC_WINDOWS
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")
#endif

stct SSampleCheckBox {
	::llc::cid_t		IdControl	= ::llc::CID_INVALID;
	bool			Checked		= false;
};

stct SSampleSlider {
	::llc::cid_t		IdRoot		= ::llc::CID_INVALID;
	::llc::cid_t		IdThumb		= ::llc::CID_INVALID;
	::llc::minmaxs3_t	ValueLimits	= {0, 100};
	::llc::s3_t		ValueCurrent	= 0;
	::llc::s3_t		ValueStep	= 1;
	::llc::s3_t		ValuePage	= 10;
	bool			Vertical	= false;
	::llc::astchar<64>	ValueText	= {};
};


stacxpr ::llc::u0_t SAMPLE_GUI_THEME_COUNT = 3;
stacxpr ::llc::u0_t PALETTE_SHADE_COUNT = 16;

sttc ::llc::SColorBGRA sampleGUIThemeColor(::llc::u0_t theme) nxpt {
	stacxpr ::llc::SColorBGRA colors[SAMPLE_GUI_THEME_COUNT] =
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
	stacxpr ::llc::u0_t shades[::llc::GUI_CONTROL_PALETTE_COUNT] = {8, 10, 5, 11, 4, 7, 2, 7, 1, 13};
	rtrn shades[state % ::llc::GUI_CONTROL_PALETTE_COUNT];
}

sttc ::llc::u0_t sampleGUIStateBorderOffset(::llc::u0_t state) nxpt {
	stacxpr ::llc::u0_t offsets[::llc::GUI_CONTROL_PALETTE_COUNT] = {5, 1, 7, 3, 0, 1, 7, 3, 5, 5};
	rtrn offsets[state % ::llc::GUI_CONTROL_PALETTE_COUNT];
}

sttc ::llc::SColorBGRA sampleGUITextColor(::llc::SColorBGRA background) nxpt {
	cnst ::llc::u2_t luminance = (::llc::u2_t)background.r * 299 + (::llc::u2_t)background.g * 587 + (::llc::u2_t)background.b * 114;
	rtrn ::sampleGUIShade({255, 255, 255}, luminance > 150000 ? 15 : 1);
}

sttc ::llc::err_t sampleGUIResourcesCreate(::llc::SGUI & gui) {
	if_true_fe(0 == gui.Colors.create());
	if_fail_fe(gui.Colors->Palettes.resize(SAMPLE_GUI_THEME_COUNT * ::llc::GUI_CONTROL_PALETTE_COUNT));
	for(::llc::u0_t iTheme = 0; iTheme < SAMPLE_GUI_THEME_COUNT; ++iTheme)
	for(::llc::u0_t iState = 0; iState < ::llc::GUI_CONTROL_PALETTE_COUNT; ++iState) {
		::llc::astatic<::llc::bgra, ::llc::UI_CONTROL_AREA_COUNT> & palette = gui.Colors->Palettes[iTheme * ::llc::GUI_CONTROL_PALETTE_COUNT + iState];
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
		for(::llc::u2_t iArea = 0; iArea < palette.size(); ++iArea)
			palette[iArea] = ::sampleGUIShade(tone, (::llc::u0_t)::llc::min((::llc::u2_t)shade + 5, (::llc::u2_t)PALETTE_SHADE_COUNT - 1));
		palette[::llc::UI_CONTROL_AREA_BACKGROUND] = ::sampleGUIShade(tone, (::llc::u0_t)::llc::min((::llc::u2_t)shade + 6, (::llc::u2_t)PALETTE_SHADE_COUNT - 1));
		palette[::llc::UI_CONTROL_AREA_CLIENT] = base;
		palette[::llc::UI_CONTROL_AREA_BORDER_LEFT] = borderColor;
		palette[::llc::UI_CONTROL_AREA_BORDER_TOP] = borderColor;
		palette[::llc::UI_CONTROL_AREA_BORDER_RIGHT] = borderColor;
		palette[::llc::UI_CONTROL_AREA_BORDER_BOTTOM] = borderColor;
		palette[::llc::UI_CONTROL_AREA_TEXT_BACKGROUND] = ::sampleGUIShade(tone, (::llc::u0_t)::llc::min((::llc::u2_t)shade + 2, (::llc::u2_t)PALETTE_SHADE_COUNT - 1));
		palette[::llc::UI_CONTROL_AREA_TEXT_FACE] = ::sampleGUITextColor(base);
	}
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
	for(::llc::u0_t iState = 0; iState < ::llc::GUI_CONTROL_PALETTE_COUNT; ++iState)
		draw.Palettes[iState] = (::llc::s1_t)((theme % SAMPLE_GUI_THEME_COUNT) * ::llc::GUI_CONTROL_PALETTE_COUNT + iState);
}

sttc ::llc::err_t sampleCheckBoxSetChecked(SSampleCheckBox & checkBox, ::llc::SGUI & gui, bool checked) {
	if_true_fe(::llc::controlInvalid(gui, checkBox.IdControl));
	if(checkBox.Checked == checked)
		rtrn 0;
	if_fail_fe(::llc::controlTextSet(gui, checkBox.IdControl, checked ? ::llc::vcst_t{"X"} : ::llc::vcst_t{}));
	checkBox.Checked = checked;
	gui.Controls.SetSelected(checkBox.IdControl, checked);
	rtrn 1;
}

sttc ::llc::err_t sampleCheckBoxCreate(SSampleCheckBox & checkBox, ::llc::SGUI & gui, ::llc::cid_t idParent, cnst ::llc::rect2s1_t & area) {
	if_true_fe(checkBox.IdControl != ::llc::CID_INVALID);
	checkBox.IdControl = ::llc::controlCreateChild(gui, idParent);
	if_true_fe(checkBox.IdControl < 0);
	::llc::SControlPlacement & placement = gui.Controls.Placement[checkBox.IdControl];
	placement.Area = area;
	placement.Margin = {1, 1, 1, 1};
	placement.Border = {2, 2, 2, 2};
	gui.Controls.Text[checkBox.IdControl].Align = ::llc::ALIGN_CENTER;
	rtrn 0;
}

sttc ::llc::err_t sampleCheckBoxUpdate(SSampleCheckBox & checkBox, ::llc::SGUI & gui) {
	if_true_fe(::llc::controlInvalid(gui, checkBox.IdControl));
	if(false == gui.Controls.IsAction(checkBox.IdControl))
		rtrn 0;
	rtrn ::sampleCheckBoxSetChecked(checkBox, gui, false == checkBox.Checked);
}

sttc ::llc::s3_t sampleSliderValueOffset(cnst SSampleSlider & slider, bool increase, ::llc::s3_t amount) nxpt {
	if(increase) {
		cnst ::llc::u3_t available = (::llc::u3_t)slider.ValueLimits.Max - (::llc::u3_t)slider.ValueCurrent;
		rtrn (::llc::u3_t)amount < available ? slider.ValueCurrent + amount : slider.ValueLimits.Max;
	}
	cnst ::llc::u3_t available = (::llc::u3_t)slider.ValueCurrent - (::llc::u3_t)slider.ValueLimits.Min;
	rtrn (::llc::u3_t)amount < available ? slider.ValueCurrent - amount : slider.ValueLimits.Min;
}

sttc ::llc::err_t sampleSliderTextUpdate(SSampleSlider & slider, ::llc::SGUI & gui) {
	cnst ::llc::err_t length = ::llc::sprintf_s(slider.ValueText.Storage, "%" LLC_FMT_S3, slider.ValueCurrent);
	if_fail_fe(length);
	if_true_fe((::llc::u2_t)length >= slider.ValueText.size());
	rtrn ::llc::controlTextSet(gui, slider.IdThumb, {slider.ValueText.Storage, (::llc::u2_t)length});
}

sttc ::llc::err_t sampleSliderSetValue(SSampleSlider & slider, ::llc::SGUI & gui, ::llc::s3_t value) {
	if_true_fe(::llc::controlInvalid(gui, slider.IdRoot));
	if_true_fe(::llc::controlInvalid(gui, slider.IdThumb));
	cnst ::llc::s3_t valueNext = ::llc::max(slider.ValueLimits.Min, ::llc::min(slider.ValueLimits.Max, value));
	slider.ValueCurrent = valueNext;

	cnst ::llc::SControlPlacement & rootPlacement = gui.Controls.Placement[slider.IdRoot];
	::llc::SControlPlacement & thumbPlacement = gui.Controls.Placement[slider.IdThumb];
	cnst ::llc::n2u1_t ncSpacing = ::llc::controlNCSpacing(rootPlacement);
	cnst ::llc::s2_t rootLength = slider.Vertical ? rootPlacement.Area.Size.y : rootPlacement.Area.Size.x;
	cnst ::llc::s2_t nonClientLength = slider.Vertical ? ncSpacing.y : ncSpacing.x;
	cnst ::llc::s2_t thumbLength = slider.Vertical ? thumbPlacement.Area.Size.y : thumbPlacement.Area.Size.x;
	cnst ::llc::s2_t trackLength = ::llc::max((::llc::s2_t)0, rootLength - nonClientLength - thumbLength);
	cnst long double range = (long double)slider.ValueLimits.Max - (long double)slider.ValueLimits.Min;
	cnst long double proportion = range ? ((long double)slider.ValueCurrent - (long double)slider.ValueLimits.Min) / range : 0;
	cnst ::llc::s1_t offsetNext = (::llc::s1_t)(trackLength * (slider.Vertical ? 1.0L - proportion : proportion));
	if(slider.Vertical)
		thumbPlacement.Area.Offset.y = offsetNext;
	else
		thumbPlacement.Area.Offset.x = offsetNext;
	if_fail_fe(::sampleSliderTextUpdate(slider, gui));
	rtrn ::llc::controlMetricsInvalidate(gui, slider.IdRoot);
}

sttc ::llc::err_t sampleSliderCreate(SSampleSlider & slider, ::llc::SGUI & gui, ::llc::cid_t idParent, cnst ::llc::rect2s1_t & area, bool vertical, ::llc::s3_t value) {
	if_true_fe(slider.IdRoot != ::llc::CID_INVALID || slider.IdThumb != ::llc::CID_INVALID);
	slider.IdRoot = ::llc::controlCreateChild(gui, idParent);
	if_true_fe(slider.IdRoot < 0);
	slider.IdThumb = ::llc::controlCreateChild(gui, slider.IdRoot);
	if_true_fe(slider.IdThumb < 0);
	slider.Vertical = vertical;
	::llc::SControlPlacement & rootPlacement = gui.Controls.Placement[slider.IdRoot];
	rootPlacement.Area = area;
	rootPlacement.Margin = {1, 1, 1, 1};
	rootPlacement.Border = {2, 2, 2, 2};
	gui.Controls.Text[slider.IdRoot].Align = ::llc::ALIGN_CENTER;
	::llc::SControlPlacement & thumbPlacement = gui.Controls.Placement[slider.IdThumb];
	thumbPlacement.Margin = {};
	thumbPlacement.Border = {1, 1, 1, 1};
	thumbPlacement.Area.Size = {16, 16};
	::llc::SControlConstraints & thumbConstraints = gui.Controls.Constraints[slider.IdThumb];
	if(vertical)
		thumbConstraints.AttachSizeToControl.x = slider.IdThumb;
	else
		thumbConstraints.AttachSizeToControl.y = slider.IdThumb;
	rtrn ::sampleSliderSetValue(slider, gui, value);
}

sttc ::llc::err_t sampleSliderUpdate(SSampleSlider & slider, ::llc::SGUI & gui, cnst ::llc::SInput & input) {
	::llc::s3_t valueNext = slider.ValueCurrent;
	cnst bool focused = gui.Controls.IsFocused(slider.IdRoot) || gui.Controls.IsFocused(slider.IdThumb);
	cnst bool hovered = gui.Controls.IsHovered(slider.IdRoot) || gui.Controls.IsHovered(slider.IdThumb);
	if(focused) {
		if(input.KeyDown(::llc::SYSTEM_KEY_Home))
			valueNext = slider.ValueLimits.Min;
		else if(input.KeyDown(::llc::SYSTEM_KEY_End))
			valueNext = slider.ValueLimits.Max;
		else if(input.KeyDown(::llc::SYSTEM_KEY_Prior))
			valueNext = ::sampleSliderValueOffset(slider, true, slider.ValuePage);
		else if(input.KeyDown(::llc::SYSTEM_KEY_Next))
			valueNext = ::sampleSliderValueOffset(slider, false, slider.ValuePage);
		else if(input.KeyDown(slider.Vertical ? ::llc::SYSTEM_KEY_Up : ::llc::SYSTEM_KEY_Right))
			valueNext = ::sampleSliderValueOffset(slider, true, slider.ValueStep);
		else if(input.KeyDown(slider.Vertical ? ::llc::SYSTEM_KEY_Down : ::llc::SYSTEM_KEY_Left))
			valueNext = ::sampleSliderValueOffset(slider, false, slider.ValueStep);
	}
	if((focused || hovered) && input.MouseCurrent.Deltas.z)
		valueNext = ::sampleSliderValueOffset(slider, 0 < input.MouseCurrent.Deltas.z, slider.ValueStep);

	cnst ::llc::SControlState & thumbState = gui.Controls.States[slider.IdThumb];
	if(thumbState.IsPressed() && input.MouseCurrent.Position != input.MousePrevious.Position) {
		cnst ::llc::SControlArea & rootArea = gui.Controls.Area[slider.IdRoot];
		cnst ::llc::SControlArea & thumbArea = gui.Controls.Area[slider.IdThumb];
		cnst ::llc::s2_t rootLength = slider.Vertical ? rootArea.Client.Global.Size.y : rootArea.Client.Global.Size.x;
		cnst ::llc::s2_t thumbSize = slider.Vertical ? thumbArea.Total.Global.Size.y : thumbArea.Total.Global.Size.x;
		cnst ::llc::s2_t trackLength = ::llc::max((::llc::s2_t)0, rootLength - thumbSize);
		if(trackLength) {
			cnst ::llc::s2_t cursorPosition = slider.Vertical ? input.MouseCurrent.Position.y : input.MouseCurrent.Position.x;
			cnst ::llc::s2_t rootOffset = slider.Vertical ? rootArea.Client.Global.Offset.y : rootArea.Client.Global.Offset.x;
			cnst ::llc::s2_t cursor = cursorPosition - rootOffset - thumbSize / 2;
			long double proportion = (long double)::llc::max<::llc::s2_t>(0, ::llc::min(trackLength, cursor)) / trackLength;
			if(slider.Vertical)
				proportion = 1.0L - proportion;
			cnst long double range = (long double)slider.ValueLimits.Max - (long double)slider.ValueLimits.Min;
			valueNext = (::llc::s3_t)((long double)slider.ValueLimits.Min + range * proportion);
		}
	}
	else if(input.ButtonDown(0) && gui.Controls.IsPressed(slider.IdRoot)) {
		cnst ::llc::SControlArea & thumbArea = gui.Controls.Area[slider.IdThumb];
		cnst ::llc::s2_t thumbOffset = slider.Vertical ? thumbArea.Total.Global.Offset.y : thumbArea.Total.Global.Offset.x;
		cnst ::llc::s2_t thumbSize = slider.Vertical ? thumbArea.Total.Global.Size.y : thumbArea.Total.Global.Size.x;
		cnst ::llc::s2_t cursorPosition = slider.Vertical ? input.MouseCurrent.Position.y : input.MouseCurrent.Position.x;
		cnst bool increase = slider.Vertical ? cursorPosition < thumbOffset + thumbSize / 2 : cursorPosition > thumbOffset + thumbSize / 2;
		valueNext = ::sampleSliderValueOffset(slider, increase, slider.ValuePage);
	}
	rtrn valueNext != slider.ValueCurrent ? ::sampleSliderSetValue(slider, gui, valueNext) : 0;
}


sttc ::llc::err_t sampleGUIViewportDoubleClick(cnst ::llc::TQueueSystemEvent & events, bool & doubleClick) {
	doubleClick = false;
	for(::llc::u2_t iEvent = 0; iEvent < events.size(); ++iEvent) {
		cnst ::llc::pobj<::llc::SEventSystem> & eventSystem = events[iEvent];
		if(0 == eventSystem.get_ref() || eventSystem->Type != ::llc::SYSTEM_EVENT_Mouse)
			continue;
		::llc::SEView<::llc::EVENT_MOUSE> eventMouse = {};
		if_fail_fe(eventSystem->ExtractChild(eventMouse));
		if(eventMouse.Type != ::llc::EVENT_MOUSE_DoubleClick)
			continue;
		::llc::vcu0_t input = eventMouse.Data;
		::llc::u0_t button = 0;
		if_fail_fe(::llc::loadPOD(input, button));
		if_true_fef(input.size(), "Unexpected mouse double-click payload tail:%u.", input.size());
		if(0 == button)
			doubleClick = true;
	}
	rtrn 0;
}




stxp ::llc::u2_t NN_PANES = 8; // Network window followed by its seven inspectors.
stct SNNSession {
 ::llc::SNN Network;
 ::llc::astatic<::llc::f3_t, 512> History = {};
 ::llc::u2_t HistoryCount = 0, HistoryCursor = 0;
 ::llc::cid_t Run = -1, Step = -1, Reset = -1, Description = -1, Status = -1, Preview = -1, Trace = -1, SpeedLabel = -1, ScaleLabel = -1;
 ::llc::astatic<::llc::cid_t, 7> Inspectors = {};
 ::llc::astatic<bool, 7> Open = {};
 SSampleSlider Speed = {};
 SSampleSlider CellSize = {}, UnitsX = {}, UnitsY = {};
 ::llc::cid_t Layout = -1, CellLabel = -1, UnitsLabelX = -1, UnitsLabelY = -1, ApplySize = -1;
 ::llc::u2_t DisplayLayout = 0; // Native, strip, square: presentation only, never connectivity.
 SSampleCheckBox AutoScale = {};
 ::llc::img8bgra TraceImage = {};
 ::llc::astchar<256> StatusText = {};
 ::llc::n2s1_t LastSize = {};
 bool Running = false;
 ::llc::u3_t LastImagePass = 0;
 ::llc::astchar<160> ImageMessage = {};
};

stct SApplication {
 ::llc::SFramework Framework;
 ::llc::astatic<SNNSession, ::llc::NN_COUNT> Sessions = {};
 ::llc::astatic<::llc::SViewport, ::llc::NN_COUNT * NN_PANES> Viewports = {};
 ::llc::astatic<::llc::img8bgra, ::llc::NN_COUNT * NN_PANES> Images = {};
 ::llc::astatic<::llc::astchar<256>, ::llc::NN_COUNT * NN_PANES> Details = {}, Captions = {};
 ::llc::astatic<::llc::cid_t, ::llc::NN_COUNT> Choices = {};
 ::llc::cid_t Desktop = -1, Title = -1, Exit = -1, Restore = -1, Save = -1;
 ::llc::n2u1_t LastSize = {};
 ::llc::s1_t WorkspaceTop = 100;
 bool Capture = false;
 SApplication(::llc::SRuntimeValues & runtime) : Framework(runtime, {}) {}
};

sttc ::llc::err_t nnControl(SApplication & app, ::llc::cid_t parent, ::llc::cid_t & id, ::llc::vcst_t text) {
 auto & gui = *app.Framework.GUI;
 id = ::llc::controlCreateChild(gui, parent);
 if_fail_fe(id);
 gui.Controls.Placement[id].Border = {1, 1, 1, 1};
 gui.Controls.Placement[id].Margin = {4, 4, 2, 2};
 gui.Controls.Text[id].Align = ::llc::ALIGN_CENTER_LEFT;
 gui.Controls.Text[id].FontSelected = 7;
 ::sampleGUIControlThemeApply(gui, id, 0);
 rtrn ::llc::controlTextSet(gui, id, text);
}

sttc ::llc::err_t nnPlace(::llc::SGUI & gui, ::llc::cid_t id, int x, int y, int width, int height) {
 auto & area = gui.Controls.Placement[id].Area;
 const ::llc::rect2s1_t next = {{(::llc::s1_t)x, (::llc::s1_t)y}, {(::llc::s1_t)::llc::max(1, width), (::llc::s1_t)::llc::max(1, height)}};
 if(area.Offset == next.Offset && area.Size == next.Size) rtrn 0;
 area = next;
 rtrn ::llc::controlMetricsInvalidate(gui, id);
}

sttc ::llc::err_t nnViewportSize(SApplication & app, ::llc::u2_t index, int x, int y, int width, int height) {
 auto & gui = *app.Framework.GUI;
 auto & viewport = app.Viewports[index];
 if_fail_fe(nnPlace(gui, viewport.IdControl, x, y, width, viewport.Folded ? 30 : height));
 if(!viewport.Folded) {
  if_fail_fe(nnPlace(gui, viewport.IdControls[::llc::VIEWPORT_CONTROL_TARGET], 0, 0, width - 8, height - 32));
  viewport.UnfoldedSize = {(::llc::s1_t)width, (::llc::s1_t)height};
 }
 rtrn 0;
}

sttc ::llc::err_t nnSessionLayout(SApplication & app, ::llc::u2_t n) {
 auto & gui = *app.Framework.GUI;
 auto & session = app.Sessions[n];
 const auto size = gui.Controls.Placement[app.Viewports[n * NN_PANES].IdControls[::llc::VIEWPORT_CONTROL_TARGET]].Area.Size;
 if(size == session.LastSize) rtrn 0;
 session.LastSize = size;
 const int width = size.x - 12, half = (width - 8) / 2;
 if_fail_fe(nnPlace(gui, session.Description, 4, 0, width, 24));
 if_fail_fe(nnPlace(gui, session.Run, 4, 30, 76, 28));
 if_fail_fe(nnPlace(gui, session.Step, 84, 30, 76, 28));
 if_fail_fe(nnPlace(gui, session.Reset, 164, 30, 76, 28));
 if_fail_fe(nnPlace(gui, session.SpeedLabel, 248, 30, 108, 28));
 if_fail_fe(nnPlace(gui, session.Speed.IdRoot, 360, 30, width - 360, 28));
 if_fail_fe(::sampleSliderSetValue(session.Speed, gui, session.Speed.ValueCurrent));
 if_fail_fe(nnPlace(gui, session.AutoScale.IdControl, 4, 64, 24, 24));
 if_fail_fe(nnPlace(gui, session.ScaleLabel, n == ::llc::NN_IMAGE ? 4 : 36, 64, width - (n == ::llc::NN_IMAGE ? 4 : 36), 24));
 if_fail_fe(nnPlace(gui, session.Layout, 4, 94, 200, 26));
 if_fail_fe(nnPlace(gui, session.CellLabel, 212, 94, 120, 26));
 if_fail_fe(nnPlace(gui, session.CellSize.IdRoot, 336, 94, width - 336, 26));
 if_fail_fe(::sampleSliderSetValue(session.CellSize, gui, session.CellSize.ValueCurrent));
 const bool configurable = n == 4 || n == 7 || n == ::llc::NN_IMAGE;
 if(configurable) {
  const int controlWidth = (width - 140) / 2;
  if_fail_fe(nnPlace(gui, session.UnitsLabelX, 4, 124, controlWidth, 22));
  if_fail_fe(nnPlace(gui, session.UnitsX.IdRoot, 4, 148, controlWidth, 26));
  if_fail_fe(::sampleSliderSetValue(session.UnitsX, gui, session.UnitsX.ValueCurrent));
  if(n == 7) {
   if_fail_fe(nnPlace(gui, session.UnitsLabelY, 12 + controlWidth, 124, controlWidth, 22));
   if_fail_fe(nnPlace(gui, session.UnitsY.IdRoot, 12 + controlWidth, 148, controlWidth, 26));
   if_fail_fe(::sampleSliderSetValue(session.UnitsY, gui, session.UnitsY.ValueCurrent));
  }
  if_fail_fe(nnPlace(gui, session.ApplySize, width - 124, 148, 124, 26));
 }
 const int inspectorsTop = configurable ? 182 : 128;
 const int count = n == 4 || n == ::llc::NN_IMAGE ? 7 : 5, columns = 4, buttonWidth = (width - 12) / columns;
 for(int i = 0; i < count; ++i)
  if_fail_fe(nnPlace(gui, session.Inspectors[i], 4 + (i % columns) * (buttonWidth + 4), inspectorsTop + (i / columns) * 30, buttonWidth, 26));
 if_fail_fe(nnPlace(gui, session.Preview, 4, inspectorsTop + 64, half, size.y - inspectorsTop - 100));
 if_fail_fe(nnPlace(gui, session.Trace, 12 + half, inspectorsTop + 64, half, size.y - inspectorsTop - 100));
 if_fail_fe(nnPlace(gui, session.Status, 4, 0, width, 28));
 rtrn 0;
}

sttc ::llc::err_t nnWorkspaceLayout(SApplication & app, bool arrange) {
 auto & gui = *app.Framework.GUI;
 const auto size = app.Framework.RootWindow.Size;
 const int width = ::llc::max(320, int(size.x)), height = ::llc::max(240, int(size.y));
 const int columns = ::llc::max(1, ::llc::min(8, (width - 24) / 176));
 app.WorkspaceTop = (::llc::s1_t)(52 + ((int(::llc::NN_COUNT) + columns - 1) / columns) * 34);
 if_fail_fe(nnPlace(gui, app.Title, 12, 10, width - 300, 30));
 if_fail_fe(nnPlace(gui, app.Restore, 0, 10, 90, 30));
 if_fail_fe(nnPlace(gui, app.Save, 100, 10, 90, 30));
 if_fail_fe(nnPlace(gui, app.Exit, 200, 10, 80, 30));
 const int buttonWidth = (width - 24) / columns;
 for(::llc::u2_t n = 0; n < ::llc::NN_COUNT; ++n)
  if_fail_fe(nnPlace(gui, app.Choices[n], 12 + (n % columns) * buttonWidth, 52 + (n / columns) * 34, buttonWidth - 6, 28));
 for(::llc::u2_t i = 0; i < app.Viewports.size(); ++i) {
  auto & v = app.Viewports[i];
  auto area = gui.Controls.Placement[v.IdControl].Area;
  const int availableHeight = ::llc::max(120, height - app.WorkspaceTop - 12);
  const int w = ::llc::min(width - 24, arrange ? (i % NN_PANES ? 530 : 740) : int(area.Size.x));
  const int h = ::llc::min(availableHeight, arrange ? (i % NN_PANES ? 340 : 560) : int(v.UnfoldedSize.y));
  const int x = arrange ? 12 + (i / NN_PANES) * 26 : area.Offset.x;
  const int y = arrange ? app.WorkspaceTop + (i / NN_PANES) * 22 : area.Offset.y;
  if_fail_fe(nnViewportSize(app, i, ::llc::max(12, ::llc::min(x, width - w - 12)), ::llc::max(int(app.WorkspaceTop), ::llc::min(y, height - (v.Folded ? 30 : h) - 12)), w, h));
 }
 for(::llc::u2_t n = 0; n < ::llc::NN_COUNT; ++n) if_fail_fe(nnSessionLayout(app, n));
 app.LastSize = size;
 rtrn 0;
}

sttc ::llc::err_t nnShowNetwork(SApplication & app, ::llc::u2_t n, bool show) {
 auto & gui = *app.Framework.GUI;
 auto & session = app.Sessions[n];
 if(show && !session.Network.State) {
  if_fail_fe(::llc::nnBegin(session.Network, n));
  if_fail_fe(::llc::nnStep(session.Network));
 }
 for(::llc::u2_t p = 0; p < NN_PANES; ++p)
  gui.Controls.SetHidden(app.Viewports[n * NN_PANES + p].IdControl, !show || (p && !session.Open[p - 1]));
 gui.Controls.SetSelected(app.Choices[n], show);
 if(show) if_fail_fe(::llc::viewportActivate(gui, app.Desktop, app.Viewports, (::llc::u0_t)(n * NN_PANES)));
 rtrn 0;
}

::llc::err_t setup(SApplication & app) {
#if defined(_DEBUG)
 extern ::llc::err_t nnVerify();
 extern ::llc::err_t nnVerifyHeatmap();
 if_fail_fe(nnVerify());
 if_fail_fe(nnVerifyHeatmap());
 app.Capture = true;
#endif
 auto & window = app.Framework.RootWindow;
 auto & gui = *app.Framework.GUI;
 window.Size = {1600, 960};
 if_null_ve(::llc::OS_NO_MEMORY, window.BackBuffer.create());
 if_fail_fe(window.BackBuffer->resize(window.Size.u2()));
 if_fail_fe(::llc::mainWindowCreate(window, app.Framework.RuntimeValues.PlatformDetail, window.Input));
 if_fail_fe(::sampleGUIResourcesCreate(gui));
 // Palette from the existing LLS workspace, applied to the sample controls.
 for(::llc::u2_t p = 0; p < gui.Colors->Palettes.size(); ++p) {
  auto & palette = gui.Colors->Palettes[p];
  const auto state = p % ::llc::GUI_CONTROL_PALETTE_COUNT;
  for(auto & color : palette) color = {34, 27, 23, 255};
  palette[::llc::UI_CONTROL_AREA_CLIENT] = state == ::llc::GUI_CONTROL_PALETTE_SELECTED || state == ::llc::GUI_CONTROL_PALETTE_SELECTED_HOVER
   ? ::llc::bgra{90, 66, 24, 255} : state == ::llc::GUI_CONTROL_PALETTE_HOVER ? ::llc::bgra{62, 49, 34, 255} : ::llc::bgra{42, 33, 27, 255};
  palette[::llc::UI_CONTROL_AREA_TEXT_FACE] = {231, 231, 222, 255};
  palette[::llc::UI_CONTROL_AREA_BORDER_LEFT] = palette[::llc::UI_CONTROL_AREA_BORDER_TOP]
   = palette[::llc::UI_CONTROL_AREA_BORDER_RIGHT] = palette[::llc::UI_CONTROL_AREA_BORDER_BOTTOM] = {87, 68, 43, 255};
 }
 gui.SelectedFont = 7; gui.ColorModeDefault = ::llc::GUI_COLOR_MODE_Flat;

 app.Desktop = ::llc::controlCreate(gui); if_fail_fe(app.Desktop);
 gui.Controls.Draw[app.Desktop].NoBorder = true; gui.Controls.Draw[app.Desktop].NoClient = true;
 gui.Controls.Constraints[app.Desktop].AttachSizeToControl = {app.Desktop, app.Desktop};
 ::sampleGUIControlThemeApply(gui, app.Desktop, 0);
 if_fail_fe(nnControl(app, app.Desktop, app.Title, "NEURAL LAB / NETWORK WORKSPACE"));
 if_fail_fe(nnControl(app, app.Desktop, app.Restore, "ARRANGE"));
 if_fail_fe(nnControl(app, app.Desktop, app.Save, "SAVE VIEW"));
 if_fail_fe(nnControl(app, app.Desktop, app.Exit, "EXIT"));
 gui.Controls.Placement[app.Restore].Align = gui.Controls.Placement[app.Save].Align = gui.Controls.Placement[app.Exit].Align = ::llc::ALIGN_TOP_RIGHT;
 // Right-aligned controls use distance from the right edge.
 const ::llc::vcst_t signals[] = {"Digit pixels", "Resonating pattern", "Phone bits", "Tour assignment", "Input window", "Rotation pixels", "Recalled pixels", "Learned forces", "Original image"};
 const ::llc::vcst_t titles[] = {"State", "Weights", "Weight changes", "Before step", "History", "Layer outputs", "Backprop errors"};
 const ::llc::vcst_t imageTitles[] = {"Original image", "Reconstruction", "RGB error x4", "Neuron removal x4", "Full-image MSE", "Layer outputs", "Backprop errors"};
 for(::llc::u2_t n = 0; n < ::llc::NN_COUNT; ++n) {
  auto & session = app.Sessions[n];
  if_fail_fe(nnControl(app, app.Desktop, app.Choices[n], ::llc::nnName(n)));
  for(::llc::u2_t p = 0; p < NN_PANES; ++p) {
   const auto index = n * NN_PANES + p;
   auto & viewport = app.Viewports[index];
   if_fail_fe(::llc::viewportInitialize(gui, viewport));
   if_fail_fe(::llc::controlSetParent(gui, viewport.IdControl, app.Desktop));
   const auto name = ::llc::nnName(n);
   const auto label = p ? (n == ::llc::NN_IMAGE ? imageTitles[p - 1] : p == 1 ? signals[n] : titles[p - 1]) : ::llc::vcst_t{"NETWORK"};
   const auto length = ::llc::sprintf_s(app.Captions[index].Storage, "%.*s / %.*s", (int)name.size(), name.begin(), (int)label.size(), label.begin());
   if_fail_fe(length);
   if_fail_fe(::llc::controlTextSet(gui, viewport.IdControls[::llc::VIEWPORT_CONTROL_TITLE], {app.Captions[index].Storage, (::llc::u2_t)length}));
   ::sampleGUIControlThemeApply(gui, viewport.IdControl, 0);
   for(auto id : viewport.IdControls) ::sampleGUIControlThemeApply(gui, id, 0);
   gui.Controls.SetHidden(viewport.IdControl, true);
  }
  const auto parent = app.Viewports[n * NN_PANES].IdControls[::llc::VIEWPORT_CONTROL_TARGET];
  if_fail_fe(nnControl(app, parent, session.Description, ::llc::nnDescription(n)));
  if_fail_fe(nnControl(app, parent, session.Run, "RUN"));
  if_fail_fe(nnControl(app, parent, session.Step, "STEP"));
  if_fail_fe(nnControl(app, parent, session.Reset, "RESET"));
  if_fail_fe(nnControl(app, parent, session.SpeedLabel, "Steps/update"));
  session.Speed.ValueLimits = {1, 256}; session.Speed.ValuePage = 8;
  if_fail_fe(::sampleSliderCreate(session.Speed, gui, parent, {{360, 30}, {180, 28}}, false, 8));
  gui.Controls.Placement[session.Speed.IdThumb].Area.Size.x = 42;
  ::sampleGUIControlThemeApply(gui, session.Speed.IdRoot, 0); ::sampleGUIControlThemeApply(gui, session.Speed.IdThumb, 0);
  if_fail_fe(::sampleCheckBoxCreate(session.AutoScale, gui, parent, {{4, 64}, {24, 24}}));
  ::sampleGUIControlThemeApply(gui, session.AutoScale.IdControl, 0);
  if_fail_fe(::sampleCheckBoxSetChecked(session.AutoScale, gui, true));
  if_fail_fe(nnControl(app, parent, session.ScaleLabel, n == ::llc::NN_IMAGE ? ::llc::vcst_t{"RGB | fixed-weight previews | error/removal amplified x4"} : ::llc::vcst_t{"Auto scale | cyan positive / amber negative"}));
  if(n == ::llc::NN_IMAGE) gui.Controls.SetHidden(session.AutoScale.IdControl, true);
  if_fail_fe(nnControl(app, parent, session.Layout, n == ::llc::NN_IMAGE ? ::llc::vcst_t{"LOAD PNG"} : ::llc::vcst_t{"State grid: native"}));
  if_fail_fe(nnControl(app, parent, session.CellLabel, n == ::llc::NN_IMAGE ? ::llc::vcst_t{"Neuron index"} : ::llc::vcst_t{"Cell px: 0=fit"}));
  session.CellSize.ValueLimits = {0, n == ::llc::NN_IMAGE ? 9 : 32}; session.CellSize.ValuePage = 4;
  if_fail_fe(::sampleSliderCreate(session.CellSize, gui, parent, {{336, 94}, {180, 26}}, false, 0));
  gui.Controls.Placement[session.CellSize.IdThumb].Area.Size.x = 42;
  ::sampleGUIControlThemeApply(gui, session.CellSize.IdRoot, 0); ::sampleGUIControlThemeApply(gui, session.CellSize.IdThumb, 0);
  if(n == 4 || n == 7 || n == ::llc::NN_IMAGE) {
   if_fail_fe(nnControl(app, parent, session.UnitsLabelX, n != 7 ? ::llc::vcst_t{"Hidden neurons"} : ::llc::vcst_t{"Map columns"}));
   session.UnitsX.ValueLimits = {1, n != 7 ? 32 : 25}; session.UnitsX.ValuePage = 4;
   if_fail_fe(::sampleSliderCreate(session.UnitsX, gui, parent, {{4, 148}, {180, 26}}, false, n != 7 ? 10 : 25));
   gui.Controls.Placement[session.UnitsX.IdThumb].Area.Size.x = 42;
   ::sampleGUIControlThemeApply(gui, session.UnitsX.IdRoot, 0); ::sampleGUIControlThemeApply(gui, session.UnitsX.IdThumb, 0);
   if(n == 7) {
    if_fail_fe(nnControl(app, parent, session.UnitsLabelY, "Map rows"));
    session.UnitsY.ValueLimits = {1, 25}; session.UnitsY.ValuePage = 4;
    if_fail_fe(::sampleSliderCreate(session.UnitsY, gui, parent, {{200, 148}, {180, 26}}, false, 25));
    gui.Controls.Placement[session.UnitsY.IdThumb].Area.Size.x = 42;
    ::sampleGUIControlThemeApply(gui, session.UnitsY.IdRoot, 0); ::sampleGUIControlThemeApply(gui, session.UnitsY.IdThumb, 0);
   }
   if_fail_fe(nnControl(app, parent, session.ApplySize, "APPLY + RESET"));
  }
  for(::llc::u2_t i = 0; i < (n == 4 || n == ::llc::NN_IMAGE ? 7U : 5U); ++i)
   if_fail_fe(nnControl(app, parent, session.Inspectors[i], n == ::llc::NN_IMAGE ? imageTitles[i] : i ? titles[i] : signals[n]));
  if_fail_fe(nnControl(app, parent, session.Preview, n == ::llc::NN_IMAGE ? ::llc::vcst_t{"Reconstruction / progressive preview"} : signals[n]));
  if_fail_fe(nnControl(app, parent, session.Trace, n == ::llc::NN_IMAGE ? ::llc::vcst_t{"Full-image MSE / completed passes"} : n == 4 ? ::llc::vcst_t{"Sample loss"} : ::llc::vcst_t{"Metric history"}));
  if_fail_fe(nnControl(app, parent, session.Status, ""));
  gui.Controls.Placement[session.Status].Align = ::llc::ALIGN_BOTTOM_LEFT;
  gui.Controls.Text[session.Preview].Align = gui.Controls.Text[session.Trace].Align = ::llc::ALIGN_TOP_LEFT;
 }
 if_fail_fe(nnWorkspaceLayout(app, true));
 if_fail_fe(nnShowNetwork(app, ::llc::NN_IMAGE, true));
 app.Sessions[::llc::NN_IMAGE].Running = ::llc::argsOptionIndex(app.Framework.RuntimeValues.EntryPointArgs, "learn") >= 0;
 if_fail_fe(::llc::sprintf_s(app.Sessions[::llc::NN_IMAGE].ImageMessage.Storage, "Built-in RGB field 64x64 | LOAD PNG to use your image"));
 if(window.Size.x >= 1200 && window.Size.y >= 700) {
  const auto n = ::llc::NN_IMAGE;
  if_fail_fe(nnViewportSize(app, n * NN_PANES, 12, app.WorkspaceTop, 700, window.Size.y - app.WorkspaceTop - 12));
  const int paneWidth = (window.Size.x - 748) / 2, paneHeight = (window.Size.y - app.WorkspaceTop - 24) / 2;
  for(::llc::u2_t p = 1; p <= 4; ++p) {
   if_fail_fe(nnViewportSize(app, n * NN_PANES + p, 724 + ((p - 1) % 2) * (paneWidth + 8), app.WorkspaceTop + ((p - 1) / 2) * (paneHeight + 8), paneWidth, paneHeight));
   app.Sessions[n].Open[p - 1] = true;
   gui.Controls.SetHidden(app.Viewports[n * NN_PANES + p].IdControl, false);
   gui.Controls.SetSelected(app.Sessions[n].Inspectors[p - 1], true);
  }
  if_fail_fe(nnSessionLayout(app, n));
 }
 rtrn ::llc::guiUpdateMetrics(gui, window.Size, false);
}

::llc::err_t cleanup(SApplication & app) {
 for(auto & session : app.Sessions) if_fail_fe(::llc::nnReset(session.Network));
 rtrn ::llc::mainWindowDestroy(app.Framework.RootWindow);
}

sttc ::llc::err_t nnLoadImage(SApplication & app, SNNSession & session) {
#ifdef LLC_WINDOWS
 char path[4096] = {};
 OPENFILENAMEA dialog = {};
 dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = app.Framework.RootWindow.PlatformDetail.WindowHandle;
 dialog.lpstrFilter = "PNG image\0*.png\0\0"; dialog.lpstrFile = path; dialog.nMaxFile = sizeof(path);
 dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
 if(!GetOpenFileNameA(&dialog)) rtrn CommDlgExtendedError() ? -1 : 0;
 const auto bytes = ::llc::fileSize(path);
 if_true_fe(bytes < 24 || bytes > 8 * 1024 * 1024);
 ::llc::au0_t encoded;
 if_fail_fe(::llc::fileToMemory(path, encoded, 8 * 1024 * 1024));
 if_true_fe(encoded.size() < 24);
 const ::llc::u0_t signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
 for(::llc::u2_t i = 0; i < 8; ++i) if_true_fe(encoded[i] != signature[i]);
 ::llc::n2u2_t size = {};
 for(::llc::u2_t i = 0; i < 4; ++i) { size.x = (size.x << 8) | encoded[16 + i]; size.y = (size.y << 8) | encoded[20 + i]; }
 if_true_fe(!size.x || !size.y || size.x > 1024 || size.y > 1024);
 ::llc::img8bgra decoded, reduced;
 if_fail_fe(::llc::pngFileLoad(encoded, decoded));
 const double scale = ::llc::min(1.0, 64.0 / ::llc::max(size.x, size.y));
 if_fail_fe(reduced.resize(::llc::n2u2_t{::llc::max(1U, (::llc::u2_t)(size.x * scale)), ::llc::max(1U, (::llc::u2_t)(size.y * scale))}));
 for(::llc::u2_t y = 0; y < reduced.metrics().y; ++y) for(::llc::u2_t x = 0; x < reduced.metrics().x; ++x) {
  auto color = decoded[y * size.y / reduced.metrics().y][x * size.x / reduced.metrics().x];
  color.r = (::llc::u0_t)((color.r * color.a + 255 * (255 - color.a)) / 255);
  color.g = (::llc::u0_t)((color.g * color.a + 255 * (255 - color.a)) / 255);
  color.b = (::llc::u0_t)((color.b * color.a + 255 * (255 - color.a)) / 255); color.a = 255;
  reduced[y][x] = color;
 }
 if_fail_fe(::llc::nnImageSource(session.Network, reduced.View));
 session.Running = false; session.HistoryCount = session.HistoryCursor = 0; session.LastImagePass = 0;
 if_fail_fe(::llc::sprintf_s(session.ImageMessage.Storage, "PNG %ux%u -> %ux%u; alpha over white", size.x, size.y, reduced.metrics().x, reduced.metrics().y));
 if_fail_fe(::sampleSliderSetValue(session.CellSize, *app.Framework.GUI, 0));
 rtrn 1;
#else
 (void)app; (void)session; rtrn -1;
#endif
}

::llc::err_t update(SApplication & app, bool systemRequestedExit) {
 if(systemRequestedExit) rtrn ::llc::APPLICATION_STATE_EXIT;
 const auto frameworkResult = ::llc::updateFramework(app.Framework);
 if(frameworkResult) rtrn frameworkResult;
 auto & gui = *app.Framework.GUI;
 auto & window = app.Framework.RootWindow;
 if(window.Closed || window.Input->KeyDown(::llc::SYSTEM_KEY_Escape) || gui.Controls.IsAction(app.Exit)) rtrn ::llc::APPLICATION_STATE_EXIT;
 if(window.Size != app.LastSize || gui.Controls.IsAction(app.Restore)) if_fail_fe(nnWorkspaceLayout(app, gui.Controls.IsAction(app.Restore)));
 if(gui.Controls.IsAction(app.Save)) app.Capture = true;
 for(::llc::u2_t n = 0; n < ::llc::NN_COUNT; ++n) {
  auto & session = app.Sessions[n];
  auto & run = session.Network;
  const auto root = app.Viewports[n * NN_PANES].IdControl;
  if(gui.Controls.IsAction(app.Choices[n])) if_fail_fe(nnShowNetwork(app, n, gui.Controls.IsHidden(root)));
  const bool visible = !gui.Controls.IsHidden(root) && !app.Viewports[n * NN_PANES].Folded;
  bool step = false;
  if(visible) {
   if_fail_fe(::sampleSliderUpdate(session.Speed, gui, *window.Input));
   if_fail_fe(::sampleCheckBoxUpdate(session.AutoScale, gui));
   if_fail_fe(::sampleSliderUpdate(session.CellSize, gui, *window.Input));
   if(n == ::llc::NN_IMAGE && gui.Controls.IsAction(session.Layout)) {
    const auto loaded = nnLoadImage(app, session);
    if(loaded < 0) if_fail_fe(::llc::sprintf_s(session.ImageMessage.Storage, "PNG load failed: use <=1024x1024, <=8 MiB"));
   }
   if(n == ::llc::NN_IMAGE && run.ImageNeuron != (::llc::u2_t)session.CellSize.ValueCurrent)
    if_fail_fe(::llc::nnImageInspect(run, (::llc::u2_t)session.CellSize.ValueCurrent));
   if(n != ::llc::NN_IMAGE && gui.Controls.IsAction(session.Layout)) {
    session.DisplayLayout = (session.DisplayLayout + 1) % 3;
    const ::llc::vcst_t layouts[] = {"State grid: native", "State grid: strip", "State grid: square"};
    if_fail_fe(::llc::controlTextSet(gui, session.Layout, layouts[session.DisplayLayout]));
   }
   if(n == 4 || n == 7 || n == ::llc::NN_IMAGE) {
    if_fail_fe(::sampleSliderUpdate(session.UnitsX, gui, *window.Input));
    if(n == 7) if_fail_fe(::sampleSliderUpdate(session.UnitsY, gui, *window.Input));
    const bool pending = n != 7 ? run.Config.HiddenUnits != (::llc::u2_t)session.UnitsX.ValueCurrent
     : run.Config.MapSize.x != (::llc::u2_t)session.UnitsX.ValueCurrent || run.Config.MapSize.y != (::llc::u2_t)session.UnitsY.ValueCurrent;
    gui.Controls.SetSelected(session.ApplySize, pending);
    if(gui.Controls.IsAction(session.ApplySize)) {
     auto config = run.Config;
     if(n != 7) config.HiddenUnits = (::llc::u2_t)session.UnitsX.ValueCurrent;
     else config.MapSize = {(::llc::u2_t)session.UnitsX.ValueCurrent, (::llc::u2_t)session.UnitsY.ValueCurrent};
     if_fail_fe(::llc::nnBegin(run, n, config));
     session.Running = false; session.HistoryCount = session.HistoryCursor = 0;
     if(n == ::llc::NN_IMAGE) {
      session.LastImagePass = 0; session.CellSize.ValueLimits.Max = config.HiddenUnits - 1;
      if_fail_fe(::sampleSliderSetValue(session.CellSize, gui, 0));
     }
    }
   }
   if(gui.Controls.IsAction(session.Reset)) {
    session.Running = false; session.HistoryCount = session.HistoryCursor = 0;
    if_fail_fe(::llc::nnBegin(run, n, run.Config));
    if(n == ::llc::NN_IMAGE) { session.LastImagePass = 0; if_fail_fe(::sampleSliderSetValue(session.CellSize, gui, 0)); }
   }
   if(gui.Controls.IsAction(session.Run) && !run.Complete && !run.Error) session.Running = !session.Running;
   step = gui.Controls.IsAction(session.Step);
   if(step) session.Running = false;
   for(::llc::u2_t p = 0; p < (n == 4 || n == ::llc::NN_IMAGE ? 7U : 5U); ++p) {
    if(!gui.Controls.IsAction(session.Inspectors[p])) continue;
    session.Open[p] = !session.Open[p];
    const auto index = n * NN_PANES + p + 1;
    auto & viewport = app.Viewports[index];
    gui.Controls.SetHidden(viewport.IdControl, !session.Open[p]);
    gui.Controls.SetSelected(session.Inspectors[p], session.Open[p]);
    if(session.Open[p]) {
     if(viewport.Folded) if_fail_fe(::llc::viewportFold(gui, viewport));
     if_fail_fe(::llc::viewportActivate(gui, app.Desktop, app.Viewports, (::llc::u0_t)index));
    }
   }
  }
  if((session.Running || step) && run.State && !run.Complete && !run.Error) {
   const auto budget = step ? 1U : (::llc::u2_t)session.Speed.ValueCurrent;
   for(::llc::u2_t unit = 0; unit < budget; ++unit) {
    const auto result = ::llc::nnStep(run, 1);
    if(result) session.Running = false;
    const auto metric = run.Inspected ? run.SampleLoss : run.Metric;
    if(n != ::llc::NN_IMAGE && result >= 0 && std::isfinite(metric)) {
     session.History[session.HistoryCursor] = metric;
     session.HistoryCursor = (session.HistoryCursor + 1) % session.History.size();
     session.HistoryCount = ::llc::min(session.HistoryCount + 1, session.History.size());
    }
    if(result) break;
   }
  }
  if(n == ::llc::NN_IMAGE && run.State) {
   if(session.ImageMessage[0]) if_fail_fe(::llc::controlTextSet(gui, session.Description, session.ImageMessage.Storage));
   if_fail_fe(::llc::nnImagePreview(run));
   if(run.ImagePass != session.LastImagePass) {
    session.LastImagePass = run.ImagePass;
    session.History[session.HistoryCursor] = run.ImageMSE;
    session.HistoryCursor = (session.HistoryCursor + 1) % session.History.size();
    session.HistoryCount = ::llc::min(session.HistoryCount + 1, session.History.size());
   }
  }
  if_fail_fe(::llc::controlTextSet(gui, session.Run, session.Running ? ::llc::vcst_t{"PAUSE"} : ::llc::vcst_t{"RUN"}));
  const char * state = run.Error ? "ERROR" : run.Complete ? "COMPLETE" : session.Running ? "RUNNING" : "PAUSED";
  const auto length = n == ::llc::NN_IMAGE
   ? ::llc::sprintf_s(session.StatusText.Storage, "%s | samples %" LLC_FMT_U3 " | MSE %.5g | preview %u/%u", state, run.ImageSamples, run.ImageMSE, run.ImageCursor, run.ImageSource.size())
   : ::llc::sprintf_s(session.StatusText.Storage, "%s | step %" LLC_FMT_U3 " | metric %.6g", state, run.Steps, run.Metric);
  if_fail_fe(length);
  if_fail_fe(::llc::controlTextSet(gui, session.Status, {session.StatusText.Storage, (::llc::u2_t)length}));
 }
 bool doubleClick = false;
 if_fail_fe(::sampleGUIViewportDoubleClick(window.EventQueue, doubleClick));
 const ::llc::n2s1_t delta = {::llc::s1_t(window.Input->MouseCurrent.Deltas.x), ::llc::s1_t(window.Input->MouseCurrent.Deltas.y)};
 for(::llc::u2_t i = 0; i < app.Viewports.size(); ++i) {
  auto & v = app.Viewports[i];
  if(gui.Controls.IsHidden(v.IdControl)) continue;
  const auto title = v.IdControls[::llc::VIEWPORT_CONTROL_TITLE];
  if(::llc::viewportIsPressed(gui, v)) if_fail_fe(::llc::viewportActivate(gui, app.Desktop, app.Viewports, (::llc::u0_t)i));
  if(gui.Controls.IsAction(v.IdControls[::llc::VIEWPORT_CONTROL_CLOSE])) {
   if(i % NN_PANES) {
    app.Sessions[i / NN_PANES].Open[i % NN_PANES - 1] = false;
    gui.Controls.SetSelected(app.Sessions[i / NN_PANES].Inspectors[i % NN_PANES - 1], false);
    gui.Controls.SetHidden(v.IdControl, true);
   } else if_fail_fe(nnShowNetwork(app, i / NN_PANES, false));
  }
  auto area = gui.Controls.Placement[v.IdControl].Area;
  if(gui.Controls.IsPressed(title) && (delta.x || delta.y)) {
   area.Offset += delta;
   area.Offset.x = (::llc::s1_t)::llc::max(0, ::llc::min(int(area.Offset.x), int(window.Size.x) - 80));
   area.Offset.y = (::llc::s1_t)::llc::max(int(app.WorkspaceTop), ::llc::min(int(area.Offset.y), int(window.Size.y) - 30));
   if_fail_fe(nnViewportSize(app, i, area.Offset.x, area.Offset.y, area.Size.x, v.UnfoldedSize.y));
  }
  if(!v.Folded && gui.Controls.IsPressed(v.IdControls[::llc::VIEWPORT_CONTROL_RESIZE_BOTTOM_RIGHT]) && (delta.x || delta.y)) {
   const int minWidth = i % NN_PANES ? 240 : 520, minHeight = i % NN_PANES ? 200 : 460;
   const int width = ::llc::min(::llc::max(minWidth, int(area.Size.x) + delta.x), int(window.Size.x) - area.Offset.x - 4);
   const int height = ::llc::min(::llc::max(minHeight, int(area.Size.y) + delta.y), int(window.Size.y) - area.Offset.y - 4);
   if_fail_fe(nnViewportSize(app, i, area.Offset.x, area.Offset.y, width, height));
  }
  if(doubleClick && gui.Controls.IsHovered(title)) if_fail_fe(::llc::viewportFold(gui, v));
 }
 for(::llc::u2_t n = 0; n < ::llc::NN_COUNT; ++n) if_fail_fe(nnSessionLayout(app, n));
 if_fail_fe(::llc::guiUpdateMetrics(gui, window.Size, false));
 ::llc::sleep(1);
 rtrn 0;
}
sttc ::llc::err_t nnHeatmap(::llc::g8bgra target, ::llc::view<const ::llc::f3_t> values, ::llc::n2u2_t cells, bool autoScale, ::llc::u2_t cellPixels) {
 if_fail_fe(target.fill(::llc::bgra{22, 18, 16}));
 if(!cells.x || !cells.y || !values.size()) rtrn 0;
 double scale = 1;
 if(autoScale) { scale = 0.000001; for(auto v : values) if(std::isfinite(v)) scale = ::llc::max(scale, std::abs(v)); }
 const auto size = target.metrics();
 if(size.x < 48 || size.y < 100) rtrn 0;
 const double cell = cellPixels ? double(cellPixels) : ::llc::min(double(size.x - 24) / cells.x, double(size.y - 96) / cells.y);
 // Keep thin strips visible while subsampling their long axis to fit.
 const double cellX = !cellPixels && cells.x <= size.x - 24 ? ::llc::max(1.0, cell) : cell;
 const double cellY = !cellPixels && cells.y <= size.y - 96 ? ::llc::max(1.0, cell) : cell;
 const double originX = (size.x - cellX * cells.x) / 2, originY = 64 + (size.y - 64 - cellY * cells.y) / 2;
 for(::llc::u2_t py = 64; py < size.y; ++py)
 for(::llc::u2_t px = 0; px < size.x; ++px) {
  const double gx = (px + 0.5 - originX) / cellX, gy = (py + 0.5 - originY) / cellY;
  if(gx < 0 || gy < 0 || gx >= cells.x || gy >= cells.y) continue;
  const auto x = (::llc::u2_t)gx, y = (::llc::u2_t)gy;
  if(cell >= 4 && ((gx - x) * cell < 1 || (gy - y) * cell < 1)) continue;
  const auto index = y * cells.x + x;
  if(index >= values.size()) continue;
  const auto value = std::isfinite(values[index]) ? values[index] : 0;
  const auto magnitude = ::llc::min(1.0, std::abs(value) / scale);
  const ::llc::bgra tone = value >= 0 ? ::llc::bgra{244, 202, 70} : ::llc::bgra{38, 138, 244};
  const ::llc::bgra color = tone * (0.12 + 0.88 * magnitude);
  target[py][px] = color;
 }
 rtrn 0;
}

::llc::err_t nnVerifyHeatmap() {
 ::llc::img8bgra image;
 ::llc::astatic<::llc::f3_t, 625> values = {};
 for(::llc::u2_t i = 0; i < values.size(); ++i) values[i] = i < 312 ? -1 : 1;
 const ::llc::n2u2_t targets[] = {{1, 1}, {48, 100}, {240, 200}};
 const ::llc::n2u2_t grids[] = {{25, 25}, {625, 1}, {1, 625}, {3, 7}};
 for(const auto size : targets) {
  if_fail_fe(image.resize(size));
  for(const auto cells : grids)
   for(const ::llc::u2_t zoom : {0U, 1U, 32U}) if_fail_fe(nnHeatmap(image.View, values, cells, true, zoom));
 }
 // Fit must retain both ends of a dense strip in the visible image.
 if_fail_fe(nnHeatmap(image.View, values, {625, 1}, true, 0));
 bool positive = false, negative = false;
 for(const auto color : image.Texels) {
  positive |= color.b == 244 && color.r == 70;
  negative |= color.r == 244 && color.b == 38;
 }
 if_true_fe(!positive || !negative);
 rtrn 0;
}

sttc ::llc::err_t nnHistory(SNNSession & session, ::llc::g8bgra target) {
 if_fail_fe(target.fill(::llc::bgra{22, 18, 16}));
 const auto size = target.metrics();
 if(size.x < 48 || size.y < 40) rtrn 0;
 for(::llc::u2_t x = 24; x < size.x; x += 64) if_fail_fe(::llc::drawLineVertical(target, ::llc::bgra{48, 40, 32}, x, 12, size.y - 12));
 for(::llc::u2_t y = 12; y < size.y; y += 32) if_fail_fe(::llc::drawLineHorizontal(target, ::llc::bgra{48, 40, 32}, y, 24, size.x - 12));
 if(session.HistoryCount < 2) rtrn 0;
 auto minimum = session.History[(session.HistoryCursor + session.History.size() - session.HistoryCount) % session.History.size()], maximum = minimum;
 for(::llc::u2_t i = 0; i < session.HistoryCount; ++i) {
  const auto value = session.History[(session.HistoryCursor + session.History.size() - session.HistoryCount + i) % session.History.size()];
  minimum = ::llc::min(minimum, value); maximum = ::llc::max(maximum, value);
 }
 const auto range = maximum > minimum ? maximum - minimum : 1.0;
 ::llc::n2s2_t previous = {};
 for(::llc::u2_t i = 0; i < session.HistoryCount; ++i) {
  const auto value = session.History[(session.HistoryCursor + session.History.size() - session.HistoryCount + i) % session.History.size()];
  const ::llc::n2s2_t point = {24 + int((size.x - 48) * i / (session.HistoryCount - 1)), int(size.y - 16 - (size.y - 32) * (value - minimum) / range)};
  if(i) if_fail_fe(::llc::drawLine(target, ::llc::bgra{244, 202, 70}, ::llc::line2s2_t{previous, point}));
  previous = point;
 }
 rtrn 0;
}


sttc ::llc::err_t nnImageSet(::llc::SGUI & gui, ::llc::cid_t id, ::llc::img8bgra & image) {
 gui.Controls.Images[id].Image = image.View;
 gui.Controls.Images[id].ImageAlign = ::llc::ALIGN_CENTER;
 rtrn 0;
}

sttc ::llc::err_t nnPicture(::llc::g8bgra target, ::llc::gc8bgra source) {
 if_fail_fe(target.fill(::llc::bgra{22, 18, 16, 255}));
 const auto size = target.metrics(), pixels = source.metrics();
 if(size.x < 24 || size.y < 80 || !pixels.x || !pixels.y) rtrn 0;
 const double scale = ::llc::min(double(size.x - 16) / pixels.x, double(size.y - 64) / pixels.y);
 const ::llc::n2u2_t extent = {::llc::max(1U, (::llc::u2_t)(pixels.x * scale)), ::llc::max(1U, (::llc::u2_t)(pixels.y * scale))};
 const ::llc::n2u2_t origin = {(size.x - extent.x) / 2, 56 + (size.y - 56 - extent.y) / 2};
 for(::llc::u2_t y = 0; y < extent.y; ++y) for(::llc::u2_t x = 0; x < extent.x; ++x)
  target[origin.y + y][origin.x + x] = source[y * pixels.y / extent.y][x * pixels.x / extent.x];
 rtrn 0;
}

::llc::err_t draw(SApplication & app) {
 auto & window = app.Framework.RootWindow;
 if(window.NoDraw) rtrn 0;
 if_fail_fe(window.BackBuffer->resize(window.Size.u2()));
 if_fail_fe(window.BackBuffer->Color.View.fill(::llc::bgra{25, 20, 17, 255}));
 auto & gui = *app.Framework.GUI;
 for(::llc::u2_t n = 0; n < ::llc::NN_COUNT; ++n) {
  auto & session = app.Sessions[n];
  auto & run = session.Network;
  if(!run.State) continue;
  for(::llc::u2_t p = 0; p < NN_PANES; ++p) {
   const auto index = n * NN_PANES + p;
   const auto & viewport = app.Viewports[index];
   if(viewport.Folded || gui.Controls.IsHidden(viewport.IdControl)) continue;
   const auto client = p ? viewport.IdControls[::llc::VIEWPORT_CONTROL_TARGET] : session.Preview;
   const auto size = gui.Controls.Placement[client].Area.Size.u2();
   auto & image = app.Images[index];
   if(image.metrics() != size) if_fail_fe(image.resize(size));
   if(n == ::llc::NN_IMAGE && p <= 4) {
    const auto & picture = p == 1 ? run.ImageSource : p == 3 ? run.ImageError : p == 4 ? run.ImageInfluence : run.ImagePrediction;
    if_fail_fe(nnPicture(image.View, picture.View));
   }
   else if(p == 5) { if_fail_fe(nnHistory(session, image.View)); }
   else {
    ::llc::view<const ::llc::f3_t> values = {run.Values.begin(), run.Count};
    ::llc::n2u2_t cells = {run.Width, run.Width ? (run.Count + run.Width - 1) / run.Width : 0};
    if(p <= 1 && run.Count && session.DisplayLayout) {
     cells.x = session.DisplayLayout == 1 ? run.Count : (::llc::u2_t)std::ceil(std::sqrt(double(run.Count)));
     cells.y = (run.Count + cells.x - 1) / cells.x;
    }
    if(p == 2 || p == 3 || p == 4) {
     const auto & data = p == 2 ? run.Weights : p == 3 ? run.WeightChanges : run.WeightsBefore;
     cells = p == 4 ? run.BeforeSize : run.WeightSize;
     values = {data.begin(), cells.x * cells.y};
    } else if(p >= 6) {
     const auto & data = p == 6 ? run.Activations : run.ErrorTerms;
     values = {data.begin(), run.Inspected ? 96U : 0U}; cells = {32, 3};
    }
    if_fail_fe(nnHeatmap(image.View, values, cells, session.AutoScale.Checked, n == ::llc::NN_IMAGE ? 0U : (::llc::u2_t)session.CellSize.ValueCurrent));
   }
   if_fail_fe(nnImageSet(gui, client, image));
   if(p) {
    const char * hint = p == 1 ? "Current sample / state" : p == 2 ? "Weight crop / up to 32 x 32" : p == 3 ? "Last step: after - before" : p == 4 ? "Immediately before last step" : p == 5 ? (run.Inspected ? "Sample loss / last 512 steps" : "Metric / last 512 steps") : p == 6 ? "Rows: input / hidden / output" : run.Training ? "Errors: none / hidden / output" : "Evaluation: output error only";
    auto length = ::llc::sprintf_s(app.Details[index].Storage, "%s", hint);
    if(n == ::llc::NN_IMAGE && p <= 5) {
     const char * imageHint = p == 1 ? "Training source (max 64px); alpha over white" : p == 2 ? "RGB prediction; progressive fixed-weight pass" : p == 3 ? "Absolute RGB error x4 (clamped for display)" : p == 4 ? "Absolute RGB change removing neuron x4" : "Full-image MSE / completed preview passes";
     length = ::llc::sprintf_s(app.Details[index].Storage, "%s\nNeuron %u | pass %" LLC_FMT_U3 " | MSE %.5g", imageHint, run.ImageNeuron, run.ImagePass, run.ImageMSE);
    }
    if(p == 6)
     length = ::llc::sprintf_s(app.Details[index].Storage, "Units %u / %u / %u (bias excluded)\nTarget %.5f  output %.5f\nLoss %.6g | %s", run.LayerUnits[0], run.LayerUnits[1], run.LayerUnits[2], run.Target, run.Prediction, run.SampleLoss, run.Training ? "TRAIN" : "EVALUATE");
    if_fail_fe(length);
    gui.Controls.Text[client].FontSelected = 7;
    gui.Controls.Text[client].Align = ::llc::ALIGN_TOP_LEFT;
    if_fail_fe(::llc::controlTextSet(gui, client, {app.Details[index].Storage, (::llc::u2_t)length}));
   } else {
    const auto traceSize = gui.Controls.Placement[session.Trace].Area.Size.u2();
    if(session.TraceImage.metrics() != traceSize) if_fail_fe(session.TraceImage.resize(traceSize));
    if_fail_fe(nnHistory(session, session.TraceImage.View));
    if_fail_fe(nnImageSet(gui, session.Trace, session.TraceImage));
   }
  }
 }
 if_fail_fe(::llc::guiDraw(gui, window.BackBuffer->Color.View));
 if(app.Capture) {
  ::llc::au0_t png = {};
  if_fail_fe(::llc::pngFileWrite(window.BackBuffer->Color.View, png));
  if_fail_fe(::llc::fileFromMemory("NN_VIEW.png", png));
  app.Capture = false;
 }
 rtrn 0;
}
LLC_DEFINE_APPLICATION_ENTRY_POINT(SApplication, "Neural Networks at your Fingertips")
