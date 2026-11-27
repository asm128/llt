#include "llc_console.h"

#include "llc_app_impl.h"
#include "llc_apod_serialize.h"
#include "llc_cstring.h"
#include "llc_framework.h"
#include "llc_runtime.h"
#include "llc_system_key.h"

#include <limits.h>

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

stxp ::llc::n2s1_t sampleDragDelta(::llc::ALIGN align, ::llc::n2s1_t delta) {
	if(::llc::bit_false(align, ::llc::ALIGN_HCENTER) && ::llc::bit_true(align, ::llc::ALIGN_RIGHT))
		delta.x = -delta.x;
	if(::llc::bit_false(align, ::llc::ALIGN_VCENTER) && ::llc::bit_true(align, ::llc::ALIGN_BOTTOM))
		delta.y = -delta.y;
	rtrn delta;
}

static_assert(::sampleDragDelta(::llc::ALIGN_CENTER_BOTTOM, {3, 5}) == ::llc::n2s1_t{ 3, -5});
static_assert(::sampleDragDelta(::llc::ALIGN_TOP_RIGHT   , {3, 5}) == ::llc::n2s1_t{-3,  5});
static_assert(::sampleDragDelta(::llc::ALIGN_CENTER      , {3, 5}) == ::llc::n2s1_t{ 3,  5});
static_assert(::sampleDragDelta(::llc::ALIGN_BOTTOM_LEFT , {3, 5}) == ::llc::n2s1_t{ 3, -5});

sttc ::llc::err_t sampleVirtualKeyboardUpdate(SSampleGUIComponents & app, bool & dragCanceled) {
	dragCanceled = false;
	cnst ::llc::SInput & input = *app.Window.Input;
	cnst ::llc::cid_t idKeyboard = app.InputBox.VirtualKeyboard.IdRoot;
	cnst ::llc::cid_t idInputBox = app.InputBox.IdRoot;
	::llc::SControlPlacement & placement = app.GUI.Controls.Placement[idInputBox];
	if(input.ButtonUp(0)) {
		app.InputBoxDragging = false;
		app.InputBoxDragCanceled = false;
	}
	if(app.InputBoxDragging && (input.KeyDown(::llc::SYSTEM_KEY_Escape) || input.ButtonDown(1))) {
		placement.Area.Offset = app.InputBoxDragOrigin;
		app.InputBoxDragging = false;
		app.InputBoxDragCanceled = true;
		dragCanceled = true;
		rtrn ::llc::controlMetricsInvalidate(app.GUI, idInputBox);
	}
	if(false == app.InputBoxDragging && false == app.InputBoxDragCanceled && input.MouseCurrent.ButtonState[0] && app.GUI.Controls.IsPressed(idKeyboard)) {
		app.InputBoxDragOrigin = placement.Area.Offset;
		app.InputBoxDragging = true;
	}
	if(false == app.InputBoxDragging || (0 == input.MouseCurrent.Deltas.x && 0 == input.MouseCurrent.Deltas.y))
		rtrn 0;
	cnst ::llc::n2f3_t scale = app.GUI.Zoom.DPI * app.GUI.Zoom.ZoomLevel;
	if_zero_fef(scale.x && scale.y, "Invalid GUI scale:{%f, %f}.", scale.x, scale.y);
	cnst ::llc::n2s1_t delta = ::sampleDragDelta(placement.Align, ::llc::n2s2_t{input.MouseCurrent.Deltas.x, input.MouseCurrent.Deltas.y}.f3().GetScaled(1.0 / scale.x, 1.0 / scale.y).s1());
	placement.Area.Offset += delta;
	rtrn ::llc::controlMetricsInvalidate(app.GUI, idInputBox);
}

sttc ::llc::err_t sampleGUIComponentsDoubleClick(cnst ::llc::TQueueSystemEvent & events, bool & doubleClick) {
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

sttc bool sampleGUIComponentsPressed(cnst SSampleGUIComponents & app, cnst ::llc::SViewport & viewport) nxpt {
	rtrn app.GUI.Controls.IsPressed(viewport.IdControl)
		|| app.GUI.Controls.IsPressed(viewport.IdControls[::llc::VIEWPORT_CONTROL_TITLE])
		|| app.GUI.Controls.IsPressed(viewport.IdControls[::llc::VIEWPORT_CONTROL_TARGET])
		;
}

sttc ::llc::err_t sampleGUIComponentsActivate(SSampleGUIComponents & app, ::llc::u0_t iViewport) {
	if_true_fe(iViewport >= app.Viewports.size());
	cnst ::llc::cid_t idRoot = app.Viewports[iViewport].IdControl;
	::llc::acid & desktopChildren = app.GUI.Controls.Children[app.Desktop];
	if(0 == desktopChildren.size() || desktopChildren[desktopChildren.size() - 1] != idRoot) {
		cnst ::llc::err_t iChild = desktopChildren.find(idRoot);
		if(iChild >= 0)
			if_fail_fe(desktopChildren.remove((::llc::u2_t)iChild));
		if_fail_fe(desktopChildren.push_back(idRoot));
	}
	for(::llc::u0_t iOther = 0; iOther < app.Viewports.size(); ++iOther) {
		cnst bool active = iOther == iViewport;
		app.GUI.Controls.SetSelected(app.Viewports[iOther].IdControl, active);
		app.GUI.Controls.SetSelected(app.Viewports[iOther].IdControls[::llc::VIEWPORT_CONTROL_TITLE], active);
	}
	rtrn 0;
}

sttc void sampleGUIComponentsImageUpdate(::llc::img8bgra & image, ::llc::u0_t theme, ::llc::u2_t frameIndex) nxpt {
	cnst ::llc::SColorBGRA tone = ::sampleGUIThemeColor(theme);
	cnst ::llc::bgra background = ::sampleGUIShade(tone, 14);
	cnst ::llc::bgra grid = ::sampleGUIShade(tone, 9);
	cnst ::llc::bgra pulse = ::sampleGUIShade(tone, 2);
	for(::llc::u2_t y = 0; y < image.metrics().y; ++y)
	for(::llc::u2_t x = 0; x < image.metrics().x; ++x) {
		cnst bool onGrid = 0 == x % 32 || 0 == y % 24;
		cnst bool onPulse = ((x + y + frameIndex) % 160) < 3;
		image[y][x] = onPulse ? pulse : onGrid ? grid : background;
	}
}

sttc ::llc::err_t sampleGUIComponentsCreate(SSampleGUIComponents & app, ::llc::u0_t iViewport, ::llc::vcst_t title, cnst ::llc::rect2s1_t & area) {
	if_true_fe(iViewport >= app.Viewports.size());
	::llc::SViewport & viewport = app.Viewports[iViewport];
	if_fail_fe(::llc::viewportInitialize(app.GUI, viewport));
	if_fail_fe(::llc::controlSetParent(app.GUI, viewport.IdControl, app.Desktop));
	::llc::SControlPlacement & root = app.GUI.Controls.Placement[viewport.IdControl];
	root.Area = area;
	root.Border = {3, 3, 3, 3};
	root.Margin = {1, 1, 1, 1};
	cnst ::llc::cid_t idTitle = viewport.IdControls[::llc::VIEWPORT_CONTROL_TITLE];
	cnst ::llc::cid_t idClient = viewport.IdControls[::llc::VIEWPORT_CONTROL_TARGET];
	app.GUI.Controls.Text[idTitle].Text = title;
	app.GUI.Controls.Text[idTitle].FontSelected = 2;
	app.GUI.Controls.Placement[idTitle].Border = {2, 2, 2, 2};
	app.GUI.Controls.Placement[idClient].Area.Size = {(::llc::s1_t)(area.Size.x - 8), (::llc::s1_t)(area.Size.y - 32)};
	app.GUI.Controls.Placement[idClient].Border = {1, 1, 1, 1};
	::sampleGUIControlThemeApply(app.GUI, viewport.IdControl, iViewport);
	for(::llc::u0_t iControl = 0; iControl < viewport.IdControls.size(); ++iControl)
		::sampleGUIControlThemeApply(app.GUI, viewport.IdControls[iControl], iViewport);
	app.UnfoldedSize[iViewport] = area.Size;
	rtrn ::llc::controlMetricsInvalidate(app.GUI, viewport.IdControl);
}

sttc ::llc::err_t sampleGUIComponentsFold(SSampleGUIComponents & app, ::llc::u0_t iViewport) {
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

sttc ::llc::err_t sampleGUIComponentsPerformanceUpdate(SSampleGUIComponents & app, double secondsLastFrame) {
	if(secondsLastFrame <= 0)
		rtrn 0;
	cnst ::llc::err_t textLength = ::llc::sprintf_s(app.PerformanceText.Storage, "FPS %.2f    Frame %.2f ms", 1.0 / secondsLastFrame, secondsLastFrame * 1000.0);
	if_fail_fe(textLength);
	app.GUI.Controls.Text[app.Performance].Text = {app.PerformanceText.Storage, (::llc::u2_t)textLength};
	app.GUI.Controls.SetUpdated(app.Performance, false);
	rtrn 0;
}

sttc ::llc::cid_t sampleGUIComponentsControlCreate(SSampleGUIComponents & app, ::llc::vcst_t text, cnst ::llc::rect2s1_t & area) {
	cnst ::llc::cid_t idControl = ::llc::controlCreateChild(app.GUI, app.ComponentsPanel);
	if(idControl < 0)
		rtrn idControl;
	::llc::SControlPlacement & placement = app.GUI.Controls.Placement[idControl];
	placement.Area = area;
	placement.Margin = {1, 1, 1, 1};
	placement.Border = {2, 2, 2, 2};
	app.GUI.Controls.Text[idControl].Align = ::llc::ALIGN_CENTER;
	app.GUI.Controls.Text[idControl].FontSelected = 2;
	if(::llc::failed(::llc::controlTextSet(app.GUI, idControl, text))) {
		::llc::controlDelete(app.GUI, idControl);
		rtrn -1;
	}
	rtrn idControl;
}

sttc ::llc::err_t sampleGUIComponentsKeyboardAppend(SSampleGUIComponents & app) {
	if_fail_fe(app.ControlsKeyboard.push_back(app.CheckBox.IdControl));
	if_fail_fe(app.ControlsKeyboard.push_back(app.SliderHorizontal.IdRoot));
	if_fail_fe(app.ControlsKeyboard.push_back(app.SliderVertical.IdRoot));
	if_fail_fe(app.ControlsKeyboard.push_back(app.ControlEdit));
	for(::llc::u2_t iCharacter = 0; iCharacter < app.InputBox.VirtualKeyboard.Characters.size(); ++iCharacter)
		if_fail_fe(app.ControlsKeyboard.push_back(app.InputBox.VirtualKeyboard.Characters[iCharacter].IdControl));
	for(::llc::u2_t iCommand = 0; iCommand < app.InputBox.VirtualKeyboard.Commands.size(); ++iCommand)
		if_fail_fe(app.ControlsKeyboard.push_back(app.InputBox.VirtualKeyboard.Commands[iCommand].IdControl));
	rtrn app.ControlsKeyboard.push_back(app.ControlExit);
}

sttc void sampleGUIComponentsThemeApply(SSampleGUIComponents & app) nxpt {
	::sampleGUIControlThemeApply(app.GUI, app.CheckBox.IdControl, 0);
	::sampleGUIControlThemeApply(app.GUI, app.SliderHorizontal.IdRoot, 1);
	::sampleGUIControlThemeApply(app.GUI, app.SliderHorizontal.IdThumb, 1);
	::sampleGUIControlThemeApply(app.GUI, app.SliderVertical.IdRoot, 2);
	::sampleGUIControlThemeApply(app.GUI, app.SliderVertical.IdThumb, 2);
	::sampleGUIControlThemeApply(app.GUI, app.ControlEdit, 0);
	::sampleGUIControlThemeApply(app.GUI, app.ControlExit, 1);
	::sampleGUIControlThemeApply(app.GUI, app.InputBox.IdRoot, 2);
	::sampleGUIControlThemeApply(app.GUI, app.InputBox.IdText, 2);
	::sampleGUIControlThemeApply(app.GUI, app.InputBox.VirtualKeyboard.IdRoot, 2);
	for(::llc::u2_t iCharacter = 0; iCharacter < app.InputBox.VirtualKeyboard.Characters.size(); ++iCharacter)
		::sampleGUIControlThemeApply(app.GUI, app.InputBox.VirtualKeyboard.Characters[iCharacter].IdControl, 2);
	for(::llc::u2_t iCommand = 0; iCommand < app.InputBox.VirtualKeyboard.Commands.size(); ++iCommand)
		::sampleGUIControlThemeApply(app.GUI, app.InputBox.VirtualKeyboard.Commands[iCommand].IdControl, 2);
}

sttc ::llc::err_t sampleGUIComponentsControlsSetup(SSampleGUIComponents & app) {
	app.ComponentsPanel = ::llc::controlCreateChild(app.GUI, app.Desktop);
	if_true_fe(app.ComponentsPanel < 0);
	::llc::SControlPlacement & panelPlacement = app.GUI.Controls.Placement[app.ComponentsPanel];
	panelPlacement.Area = {{18, 18}, {500, 112}};
	panelPlacement.Align = ::llc::ALIGN_BOTTOM_LEFT;
	panelPlacement.Margin = {};
	panelPlacement.Border = {};
	app.GUI.Controls.Draw[app.ComponentsPanel].NoBorder = true;
	app.GUI.Controls.Draw[app.ComponentsPanel].NoClient = true;
	::sampleGUIControlThemeApply(app.GUI, app.ComponentsPanel, 2);

	if_fail_fe(::sampleCheckBoxCreate(app.CheckBox, app.GUI, app.ComponentsPanel, {{0, 0}, {32, 32}}));
	cnst ::llc::cid_t idCheckBoxLabel = ::sampleGUIComponentsControlCreate(app, "CHECK BOX | ENTER / SPACE", {{38, 0}, {212, 32}});
	if_true_fe(idCheckBoxLabel < 0);
	app.GUI.Controls.Draw[idCheckBoxLabel].NoBorder = true;
	app.GUI.Controls.Draw[idCheckBoxLabel].NoClient = true;
	app.GUI.Controls.Text[idCheckBoxLabel].Align = ::llc::ALIGN_CENTER_LEFT;
	::sampleGUIControlThemeApply(app.GUI, idCheckBoxLabel, 0);

	if_fail_fe(::sampleSliderCreate(app.SliderHorizontal, app.GUI, app.ComponentsPanel, {{0, 40}, {250, 30}}, false, 35));
	if_fail_fe(::sampleSliderCreate(app.SliderVertical, app.GUI, app.ComponentsPanel, {{260, 0}, {40, 108}}, true, 65));

	app.ControlEdit = ::sampleGUIComponentsControlCreate(app, "EDIT TEXT / CP437 KEYBOARD", {{310, 0}, {190, 32}});
	if_true_fe(app.ControlEdit < 0);
	app.ControlExit = ::sampleGUIComponentsControlCreate(app, "EXIT", {{310, 40}, {190, 32}});
	if_true_fe(app.ControlExit < 0);

	if_fail_fe(::llc::inputBoxCreate(app.InputBox, app.GUI, app.Desktop));
	app.GUI.Controls.Placement[app.InputBox.IdRoot].Align = ::llc::ALIGN_CENTER_BOTTOM;
	app.GUI.Controls.Placement[app.InputBox.IdRoot].Area.Offset.y = 70;
	app.GUI.Controls.Text[app.InputBox.IdText].FontSelected = 2;
	if_fail_fe(::sampleGUIComponentsKeyboardAppend(app));
	::sampleGUIComponentsThemeApply(app);
	rtrn 0;
}

::llc::err_t sampleGUIComponentsSetup(SSampleGUIComponents & app) {
	app.GUI.SelectedFont = 2;
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
	app.GUI.Controls.Placement[app.Help].Area = {{18, 18}, {1040, 42}};
	app.GUI.Controls.Placement[app.Help].Border = {2, 2, 2, 2};
	app.GUI.Controls.Placement[app.Help].Margin = {1, 1, 1, 1};
	app.GUI.Controls.Text[app.Help].Text = "DRAG / FOLD VIEWPORTS | TAB / SHIFT+TAB FOCUS | ENTER / SPACE ACTIVATE | ARROWS / PAGE / HOME / END SLIDE | ESC CLOSES / EXITS";
	app.GUI.Controls.Text[app.Help].Align = ::llc::ALIGN_CENTER_LEFT;
	app.GUI.Controls.Text[app.Help].FontSelected = 1;
	::sampleGUIControlThemeApply(app.GUI, app.Help, 2);
	if_fail_fe(::sampleGUIComponentsCreate(app, 0, "AZURE CPU SURFACE", {{80, 92}, {420, 250}}));
	if_fail_fe(::sampleGUIComponentsCreate(app, 1, "AMBER CPU SURFACE", {{408, 184}, {430, 280}}));
	if_fail_fe(::sampleGUIComponentsCreate(app, 2, "MONO CPU SURFACE", {{754, 104}, {360, 230}}));
	if_fail_fe(::sampleGUIComponentsActivate(app, 2));
	if_fail_fe(::sampleGUIComponentsControlsSetup(app));
	app.Performance = ::llc::controlCreate(app.GUI);
	if_true_fe(app.Performance < 0);
	app.GUI.Controls.Placement[app.Performance].Area = {{0, 14}, {360, 36}};
	app.GUI.Controls.Placement[app.Performance].Align = ::llc::ALIGN_CENTER_BOTTOM;
	app.GUI.Controls.Placement[app.Performance].Border = {1, 1, 1, 1};
	app.GUI.Controls.Placement[app.Performance].Margin = {1, 1, 1, 1};
	app.GUI.Controls.Text[app.Performance].Text = "FPS --    Frame -- ms";
	app.GUI.Controls.Text[app.Performance].FontSelected = 2;
	::sampleGUIControlThemeApply(app.GUI, app.Performance, 2);
	rtrn ::llc::guiUpdateMetrics(app.GUI, app.Window.Size, false);
}

::llc::err_t sampleGUIComponentsUpdate(SSampleGUIComponents & app, bool systemRequestedExit, double secondsLastFrame) {
	if(systemRequestedExit || app.Window.Closed)
		rtrn ::llc::APPLICATION_STATE_EXIT;
	cnst bool editingAtFrameStart = app.InputBox.Editing;
	if_fail_fe(::sampleCheckBoxUpdate(app.CheckBox, app.GUI));
	if_fail_fe(::sampleSliderUpdate(app.SliderHorizontal, app.GUI, *app.Window.Input));
	if_fail_fe(::sampleSliderUpdate(app.SliderVertical, app.GUI, *app.Window.Input));
	if(app.GUI.Controls.IsAction(app.ControlEdit))
		app.InputBox.Edit(app.GUI, true);
	if(editingAtFrameStart) {
		bool dragCanceled = false;
		if_fail_fe(::sampleVirtualKeyboardUpdate(app, dragCanceled));
		if(false == dragCanceled) {
			cnst ::llc::err_t inputResult = app.InputBox.Update(app.GUI, app.InputBox.VirtualKeyboard, app.Window.EventQueue, app.ControlsKeyboard);
			if_fail_fe(inputResult);
			if(INT_MAX == inputResult)
				app.InputBox.Edit(app.GUI, false);
		}
	}
	else if(app.Window.Input->KeyDown(::llc::SYSTEM_KEY_Escape))
		rtrn ::llc::APPLICATION_STATE_EXIT;
	if(app.GUI.Controls.IsAction(app.ControlExit))
		rtrn ::llc::APPLICATION_STATE_EXIT;
	if_fail_fe(::sampleGUIComponentsPerformanceUpdate(app, secondsLastFrame));
	bool doubleClick = false;
	if_fail_fe(::sampleGUIComponentsDoubleClick(app.Window.EventQueue, doubleClick));
	::llc::u2_t hoveredViewport = app.Viewports.size();
	for(::llc::u0_t iViewport = 0; iViewport < app.Viewports.size(); ++iViewport) {
		::llc::SViewport & viewport = app.Viewports[iViewport];
		cnst ::llc::cid_t idTitle = viewport.IdControls[::llc::VIEWPORT_CONTROL_TITLE];
		cnst ::llc::cid_t idClose = viewport.IdControls[::llc::VIEWPORT_CONTROL_CLOSE];
		cnst ::llc::cid_t idResize = viewport.IdControls[::llc::VIEWPORT_CONTROL_RESIZE_BOTTOM_RIGHT];
		if(::sampleGUIComponentsPressed(app, viewport))
			if_fail_fe(::sampleGUIComponentsActivate(app, iViewport));
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
	}
	if(doubleClick && hoveredViewport < app.Viewports.size())
		if_fail_fe(::sampleGUIComponentsFold(app, (::llc::u0_t)hoveredViewport));
	rtrn ::llc::guiUpdateMetrics(app.GUI, app.Window.Size, false);
}

::llc::err_t sampleGUIComponentsDraw(SSampleGUIComponents & app, ::llc::img8bgra (&clientImages)[SAMPLE_GUI_COMPONENTS_COUNT], ::llc::u2_t frameIndex, ::llc::g8bgra target) {
	if_fail_fe(target.fill(::llc::SColorBGRA{16, 14, 12}));
	for(::llc::u0_t iViewport = 0; iViewport < SAMPLE_GUI_COMPONENTS_COUNT; ++iViewport) {
		cnst ::llc::cid_t idClient = app.Viewports[iViewport].IdControls[::llc::VIEWPORT_CONTROL_TARGET];
		cnst ::llc::n2u2_t imageSize = app.GUI.Controls.Placement[idClient].Area.Size.u2();
		if(clientImages[iViewport].metrics() != imageSize)
			if_fail_fe(clientImages[iViewport].resize(imageSize));
		::sampleGUIComponentsImageUpdate(clientImages[iViewport], iViewport, frameIndex);
		app.GUI.Controls.Images[idClient].Image = clientImages[iViewport].View;
		app.GUI.Controls.Images[idClient].ImageAlign = ::llc::ALIGN_CENTER;
	}
	rtrn ::llc::guiDraw(app.GUI, target);
}

#define LLC_SAMPLE_GUI_COMPONENTS_TITLE "LLC GUI components"

stct SApplication {
	::llc::SFramework	Framework;
	SSampleGUIComponents	Sample;
	::llc::img8bgra	ClientImages[SAMPLE_GUI_COMPONENTS_COUNT] = {};
	::llc::u2_t		FrameIndex = 0;

	SApplication(::llc::SRuntimeValues & runtimeValues)
		: Framework(runtimeValues, {}), Sample(Framework.RootWindow, *Framework.GUI) {
		if_fail_e(::llc::rasterFontDefaults(Sample.Engine.Scene->Graphics->Fonts));
	}
};

::llc::err_t setup(SApplication & app) {
	::llc::SWindow & window = app.Framework.RootWindow;
	window.Size = {1280, 720};
	if_null_ve(-1, window.BackBuffer.create());
	if_fail_fe(window.BackBuffer->resize(window.Size.u2()));
	if_fail_fe(::llc::mainWindowCreate(window, app.Framework.RuntimeValues.PlatformDetail, window.Input));
	rtrn ::sampleGUIComponentsSetup(app.Sample);
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
	cnst ::llc::err_t resultSample = ::sampleGUIComponentsUpdate(app.Sample, false, app.Framework.FrameInfo.Seconds.LastFrame);
	::llc::sleep(1);
	rtrn resultSample;
}

::llc::err_t draw(SApplication & app) {
	::llc::SWindow & window = app.Framework.RootWindow;
	if(window.NoDraw)
		rtrn 0;
	if_fail_fe(window.BackBuffer->resize(window.Size.u2()));
	rtrn ::sampleGUIComponentsDraw(app.Sample, app.ClientImages, app.FrameIndex++, window.BackBuffer->Color.View);
}

LLC_DEFINE_APPLICATION_ENTRY_POINT(SApplication, LLC_SAMPLE_GUI_COMPONENTS_TITLE)
