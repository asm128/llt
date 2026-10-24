#include "lcp_vs.h"
#include "lcp_metrics.h"
#include "lcp_text.h"
#include "llc_json_expression.h"
#include "llc_process.h"
#include "llc_complus.h"

llc::err_t lcpVSBuildCommand(cnst llc::SJSONReader & reader, llc::string & application, llc::string & command) {
	llc::vcst_t msbuild, configuration, platform;
	if_fail_fe(llc::jsonExpressionResolve("params.arguments.msbuild", reader, 0, msbuild));
	if_fail_fe(lcpStringDecode(msbuild, application));
	if(!application.size()) { if_fail_fe(application.append(msbuild)); }
	if_fail_fe(llc::jsonExpressionResolve("params.arguments.configuration", reader, 0, configuration));
	if_fail_fe(llc::split<llc::sc_c>('|', configuration, configuration, platform));
	if_true_fe(!configuration.size() || !platform.size());
	if_fail_fe(llc::jsonStringFormat("MSBuild.exe \"{params.arguments.solution}\" /t:\"{params.arguments.project}\"", reader, 0, command));
	rtrn llc::append_strings(command, " /p:Configuration=\"", configuration, "\" /p:Platform=\"", platform, '"');
}

namespace {
tplt <size_t _nSize>
llc::err_t buildFind(llc::vcst_t line, cnst char (&text)[_nSize]) { rtrn line.find({text, _nSize - 1}); }

llc::vcst_t buildLineTrim(llc::vcst_t line) {
	while(line.size() && (line[0] == ' ' || line[0] == '\t' || line[0] == '\r')) line = {line.begin() + 1, line.size() - 1};
	while(line.size() && (line[line.size() - 1] == ' ' || line[line.size() - 1] == '\t' || line[line.size() - 1] == '\r')) line = {line.begin(), line.size() - 1};
	rtrn line;
}

llc::err_t buildDiagnosticSanitize(llc::vcst_t line, llc::string & summary) {
	cnst llc::err_t project = buildFind(line, " [");
	if(project >= 0) line = {line.begin(), (llc::u2_t)project};
	cnst llc::err_t location = line.find('(');
	if(location > 0) {
		cnst llc::vcst_t source = {line.begin(), (llc::u2_t)location};
		cnst llc::err_t backslash = source.rfind('\\'), slash = source.rfind('/');
		cnst llc::err_t separator = llc::max(backslash, slash);
		if(separator >= 0) line = {line.begin() + separator + 1, line.size() - (llc::u2_t)(separator + 1)};
	}
	rtrn summary.append(line);
}

llc::err_t buildDiagnosticReason(llc::vcst_t line, llc::vcst_t & reason) {
	cnst llc::err_t project = buildFind(line, " [");
	if(project >= 0) line = {line.begin(), (llc::u2_t)project};
	llc::err_t marker = buildFind(line, ": fatal error ");
	if(marker < 0) marker = buildFind(line, ": error ");
	if(marker >= 0) { reason = {line.begin() + marker + 2, line.size() - (llc::u2_t)(marker + 2)}; rtrn marker; }
	if(buildFind(line, "fatal error ") == 0 || buildFind(line, "error ") == 0) { reason = line; rtrn 0; }
	rtrn -1;
}

bool buildDiagnosticSource(llc::vcst_t line) {
	rtrn line.find('(') >= 0 && (buildFind(line, "): error ") >= 0 || buildFind(line, ") : error ") >= 0 || buildFind(line, "): fatal error ") >= 0 || buildFind(line, ") : fatal error ") >= 0);
}

bool buildDiagnosticLocation(llc::vcst_t line, llc::vcst_t & location) {
	llc::err_t end = buildFind(line, "):");
	if(end < 0) end = buildFind(line, ") :");
	if(end < 0 || line.find('(') < 0) rtrn false;
	location = {line.begin(), (llc::u2_t)end + 1};
	rtrn true;
}

bool buildDiagnosticSourceFile(llc::vcst_t location) {
	rtrn buildFind(location, ".cpp(") >= 0 || buildFind(location, ".cxx(") >= 0 || buildFind(location, ".cc(") >= 0 || buildFind(location, ".c(") >= 0;
}

llc::u0_t buildDiagnosticCallsiteRank(llc::vcst_t context, llc::vcst_t location) {
	if(!location.size()) rtrn 0;
	if(buildFind(context, "see the first reference") >= 0 || buildFind(context, "required from here") >= 0 || buildFind(context, "instantiated from here") >= 0) rtrn 4;
	cnst bool reference = buildFind(context, "see reference to") >= 0;
	cnst bool compiling = buildFind(context, "while compiling") >= 0;
	if(!reference && !compiling) rtrn 0;
	if(buildDiagnosticSourceFile(location)) rtrn 3;
	rtrn reference ? 2 : 1;
}

llc::err_t buildDiagnosticCallsite(llc::vcst_t location, llc::vcst_t reason, llc::string & summary) {
	cnst llc::err_t open = location.find('(');
	if(open > 0) {
		cnst llc::vcst_t source = {location.begin(), (llc::u2_t)open};
		cnst llc::err_t backslash = source.rfind('\\'), slash = source.rfind('/');
		cnst llc::err_t separator = llc::max(backslash, slash);
		if(separator >= 0) location = {location.begin() + separator + 1, location.size() - (llc::u2_t)(separator + 1)};
	}
	if_fail_fe(summary.append(location));
	if_fail_fe(summary.append_string(": "));
	rtrn summary.append(reason);
}

llc::err_t buildDiagnosticFind(llc::vcst_t output, bool located, llc::string & summary) {
	llc::vcst_t root, reason, pendingLocation, callsite;
	llc::u0_t callsiteRank = 0;
	while(output.size()) {
		llc::vcst_t line, remaining;
		cnst llc::err_t split = llc::split<llc::sc_c>('\n', output, line, remaining);
		if(llc::failed(split)) { line = output; remaining = {}; }
		line = buildLineTrim(line);
		llc::vcst_t lineReason;
		cnst bool error = buildDiagnosticReason(line, lineReason) >= 0;
		if(error && (!located || buildDiagnosticSource(line))) {
			if(root.size()) break;
			root = line;
			reason = lineReason;
			pendingLocation = {};
		}
		else if(root.size()) {
			llc::vcst_t location;
			if(buildDiagnosticLocation(line, location)) {
				cnst llc::u0_t rank = buildDiagnosticCallsiteRank(line, location);
				if(rank > callsiteRank) { callsite = location; callsiteRank = rank; }
				pendingLocation = location;
			}
			else {
				cnst llc::u0_t rank = buildDiagnosticCallsiteRank(line, pendingLocation);
				if(rank > callsiteRank) { callsite = pendingLocation; callsiteRank = rank; }
				pendingLocation = {};
			}
		}
		if(!remaining.size()) break;
		output = remaining;
	}
	if(callsite.size() && callsiteRank > 1) rtrn buildDiagnosticCallsite(callsite, reason, summary);
	if(root.size()) rtrn buildDiagnosticSanitize(root, summary);
	rtrn 0;
}
}

llc::err_t lcpVSBuildSummary(llc::vcst_t output, llc::err_t status, llc::u2_t exitCode, llc::string & summary) {
	if_fail_fe(summary.clear());
	if(llc::succeeded(status) && !exitCode) rtrn summary.append_string("Build succeeded.");
	if_fail_fe(buildDiagnosticFind(output, true, summary));
	if(!summary.size()) { if_fail_fe(buildDiagnosticFind(output, false, summary)); }
	if(!summary.size()) { if_fail_fe(llc::append_strings(summary, "Build failed (", llc::str(status), "/", llc::str(exitCode), ").")); }
	rtrn summary.size();
}

#ifdef _DEBUG
llc::err_t lcpVSBuildSummaryTest() {
	stct SCase { llc::vcst_t Output, Expected; };
	cnst SCase cases[] = {
		{
			"D:\\library\\llc_min.h(10,2): error C2782: 'min': template parameter '_Ty' is ambiguous [sample.vcxproj]\n"
			"  D:\\source\\sample.cpp(42,9):\n"
			"  see reference to function template instantiation 'min<int,llc::u3_t>' being compiled\n",
			"sample.cpp(42,9): error C2782: 'min': template parameter '_Ty' is ambiguous"
		},
		{
			"D:\\library\\llc_ref.h(48,27): error C2512: 'SApp': no appropriate default constructor available [sample.vcxproj]\n"
			"  D:\\source\\app.h(19,21):\n"
			"  see reference to class template instantiation 'llc::pobj<SApp>' being compiled\n"
			"  D:\\source\\sample.cpp(102,2):\n"
			"  see the first reference to 'llc::pobj<SApp>::operator ->' in 'commandQueue'\n",
			"sample.cpp(102,2): error C2512: 'SApp': no appropriate default constructor available"
		},
		{
			"D:\\source\\sample.cpp(7,3): error C2664: cannot convert argument 1 [sample.vcxproj]\n",
			"sample.cpp(7,3): error C2664: cannot convert argument 1"
		},
		{
			"LINK : fatal error LNK1168: cannot open sample.dll for writing [sample.vcxproj]\n",
			"LINK : fatal error LNK1168: cannot open sample.dll for writing"
		},
	};
	for(llc::u2_t iCase = 0; iCase < llc::size(cases); ++iCase) {
		cnst SCase & test = cases[iCase];
		llc::string summary;
		if_fail_fe(lcpVSBuildSummary(test.Output, 0, 1, summary));
		if(summary != test.Expected) {
			llc::vcst_t first, remaining, reason;
			cnst llc::err_t split = llc::split<llc::sc_c>('\n', test.Output, first, remaining);
			error_printf("Build summary test %u expected '%.*s', got '%s'. split=%i marker=%i source=%u first='%.*s'.", iCase, LLC_VIEW_ARGS(test.Expected), summary.begin(), split, buildDiagnosticReason(first, reason), buildDiagnosticSource(first), LLC_VIEW_ARGS(first));
			rtrn -1;
		}
	}
	llc::string fallback;
	if_fail_fe(lcpVSBuildSummary("no compiler diagnostic\n", 0, 1, fallback));
	if_true_ve(-1, fallback != "Build failed (0/1).");
	rtrn 0;
}
#endif

#ifdef LLC_WINDOWS
#include <comdef.h>
#include <objbase.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "comsuppw.lib")

namespace {
using namespace llc;
using Dispatch = pcom<IDispatch>;

stct SApartment {
	HRESULT Result = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
	~SApartment() { if(SUCCEEDED(Result)) CoUninitialize(); }
};

// Automation arguments are reversed by IDispatch, not by the caller.
err_t invoke(IDispatch * object, cnst wchar_t * name, WORD flags, _variant_t & result, VARIANT * args = 0, UINT count = 0) {
	if_null_ve(E_POINTER, object);
	DISPID member, put = DISPID_PROPERTYPUT;
	LPOLESTR memberName = const_cast<LPOLESTR>(name);
	HRESULT status = object->GetIDsOfNames(IID_NULL, &memberName, 1, LOCALE_USER_DEFAULT, &member);
	if_fail_ve(status, status);
	DISPPARAMS params = {args, flags == DISPATCH_PROPERTYPUT ? &put : 0, count, flags == DISPATCH_PROPERTYPUT ? 1U : 0U};
	EXCEPINFO exception = {};
	result.Clear();
	status = object->Invoke(member, IID_NULL, LOCALE_USER_DEFAULT, flags, &params, &result, &exception, 0);
	if(exception.pfnDeferredFillIn) exception.pfnDeferredFillIn(&exception);
	SysFreeString(exception.bstrSource); SysFreeString(exception.bstrDescription); SysFreeString(exception.bstrHelpFile);
	rtrn status;
}
err_t get(IDispatch * object, cnst wchar_t * name, _variant_t & result) { rtrn invoke(object, name, DISPATCH_PROPERTYGET, result); }
err_t child(IDispatch * object, cnst wchar_t * name, Dispatch & result) {
	_variant_t value;
	if_fail_fe(get(object, name, value));
	if_true_ve(E_NOINTERFACE, value.vt != VT_DISPATCH || !value.pdispVal);
	result = value.pdispVal;
	rtrn 0;
}
err_t item(IDispatch * collection, long index, Dispatch & result) {
	_variant_t argument(index), value;
	if_fail_fe(invoke(collection, L"Item", DISPATCH_METHOD | DISPATCH_PROPERTYGET, value, &argument, 1));
	if_true_ve(E_NOINTERFACE, value.vt != VT_DISPATCH || !value.pdispVal);
	result = value.pdispVal;
	rtrn 0;
}
err_t utf8(BSTR value, string & text) {
	cnst UINT length = SysStringLen(value);
	if(!length) rtrn 0;
	cnst int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, length, 0, 0, 0, 0);
	if_zero_fe(count);
	cnst u2_t offset = text.size();
	if_fail_fe(text.resize(offset + count));
	if_zero_fe(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, length, text.Data + offset, count, 0, 0));
	rtrn 0;
}
err_t propertyText(IDispatch * object, cnst wchar_t * name, string & text) {
	_variant_t value;
	if_fail_fe(get(object, name, value));
	if_true_fe(value.vt != VT_BSTR);
	rtrn utf8(value.bstrVal, text);
}
err_t argument(cnst SJSONReader & reader, vcst_t path, string & result) {
	vcst_t value;
	if_fail_fe(jsonExpressionResolve(path, reader, 0, value));
	if_fail_fe(lcpStringDecode(value, result));
	if(!result.size()) { if_fail_fe(result.append(value)); }
	rtrn 0;
}
err_t wide(vcst_t text, _variant_t & result) {
	cnst int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.begin(), text.size(), 0, 0);
	if_true_fe(count <= 0);
	result.vt = VT_BSTR;
	result.bstrVal = SysAllocStringLen(0, count);
	if_null_ve(E_OUTOFMEMORY, result.bstrVal);
	if_zero_fe(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.begin(), text.size(), result.bstrVal, count));
	rtrn 0;
}

stct SInstance { u2_t PID = {}; Dispatch DTE; };
err_t instances(aobj<SInstance> & output) {
	pcom<IRunningObjectTable> table;
	pcom<IEnumMoniker> iterator;
	pcom<IBindCtx> context;
	if_fail_fe(GetRunningObjectTable(0, &table));
	if_fail_fe(CreateBindCtx(0, &context));
	if_fail_fe(table->EnumRunning(&iterator));
	for(;;) {
		pcom<IMoniker> moniker;
		cnst HRESULT next = iterator->Next(1, &moniker, 0);
		if(next == S_FALSE) break;
		if_fail_fe(next);
		LPOLESTR display = 0;
		if(FAILED(moniker->GetDisplayName(context, 0, &display))) continue;
		cnst bool visualStudio = wcsncmp(display, L"!VisualStudio.DTE.", 18) == 0;
		cnst wchar_t * separator = wcsrchr(display, L':');
		cnst u2_t pid = visualStudio && separator ? wcstoul(separator + 1, 0, 10) : 0;
		CoTaskMemFree(display);
		if(!pid) continue;
		pcom<IUnknown> unknown;
		if(FAILED(table->GetObject(moniker, &unknown))) continue;
		SInstance instance;
		instance.PID = pid;
		if(FAILED(unknown.as(instance.DTE))) continue;
		if_fail_fe(output.push_back(instance));
	}
	rtrn 0;
}
err_t state(SInstance & instance, string & output) {
	Dispatch solution, build, config, debugger;
	string solutionPath, name, platform;
	_variant_t mode, building;
	if_fail_fe(child(instance.DTE, L"Solution", solution));
	if_fail_fe(propertyText(solution, L"FullName", solutionPath));
	if_fail_fe(child(instance.DTE, L"Debugger", debugger));
	if_fail_fe(get(debugger, L"CurrentMode", mode));
	if_fail_fe(child(solution, L"SolutionBuild", build));
	if_fail_fe(get(build, L"BuildState", building));
	if(solutionPath.size()) {
		if_fail_fe(child(build, L"ActiveConfiguration", config));
		if_fail_fe(propertyText(config, L"Name", name));
		if_fail_fe(propertyText(config, L"PlatformName", platform));
	}
	if_fail_fe(append_strings(output, "{\"instance\":", str(instance.PID), ",\"solution\":"));
	if_fail_fe(lcpJSONString(output, solutionPath));
	if_fail_fe(output.append_string(",\"configuration\":"));
	string configuration;
	if(name.size()) { if_fail_fe(append_strings(configuration, name, '|', platform)); }
	if_fail_fe(lcpJSONString(output, configuration));
	rtrn append_strings(output, ",\"debugger\":", str((s2_t)(long)mode), ",\"buildState\":", str((s2_t)(long)building), '}');
}
err_t idle(IDispatch * dte) {
	Dispatch debugger, solution, build;
	_variant_t mode, building;
	if_fail_fe(child(dte, L"Debugger", debugger));
	if_fail_fe(get(debugger, L"CurrentMode", mode));
	if_true_vef(OS_BUSY, (long)mode != 1, "%s", "IDE is debugging; stop it explicitly first.");
	if_fail_fe(child(dte, L"Solution", solution));
	if_fail_fe(child(solution, L"SolutionBuild", build));
	if_fail_fe(get(build, L"BuildState", building));
	if_true_vef(OS_BUSY, (long)building == 2, "%s", "IDE is building.");
	rtrn 0;
}
err_t saved(IDispatch * object) {
	_variant_t value;
	if_fail_fe(get(object, L"Saved", value));
	if_true_vef(OS_BUSY, !(bool)value, "%s", "Unsaved work; save or discard it in Visual Studio first.");
	rtrn 0;
}
err_t cleanProject(IDispatch * project);
err_t cleanProjects(IDispatch * projects, bool recurse = false) {
	_variant_t count;
	if_fail_fe(get(projects, L"Count", count));
	for(long i = 1; i <= (long)count; ++i) {
		Dispatch project;
		if_fail_fe(item(projects, i, project));
		if_fail_fe(saved(project));
		if(recurse) { if_fail_fe(cleanProject(project)); }
	}
	rtrn 0;
}
err_t cleanProject(IDispatch * project) {
	string kind;
	if_fail_fe(propertyText(project, L"Kind", kind));
	if(_stricmp(kind.begin(), "{66A26720-8FB5-11D2-AA7E-00C04F688DDE}")) rtrn 0;
	Dispatch items;
	if_fail_fe(child(project, L"ProjectItems", items));
	_variant_t count;
	if_fail_fe(get(items, L"Count", count));
	for(long i = 1; i <= (long)count; ++i) {
		Dispatch entry;
		_variant_t subproject;
		if_fail_fe(item(items, i, entry));
		if_fail_fe(get(entry, L"SubProject", subproject));
		if(subproject.vt == VT_DISPATCH && subproject.pdispVal) {
			if_fail_fe(saved(subproject.pdispVal));
			if_fail_fe(cleanProject(subproject.pdispVal));
		}
	}
	rtrn 0;
}
err_t safeClose(IDispatch * dte) {
	if_fail_fe(idle(dte));
	Dispatch solution, documents, projects;
	if_fail_fe(child(dte, L"Solution", solution));
	if_fail_fe(saved(solution));
	if_fail_fe(child(solution, L"Projects", projects));
	if_fail_fe(cleanProjects(projects, true));
	if_fail_fe(child(dte, L"Documents", documents));
	if_fail_fe(cleanProjects(documents));
	_variant_t result;
	rtrn invoke(dte, L"Quit", DISPATCH_METHOD, result);
}
err_t findProject(IDispatch * project, vcst_t wanted, string & found, u2_t & matches) {
	string name, unique;
	if_fail_fe(propertyText(project, L"Name", name));
	if_fail_fe(propertyText(project, L"UniqueName", unique));
	if(name == wanted || unique == wanted) { ++matches; if_fail_fe(found.append(unique)); }
	string kind;
	if_fail_fe(propertyText(project, L"Kind", kind));
	if(_stricmp(kind.begin(), "{66A26720-8FB5-11D2-AA7E-00C04F688DDE}")) rtrn 0;
	Dispatch items;
	if_fail_fe(child(project, L"ProjectItems", items));
	_variant_t count;
	if_fail_fe(get(items, L"Count", count));
	for(long i = 1; i <= (long)count; ++i) {
		Dispatch entry;
		if_fail_fe(item(items, i, entry));
		_variant_t subproject;
		if_fail_fe(get(entry, L"SubProject", subproject));
		if(subproject.vt == VT_DISPATCH && subproject.pdispVal) {
			if_fail_fe(findProject(subproject.pdispVal, wanted, found, matches));
		}
	}
	rtrn 0;
}
err_t launch(IDispatch * dte, cnst SJSONReader & reader) {
	if_fail_fe(idle(dte));
	string wanted, configuration, requestedSolution, actualSolution;
	if_fail_fe(argument(reader, "params.arguments.project", wanted));
	if_fail_fe(argument(reader, "params.arguments.configuration", configuration));
	if_fail_fe(argument(reader, "params.arguments.solution", requestedSolution));
	Dispatch solution, build, configs, selected, projects;
	if_fail_fe(child(dte, L"Solution", solution));
	if_fail_fe(propertyText(solution, L"FullName", actualSolution));
	for(u2_t i = 0; i < requestedSolution.size(); ++i) if(requestedSolution[i] == '/') requestedSolution[i] = '\\';
	if_true_vef(OS_INVALID_PARAMETER, _stricmp(requestedSolution.begin(), actualSolution.begin()) != 0, "%s", "Selected IDE has another solution open.");
	if_fail_fe(child(solution, L"SolutionBuild", build));
	if_fail_fe(child(build, L"SolutionConfigurations", configs));
	_variant_t count;
	if_fail_fe(get(configs, L"Count", count));
	for(long i = 1; i <= (long)count; ++i) {
		Dispatch config;
		string name, platform, full;
		if_fail_fe(item(configs, i, config));
		if_fail_fe(propertyText(config, L"Name", name));
		if_fail_fe(propertyText(config, L"PlatformName", platform));
		if_fail_fe(append_strings(full, name, '|', platform));
		if(full == configuration) selected = config;
	}
	if_null_vef(OS_NOT_FOUND, selected.get(), "%s", "Solution configuration not found.");
	if_fail_fe(child(solution, L"Projects", projects));
	if_fail_fe(get(projects, L"Count", count));
	string projectName;
	u2_t matches = 0;
	for(long i = 1; i <= (long)count; ++i) {
		Dispatch project;
		if_fail_fe(item(projects, i, project));
		if_fail_fe(findProject(project, wanted, projectName, matches));
	}
	if_true_vef(OS_NOT_FOUND, matches != 1, "%s", "Project must match exactly one name or UniqueName.");
	_variant_t result, projectValue, startup;
	if_fail_fe(wide(projectName, projectValue));
	startup.vt = VT_ARRAY | VT_BSTR;
	startup.parray = SafeArrayCreateVector(VT_BSTR, 0, 1);
	if_null_ve(E_OUTOFMEMORY, startup.parray);
	long first = 0;
	if_fail_fe(SafeArrayPutElement(startup.parray, &first, projectValue.bstrVal));
	if_fail_fe(invoke(selected, L"Activate", DISPATCH_METHOD, result));
	if_fail_fe(invoke(build, L"StartupProjects", DISPATCH_PROPERTYPUT, result, &startup, 1));
	_variant_t args[] = {_variant_t(L""), _variant_t(L"Debug.Start")};
	rtrn invoke(dte, L"ExecuteCommand", DISPATCH_METHOD, result, args, 2);
}
err_t startIDE(vcst_t executable, vcst_t solution, u2_t & pid) {
	string command;
	if_fail_fe(append_strings(command, '"', executable, '"'));
	if(solution.size()) { if_fail_fe(append_strings(command, " \"", solution, '"')); }
	SProcessManager processes;
	cnst err_t id = processes.Create(executable, command, {});
	if_fail_fe(id);
	SProcessManager<>::TProcessScope scope{processes, (u2_t)id};
	if_fail_fe(processes.Launch(id));
	pid = processes.PlatformState[id].Info.Id;
	rtrn scope.Close();
}
err_t automation(cnst SJSONReader & reader, vcst_t action, string & text) {
	SApartment apartment;
	if_fail_fe(apartment.Result);
	aobj<SInstance> available;
	if_fail_fe(instances(available));
	if(action == "list") {
		if_fail_fe(text.push_back('['));
		for(u2_t i = 0; i < available.size(); ++i) {
			if(i) { if_fail_fe(text.push_back(',')); }
			string entry;
			cnst err_t result = state(available[i], entry);
			if(failed(result)) { if_fail_fe(append_strings(text, "{\"instance\":", str(available[i].PID), ",\"unavailable\":true}")); }
			else { if_fail_fe(text.append(entry)); }
		}
		rtrn text.push_back(']');
	}
	s3_t pid = 0;
	cnst err_t field = jsonExpressionResolve("params.arguments.instance", reader, 0);
	if(field < 0 && (action == "launch" || action == "run")) {
		string executable, solution, project, configuration;
		if_fail_fe(argument(reader, "params.arguments.devenv", executable));
		if_fail_fe(argument(reader, "params.arguments.solution", solution));
		if_fail_fe(argument(reader, "params.arguments.project", project));
		if_fail_fe(argument(reader, "params.arguments.configuration", configuration));
		if_true_fe(!executable.size() || !solution.size() || !project.size() || !configuration.size());
		for(u2_t i = 0; i < solution.size(); ++i) if(solution[i] == '/') solution[i] = '\\';
		u2_t started = 0;
		if_fail_fe(startIDE(executable, solution, started));
		pid = started;
		// Wait only for discovery/readiness. Never retry a launch or another mutation.
		for(u2_t attempt = 0; attempt < 150; ++attempt) {
			if_fail_fe(available.clear());
			if_fail_fe(instances(available));
			bool ready = false;
			for(SInstance & candidate : available) {
				if(candidate.PID != started) continue;
				Dispatch loaded;
				string path;
				ready = succeeded(child(candidate.DTE, L"Solution", loaded))
					&& succeeded(propertyText(loaded, L"FullName", path))
					&& _stricmp(path.begin(), solution.begin()) == 0;
			}
			if(ready) break;
			Sleep(200);
		}
	}
	else {
		if_fail_fe(field);
		if_fail_fe(jsonObjectGetInteger(reader, field, pid));
	}
	if_true_fe(pid <= 0 || pid > UINT32_MAX);
	for(SInstance & instance : available) {
		if(instance.PID != (u2_t)pid) continue;
		if(action == "ping") rtrn state(instance, text);
		if(action == "launch" || action == "run") {
			if_fail_fe(launch(instance.DTE, reader));
			rtrn append_strings(text, "Launch requested in IDE ", str(instance.PID), ". Uses the project's debugger settings; ping to inspect state.");
		}
		if(action == "close") {
			if_fail_fe(safeClose(instance.DTE));
			rtrn append_strings(text, "Close requested for IDE ", str(instance.PID), '.');
		}
		if(action == "restart") {
			string solutionPath, executable;
			Dispatch solution;
			if_fail_fe(child(instance.DTE, L"Solution", solution));
			if_fail_fe(propertyText(solution, L"FullName", solutionPath));
			if_fail_fe(propertyText(instance.DTE, L"FullName", executable));
			HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, instance.PID);
			if_null_ve(OS_ERROR, process);
			cnst err_t closed = safeClose(instance.DTE);
			instance.DTE.clear();
			solution.clear();
			cnst DWORD waited = succeeded(closed) ? WaitForSingleObject(process, 15000) : WAIT_FAILED;
			CloseHandle(process);
			if_fail_fe(closed);
			if_true_vef(OS_BUSY, waited != WAIT_OBJECT_0, "%s", "IDE has not exited; no second instance was started.");
			u2_t newPID = 0;
			if_fail_fe(startIDE(executable, solutionPath, newPID));
			rtrn append_strings(text, "IDE restarted; new instance ", str(newPID), ". Solution reopening; list/ping for readiness. No application was launched.");
		}
	}
	rtrn OS_NOT_FOUND;
}
}
#endif

llc::err_t lcpVSCall(cnst llc::SJSONReader & reader, llc::string & body, llc::err_t * operationStatus) {
	llc::vcst_t action;
	if_fail_fe(llc::jsonExpressionResolve("params.arguments.action", reader, 0, action));
	if(action != "build" && action != "launch" && action != "run" && action != "list" && action != "ping" && action != "close" && action != "restart") {
		if(operationStatus) *operationStatus = llc::OS_INVALID_PARAMETER;
		rtrn lcpTextBody(body, "Unknown vs action. Open help: vs.", true);
	}
	llc::string text;
	llc::err_t status = 0;
	if(action == "build") {
		llc::string application, command;
		status = lcpVSBuildCommand(reader, application, command);
		llc::au0_t output;
		llc::u2_t exitCode = 0;
		if(llc::succeeded(status)) status = llc::SProcessManager{}.Execute(application, command, {}, output, exitCode, {});
		if_fail_fe(lcpMetricsRecord(COMMUNICATION_CHANNEL::Process, true, output));
		if_fail_fe(lcpVSBuildSummary({(cnst llc::sc_t*)output.begin(), output.size()}, status, exitCode, text));
		if(operationStatus) *operationStatus = llc::failed(status) ? status : exitCode ? -1 : 0;
		rtrn lcpTextBody(body, text, llc::failed(status) || exitCode != 0);
	}
#ifdef LLC_WINDOWS
	try { status = automation(reader, action, text); }
	catch(cnst _com_error & error) { status = error.Error(); }
#else
	status = -1;
#endif
	if(llc::failed(status)) {
		if_fail_fe(text.clear());
		if_fail_fe(llc::append_strings(text, "Visual Studio ", action, " failed (", llc::str(status), "). Use vs list for an accessible instance; launch requires its exact solution/project/configuration. Close/restart require saved work and no build/debug session."));
	}
	if(operationStatus) *operationStatus = status;
	rtrn lcpTextBody(body, text, llc::failed(status));
}
