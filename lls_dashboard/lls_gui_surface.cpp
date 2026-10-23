#include "lls_gui_surface.h"

llc::err_t llsSurfaceUpdate(SLLSGUISurface & surface, llc::SGUI & parent, llc::cid_t client, cnst llc::SInput & input) {
	cnst auto area = parent.Controls.Area[client].Client.Global;
	cnst bool visible = !llc::controlHidden(parent, client) && area.Size.x > 0 && area.Size.y > 0;
	cnst bool hovered = visible && parent.Controls.IsHovered(client);
	if(input.ButtonDown(0)) surface.Focused = surface.Captured = hovered;
	if(!visible) surface.Focused = surface.Captured = false;
	surface.Input.MousePrevious = surface.Input.MouseCurrent;
	surface.Input.KeyboardPrevious = surface.Input.KeyboardCurrent;
	surface.Input.MouseCurrent = input.MouseCurrent;
	surface.Input.MouseCurrent.Position -= area.Offset;
	surface.Input.KeyboardCurrent = surface.Focused ? input.KeyboardCurrent : llc::SInputKeyboard{};
	if(!hovered && !surface.Captured) {
		surface.Input.MouseCurrent.Position = {-1, -1};
		surface.Input.MouseCurrent.ButtonState = {};
		surface.Input.MouseCurrent.Deltas = {};
	}
	if(!input.MouseCurrent.ButtonState[0]) surface.Captured = false;
	if(visible) {
		cnst auto size = area.Size.u2();
		if(surface.Target.metrics() != size) { if_fail_fe(surface.Target.resize(size)); }
		if_fail_fe(llc::guiUpdateMetrics(surface.GUI, size.u1(), surface.GUI.LastSize != size.u1()));
	}
	rtrn llc::guiProcessInput(surface.GUI, surface.Input);
}

llc::err_t llsSurfaceDraw(SLLSGUISurface & surface, llc::SGUI & parent, llc::cid_t client) {
	if(llc::controlHidden(parent, client)) rtrn 0;
	if_fail_fe(surface.Target.View.fill(llc::bgra{25, 20, 17, 255}));
	if_fail_fe(llc::guiDraw(surface.GUI, surface.Target.View));
	parent.Controls.Images[client].Image = surface.Target.View;
	parent.Controls.Images[client].ImageAlign = llc::ALIGN_TOP_LEFT;
	rtrn 0;
}
