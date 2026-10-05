#include "llc_path.h"
#include "llc_file.h"
#include "llc_string_compose.h"
#include "llc_string.h"
#include "llc_timer.h"
#include "llc_minmax.h"
#include "llc_args.h"
#include "llc_runtime.h"
#include "llc_xml_reader.h"

sttc	::llc::err_t	ilc_entry_point		(::llc::SRuntimeValues & runtimeValues);
LLC_SYSTEM_OS_ENTRY_POINT(::ilc_entry_point);

LLC_USING_TYPEINT();
LLC_USING_APOD();
LLC_USING_VIEW();

stct SILCApp {
	::llc::SCommandLineArgs	CommandLineArgs;
	::llc::SXMLFile			ProjectFile;
};

sttc bool ilcTextEquals(::llc::vcsc_t left, ::llc::vcsc_t right) {
	rtrn left.size() == right.size() && (0 == left.size() || 0 == memcmp(left.begin(), right.begin(), left.size()));
}

sttc ::llc::err_t ilcPathDirectory(::llc::vcsc_t path, ::llc::vcsc_t & directory) {
	cnst ::llc::err_t iSlash		= ::llc::findLastSlash(path);
	if_fail_fef(iSlash, "Path has no directory:'%.*s'.", (int)path.size(), path.begin());
	rtrn path.slice(directory, 0, (::llc::u2_t)iSlash + 1);
}

sttc ::llc::err_t ilcProjectName(::llc::vcsc_t projectPath, ::llc::vcsc_t & projectName) {
	cnst ::llc::err_t iSlash		= ::llc::findLastSlash(projectPath);
	cnst ::llc::u2_t iBegin		= 0 <= iSlash ? (::llc::u2_t)iSlash + 1 : 0;
	::llc::u2_t iEnd				= projectPath.size();
	for(::llc::u2_t iCharacter = iBegin; iCharacter < projectPath.size(); ++iCharacter)
		if(projectPath[iCharacter] == '.')
			iEnd					= iCharacter;
	if_true_fef(iEnd == iBegin, "Project path has no project name:'%.*s'.", (int)projectPath.size(), projectPath.begin());
	rtrn projectPath.slice(projectName, iBegin, iEnd - iBegin);
}

sttc ::llc::err_t ilcPropertyExpand
	(::llc::vcsc_t source, ::llc::vcsc_t solutionDir, ::llc::vcsc_t projectDir, ::llc::vcsc_t projectName
	, ::llc::vcsc_t platform, ::llc::vcsc_t configuration, ::llc::asc_t & output) {
	output.clear();
	for(::llc::u2_t iCharacter = 0; iCharacter < source.size();) {
		if(iCharacter + 2 > source.size() || source[iCharacter] != '$' || source[iCharacter + 1] != '(') {
			if_fail_fe(output.push_back(source[iCharacter++]));
			continue;
		}
		cnst ::llc::u2_t iName		= iCharacter + 2;
		::llc::u2_t iEnd				= iName;
		while(iEnd < source.size() && source[iEnd] != ')')
			++iEnd;
		if_true_fef(iEnd >= source.size(), "Unclosed project property in:'%.*s'.", (int)source.size(), source.begin());
		cnst ::llc::vcsc_t property	= {&source[iName], iEnd - iName};
		::llc::vcsc_t replacement;
		     if(::ilcTextEquals(property, LLC_CXS("SolutionDir"  ))) replacement = solutionDir;
		else if(::ilcTextEquals(property, LLC_CXS("ProjectDir"   ))) replacement = projectDir;
		else if(::ilcTextEquals(property, LLC_CXS("ProjectName"  ))) replacement = projectName;
		else if(::ilcTextEquals(property, LLC_CXS("Platform"     ))) replacement = platform;
		else if(::ilcTextEquals(property, LLC_CXS("Configuration"))) replacement = configuration;
		else {
			error_printf("Unsupported project property:'%.*s'.", (int)property.size(), property.begin());
			rtrn -1;
		}
		if_fail_fe(output.append(replacement));
		iCharacter					= iEnd + 1;
	}
	rtrn output.size();
}

sttc ::llc::err_t ilcProjectPathProperties
	(cnst ::llc::SXMLReader & reader, ::llc::vcsc_t xmlDoc, ::llc::vcsc_t platform, ::llc::vcsc_t configuration
	, ::llc::vcsc_t & outDir, ::llc::vcsc_t & intDir) {
	::llc::asc_t conditionText;
	if_fail_fe(::llc::append_strings(conditionText, "'$(Configuration)|$(Platform)'=='", configuration, "|", platform, "'"));
	cnst ::llc::err_t iProject	= ::llc::xmlNodeChild(reader, xmlDoc, 0, LLC_CXS("Project"));
	if_fail_fef(iProject, "%s", "Project XML has no Project root node.");
	for(::llc::u2_t iToken = (::llc::u2_t)iProject + 1; iToken < reader.Token.size(); ++iToken) {
		cnst ::llc::SXMLToken & token = reader.Token[iToken];
		if(token.Type != ::llc::XML_TOKEN_TAG_NODE || token.Parent != iProject)
			continue;
		::llc::vcsc_t nodeName;
		if(0 > ::llc::xmlNodeName(reader, xmlDoc, iToken, nodeName) || false == ::ilcTextEquals(nodeName, LLC_CXS("PropertyGroup")))
			continue;
		::llc::vcsc_t condition;
		cnst ::llc::err_t iCondition = ::llc::xmlNodeAttribute(reader, xmlDoc, iToken, LLC_CXS("Condition"), condition);
		if(0 <= iCondition && false == ::ilcTextEquals(condition, conditionText))
			continue;
		cnst ::llc::err_t iOutNode = ::llc::xmlNodeChild(reader, xmlDoc, iToken, LLC_CXS("OutDir"));
		if(0 <= iOutNode) {
			::llc::vcsc_t value;
			if(0 <= ::llc::xmlNodeText(reader, xmlDoc, (::llc::u2_t)iOutNode, value)) {
				if_fail_fe(::llc::trim(value));
				outDir					= value;
			}
		}
		cnst ::llc::err_t iIntNode = ::llc::xmlNodeChild(reader, xmlDoc, iToken, LLC_CXS("IntDir"));
		if(0 <= iIntNode) {
			::llc::vcsc_t value;
			if(0 <= ::llc::xmlNodeText(reader, xmlDoc, (::llc::u2_t)iIntNode, value)) {
				if_fail_fe(::llc::trim(value));
				intDir					= value;
			}
		}
	}
	if_zero_fef(outDir.size(), "OutDir not found for %.*s|%.*s.", (int)configuration.size(), configuration.begin(), (int)platform.size(), platform.begin());
	if_zero_fef(intDir.size(), "IntDir not found for %.*s|%.*s.", (int)configuration.size(), configuration.begin(), (int)platform.size(), platform.begin());
	rtrn 0;
}

sttc ::llc::err_t ilcResolveProjectPaths
	(::llc::SXMLFile & projectFile, ::llc::vcsc_t solutionPath, ::llc::vcsc_t projectPath, ::llc::vcsc_t platform, ::llc::vcsc_t configuration
	, ::llc::asc_t & outDir, ::llc::asc_t & intDir) {
	::llc::vcsc_t solutionDir;
	::llc::vcsc_t projectDir;
	::llc::vcsc_t projectName;
	if_fail_fe(::ilcPathDirectory(solutionPath, solutionDir));
	if_fail_fe(::ilcPathDirectory(projectPath, projectDir));
	if_fail_fe(::ilcProjectName(projectPath, projectName));
	if_fail_fe(::llc::xmlFileRead(projectFile, projectPath));
	::llc::vcsc_t rawOutDir;
	::llc::vcsc_t rawIntDir;
	if_fail_fe(::ilcProjectPathProperties(projectFile.Reader, projectFile.Bytes, platform, configuration, rawOutDir, rawIntDir));
	::llc::asc_t expanded;
	if_fail_fe(::ilcPropertyExpand(rawOutDir, solutionDir, projectDir, projectName, platform, configuration, expanded));
	if_fail_fe(::llc::pathNormalize(expanded, outDir));
	if_fail_fe(::ilcPropertyExpand(rawIntDir, solutionDir, projectDir, projectName, platform, configuration, expanded));
	if_fail_fe(::llc::pathNormalize(expanded, intDir));
	rtrn 0;
}

sttc ::llc::err_t ilcDisplayHelp(cnst SILCApp & appState) {
	always_printf
		( "Usage: %s [-platform=<platform>] [-configuration=<configuration>] <solution.sln|solution.slnx> <project.vcxproj>"
		"\nResolves the selected project's OutDir and IntDir using the supplied solution and project paths."
		, appState.CommandLineArgs.ProgramName.begin()
		);
	rtrn 0;
}

sttc	::llc::err_t	ilc_entry_point		(::llc::SRuntimeValues & runtimeValues) {
	SILCApp appState;
	appState.CommandLineArgs		= runtimeValues.EntryPointArgs;
	if(0 <= ::llc::argsOptionIndex(appState.CommandLineArgs, "help") || appState.CommandLineArgs.Positionals.size() < 2)
		rtrn ::ilcDisplayHelp(appState);
	::llc::vcst_t platform			= LLC_CXS("x64");
	::llc::vcst_t configuration	= LLC_CXS("Debug");
	if(0 <= ::llc::argsOptionIndex(appState.CommandLineArgs, "platform"))
		if_fail_fe(::llc::argsOptionValue(appState.CommandLineArgs, "platform", platform));
	if(0 <= ::llc::argsOptionIndex(appState.CommandLineArgs, "configuration"))
		if_fail_fe(::llc::argsOptionValue(appState.CommandLineArgs, "configuration", configuration));
	::llc::asc_t outDir;
	::llc::asc_t intDir;
	if_fail_fe(::ilcResolveProjectPaths
		(appState.ProjectFile, appState.CommandLineArgs.Positionals[0], appState.CommandLineArgs.Positionals[1], platform, configuration, outDir, intDir));
	always_printf("OutDir: %s", outDir.begin());
	always_printf("IntDir: %s", intDir.begin());
	rtrn 0;
}
