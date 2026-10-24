#include "llc_framework.h"
#include "llc_gui_inputbox.h"
#include "lls_gui_build.h"
#include "lls_explorer.h"
#include "lls_gui_surface.h"

#ifndef LLS_GUI_H
#define LLS_GUI_H

stct SLLSGUI {
	llc::SFramework		Framework;
	SLLSGUISurface Surface;
	llc::cid_t Host = -1, Exit = -1, Restart = -1, StopService = -1, Benchmark = -1;
	llc::u3_t SessionId = 0, RenderUs = 0, RenderCount = 0, RenderTotalUs = 0, RenderPeakUs = 0, BenchmarkUs = 0, BenchmarkTotalUs = 0;
	llc::u2_t BenchmarkLeft = 0;
	llc::SInputBox		Input;
	llc::acid			Rows, Lines, Processable, Tabs, Breadcrumbs;
	llc::aobj<llc::string> RowText, TabText, CommandTemplateText, CommandTemplateJSON;
	llc::aobj<llc::vcst_t> Paths, DetailLines, DetailPaths;
	llc::au2_t TabValues;
	SGUIFamily Family;
	SExplorer Explorer;
	llc::SJSONFile Layout;
	llc::au2_t			Matches;
	llc::string			Query, Detail, Stats, Notice;
	llc::cid_t			Title = -1, Statistics = -1, Hint = -1, Keyboard = -1;
	llc::cid_t			ListPrev = -1, ListNext = -1, PagePrev = -1, PageNext = -1, OpenPath = -1;
	llc::u2_t			Tab = 0, ListOffset = 0, LineOffset = 0, Selected = (llc::u2_t)-1;
	llc::u2_t			Revision = (llc::u2_t)-1, VisibleLines = 0, VisibleRows = 0, Columns = 0;
	llc::n2u1_t			LastSize = {};
	double				Refresh = 0;
	bool				ShowKeyboard = false, Dirty = true;

	explicit SLLSGUI(llc::SRuntimeValues & runtime) : Framework(runtime, {}) {}
};

stct SApplication;
llc::err_t llsGUISetup	(SApplication & app);
llc::err_t llsGUIUpdate	(SApplication & app);
llc::err_t llsHelpFind	(llc::vcst_t query, llc::au2_t & matches);

#endif
