#include "llc_framework.h"
#include "llc_gui_composite.h"
#include "llc_gui_inputbox.h"
#include "llc_process_broker.h"
#include "lls_gui_build.h"
#include "lls_explorer.h"
#include "lls_gui_surface.h"
#include <atomic>
#include <memory>

#ifndef LLS_GUI_H
#define LLS_GUI_H

GDEFINE_ENUM_TYPE(LLS_TOOLBAR, llc::u0_t);
GDEFINE_ENUM_VALUED(LLS_TOOLBAR, Explorer , 0, "D");
GDEFINE_ENUM_VALUED(LLS_TOOLBAR, Help     , 1, "?");
GDEFINE_ENUM_VALUED(LLS_TOOLBAR, Processes, 2, "P");
GDEFINE_ENUM_VALUED(LLS_TOOLBAR, Services , 3, "S");
GDEFINE_ENUM_VALUED(LLS_TOOLBAR, Console  , 4, ">");

GDEFINE_ENUM_TYPE(LLS_SESSION_ACTION, llc::u0_t);
GDEFINE_ENUM_VALUED(LLS_SESSION_ACTION, StopService     , 0, "\xFE");
GDEFINE_ENUM_VALUED(LLS_SESSION_ACTION, RestartService  , 1, "\x1D");
GDEFINE_ENUM_VALUED(LLS_SESSION_ACTION, RestartDashboard, 2, "\x12");
GDEFINE_ENUM_VALUED(LLS_SESSION_ACTION, ExitDashboard   , 3, "X");

GDEFINE_ENUM_TYPE(LLS_VIEWPORT, llc::u0_t);
GDEFINE_ENUM_VALUE(LLS_VIEWPORT, Title , 0);
GDEFINE_ENUM_VALUE(LLS_VIEWPORT, Close , 1);
GDEFINE_ENUM_VALUE(LLS_VIEWPORT, Target, 2);
GDEFINE_ENUM_VALUE(LLS_VIEWPORT, Resize, 3);

GDEFINE_ENUM_TYPE(PROCESS_VIEWPORT, llc::u0_t);
GDEFINE_ENUM_VALUE(PROCESS_VIEWPORT, Row00,  0);
GDEFINE_ENUM_VALUE(PROCESS_VIEWPORT, Row01,  1);
GDEFINE_ENUM_VALUE(PROCESS_VIEWPORT, Row02,  2);
GDEFINE_ENUM_VALUE(PROCESS_VIEWPORT, Row03,  3);
GDEFINE_ENUM_VALUE(PROCESS_VIEWPORT, Row04,  4);
GDEFINE_ENUM_VALUE(PROCESS_VIEWPORT, Row05,  5);
GDEFINE_ENUM_VALUE(PROCESS_VIEWPORT, Row06,  6);
GDEFINE_ENUM_VALUE(PROCESS_VIEWPORT, Row07,  7);
GDEFINE_ENUM_VALUE(PROCESS_VIEWPORT, Row08,  8);
GDEFINE_ENUM_VALUE(PROCESS_VIEWPORT, Row09,  9);
GDEFINE_ENUM_VALUE(PROCESS_VIEWPORT, Row10, 10);
GDEFINE_ENUM_VALUE(PROCESS_VIEWPORT, Row11, 11);

GDEFINE_ENUM_TYPE(SERVICE_VIEWPORT, llc::u0_t);
GDEFINE_ENUM_VALUE(SERVICE_VIEWPORT, Service        , 0);
GDEFINE_ENUM_VALUE(SERVICE_VIEWPORT, ServiceValue   , 1);
GDEFINE_ENUM_VALUE(SERVICE_VIEWPORT, Connection     , 2);
GDEFINE_ENUM_VALUE(SERVICE_VIEWPORT, ConnectionValue, 3);
GDEFINE_ENUM_VALUE(SERVICE_VIEWPORT, Identity       , 4);
GDEFINE_ENUM_VALUE(SERVICE_VIEWPORT, IdentityValue  , 5);
GDEFINE_ENUM_VALUE(SERVICE_VIEWPORT, Response       , 6);
GDEFINE_ENUM_VALUE(SERVICE_VIEWPORT, ResponseValue  , 7);
GDEFINE_ENUM_VALUE(SERVICE_VIEWPORT, Stop           , 8);
GDEFINE_ENUM_VALUE(SERVICE_VIEWPORT, Restart        , 9);

GDEFINE_ENUM_TYPE(CONSOLE_VIEWPORT, llc::u0_t);
GDEFINE_ENUM_VALUE(CONSOLE_VIEWPORT, Row00, 0);
GDEFINE_ENUM_VALUE(CONSOLE_VIEWPORT, Row01, 1);
GDEFINE_ENUM_VALUE(CONSOLE_VIEWPORT, Row02, 2);
GDEFINE_ENUM_VALUE(CONSOLE_VIEWPORT, Row03, 3);

GDEFINE_ENUM_TYPE(EXPLORER_VIEWPORT, llc::u0_t);
GDEFINE_ENUM_VALUE(EXPLORER_VIEWPORT, Path    ,  0);
GDEFINE_ENUM_VALUE(EXPLORER_VIEWPORT, Row00   ,  1);
GDEFINE_ENUM_VALUE(EXPLORER_VIEWPORT, Row01   ,  2);
GDEFINE_ENUM_VALUE(EXPLORER_VIEWPORT, Row02   ,  3);
GDEFINE_ENUM_VALUE(EXPLORER_VIEWPORT, Row03   ,  4);
GDEFINE_ENUM_VALUE(EXPLORER_VIEWPORT, Row04   ,  5);
GDEFINE_ENUM_VALUE(EXPLORER_VIEWPORT, Row05   ,  6);
GDEFINE_ENUM_VALUE(EXPLORER_VIEWPORT, Row06   ,  7);
GDEFINE_ENUM_VALUE(EXPLORER_VIEWPORT, Row07   ,  8);
GDEFINE_ENUM_VALUE(EXPLORER_VIEWPORT, Row08   ,  9);
GDEFINE_ENUM_VALUE(EXPLORER_VIEWPORT, Row09   , 10);
GDEFINE_ENUM_VALUE(EXPLORER_VIEWPORT, Row10   , 11);
GDEFINE_ENUM_VALUE(EXPLORER_VIEWPORT, Row11   , 12);
GDEFINE_ENUM_VALUE(EXPLORER_VIEWPORT, Row12   , 13);
GDEFINE_ENUM_VALUE(EXPLORER_VIEWPORT, Row13   , 14);
GDEFINE_ENUM_VALUE(EXPLORER_VIEWPORT, Row14   , 15);
GDEFINE_ENUM_VALUE(EXPLORER_VIEWPORT, Row15   , 16);
GDEFINE_ENUM_VALUE(EXPLORER_VIEWPORT, Previous, 17);
GDEFINE_ENUM_VALUE(EXPLORER_VIEWPORT, Next    , 18);
GDEFINE_ENUM_VALUE(EXPLORER_VIEWPORT, Summary , 19);

GDEFINE_ENUM_TYPE(HELP_VIEWPORT, llc::u0_t);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Topic00      ,  0);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Topic01      ,  1);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Topic02      ,  2);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Topic03      ,  3);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Topic04      ,  4);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Topic05      ,  5);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Topic06      ,  6);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Topic07      ,  7);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Topic08      ,  8);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Topic09      ,  9);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Topic10      , 10);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Topic11      , 11);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, TopicPrevious, 12);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, TopicNext    , 13);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Line00       , 14);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Line01       , 15);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Line02       , 16);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Line03       , 17);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Line04       , 18);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Line05       , 19);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Line06       , 20);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Line07       , 21);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Line08       , 22);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Line09       , 23);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Line10       , 24);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Line11       , 25);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Line12       , 26);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Line13       , 27);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Line14       , 28);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Line15       , 29);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Line16       , 30);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Line17       , 31);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Line18       , 32);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, Line19       , 33);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, PagePrevious , 34);
GDEFINE_ENUM_VALUE(HELP_VIEWPORT, PageNext     , 35);

enum LLS_GUI_VIEWPORT : llc::u0_t {
	LLS_GUI_VIEWPORT_SESSION,
	LLS_GUI_VIEWPORT_WORKSPACE,
	LLS_GUI_VIEWPORT_INSPECTOR,
	LLS_GUI_VIEWPORT_COUNT,
};

stct SConsoleWork {
	llc::SProcessBrokerJob Job = {};
	std::atomic<int> Status = 0;
};

stct SLLSGUI {
	llc::SFramework		Framework;
	SLLSGUISurface Surface;
	llc::cid_t Host = -1, Toolbar = -1, SessionToolbar = -1, Processes = -1, Services = -1, Console = -1, Help = -1;
	llc::acid Explorers;
	llc::aobj<SExplorer> ExplorerState;
	llc::aobj<llc::aobj<llc::string>> ExplorerText;
	llc::au2_t ExplorerOffset;
	llc::cid_t Exit = -1, Restart = -1, StopService = -1, RestartService = -1, Benchmark = -1;
	llc::astatic<llc::SViewport, LLS_GUI_VIEWPORT_COUNT> Viewports = {};
	llc::astatic<llc::astcid<llc::GUI_VIEWPORT_ACTION_COUNT>, LLS_GUI_VIEWPORT_COUNT> ViewportActions = {};
	llc::u3_t SessionId = 0, RenderUs = 0, RenderCount = 0, RenderTotalUs = 0, RenderPeakUs = 0, BenchmarkUs = 0, BenchmarkTotalUs = 0;
	llc::u2_t BenchmarkLeft = 0;
	llc::SInputBox		Input, ConsoleInput;
	llc::acid			Rows, Lines, Processable, Tabs, Breadcrumbs;
	llc::aobj<llc::string> ConsoleLines;
	std::shared_ptr<SConsoleWork> PendingConsole = {};
	llc::aobj<llc::string> RowText, TabText, CommandTemplateText, CommandTemplateJSON;
	llc::aobj<llc::vcst_t> Paths, DetailLines, DetailPaths;
	llc::aobj<llc::vcst_t> HelpLines;
	llc::au2_t TabValues;
	SGUIFamily Family;
	SExplorer Explorer;
	llc::SJSONFile Layout;
	llc::au2_t			Matches;
	llc::string			Query, Detail, Stats, Notice, ConsoleFolder;
	llc::cid_t			Title = -1, Statistics = -1, Hint = -1, Keyboard = -1;
	llc::cid_t			ListPrev = -1, ListNext = -1, PagePrev = -1, PageNext = -1, OpenPath = -1;
	llc::u2_t			Tab = 0, ListOffset = 0, LineOffset = 0, Selected = (llc::u2_t)-1;
	llc::u2_t			HelpSelected = 0, HelpTopicOffset = 0, HelpLineOffset = 0, HelpColumns = 0;
	llc::u2_t			Revision = (llc::u2_t)-1, VisibleLines = 0, VisibleRows = 0, Columns = 0;
	llc::n2u1_t			LastSize = {};
	double				Refresh = 0;
	bool				ShowKeyboard = false, ConsoleFocused = false, Dirty = true;

	explicit SLLSGUI(llc::SRuntimeValues & runtime) : Framework(runtime, {}) {}
};

stct SApplication;
llc::err_t llsGUISetup	(SApplication & app);
llc::err_t llsGUIUpdate	(SApplication & app);
llc::err_t llsHelpFind	(llc::vcst_t query, llc::au2_t & matches);

#endif
