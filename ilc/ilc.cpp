#include "llc_args.h"
#include "llc_runtime.h"
#include "llc_xml_reader.h"

sttc	::llc::err_t	ilc_entry_point		(::llc::SRuntimeValues & runtimeValues);
LLC_SYSTEM_OS_ENTRY_POINT(::ilc_entry_point);

LLC_USING_TYPEINT();
LLC_USING_APOD();
LLC_USING_VIEW();

stct SILCApp {
	::llc::SCommandLineArgs	CommandLineArgs			= {};
	::llc::vcst_t			SolutionPath			= {};
	::llc::vcst_t			ProjectPath			= {};
	::llc::SXMLFile			ProjectFile			= {};
};

sttc ::llc::err_t ilcDisplayHelp(cnst SILCApp & appState) {
	always_printf
		( "Usage: %s [-platform=<platform>] [-configuration=<configuration>] <solution.sln|solution.slnx> <project.vcxproj>"
		"\nResolves the selected project's OutDir and IntDir using the supplied solution and project paths."
		, appState.CommandLineArgs.ProgramName.begin()
		);
	rtrn 0;
}

sttc ::llc::err_t ilcBuild(SILCApp &) { rtrn 0; }

sttc	::llc::err_t	ilc_entry_point		(::llc::SRuntimeValues & runtimeValues) {
	SILCApp appState = {};
	appState.CommandLineArgs = runtimeValues.EntryPointArgs;
	if(0 <= ::llc::argsOptionIndex(appState.CommandLineArgs, "help") || appState.CommandLineArgs.Positionals.size() < 2)
		rtrn ::ilcDisplayHelp(appState);
	appState.SolutionPath = appState.CommandLineArgs.Positionals[0];
	appState.ProjectPath = appState.CommandLineArgs.Positionals[1];
	if_fail_fe(::llc::xmlFileRead(appState.ProjectFile, appState.ProjectPath));
	rtrn ::ilcBuild(appState);
}
