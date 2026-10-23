#include "llc_gui.h"

#ifndef LLS_GUI_SURFACE_H
#define LLS_GUI_SURFACE_H

// A GUI belongs to this image, not to an OS window. The parent only displays it.
stct SLLSGUISurface {
	llc::SGUI GUI;
	llc::img8bgra Target;
	llc::SInput Input;
	bool Focused = true, Captured = false;
};

llc::err_t llsSurfaceUpdate(SLLSGUISurface & surface, llc::SGUI & parent, llc::cid_t client, cnst llc::SInput & input);
llc::err_t llsSurfaceDraw(SLLSGUISurface & surface, llc::SGUI & parent, llc::cid_t client);

#endif
