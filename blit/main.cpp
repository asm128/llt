#include "blitter.h"

#include "llc_keyval_view.h"
#include "llc_parse.h"
#include "llc_runtime.h"

#include <cstdio>

LLC_USING_TYPEINT();

::llc::err_t			jsonFail						(::llc::string & output, ::llc::vcst_t textToLog, ::llc::vcst_t status)	{
	error_printf("%.*s", LLC_VIEW_ARGS(textToLog));
	output											= ::llc::vcst_t{"Status: "};
	if_fail_fef(output.append(status)						, "%s", "Failed to generate output message.");
	if_fail_fef(output.append(::llc::vcst_t{"\r\nContent-Type: application/json\r\n\r\n"}), "%s", "Failed to generate output message.");
	if_fail_fef(output.append(::llc::vcst_t{"{ \"status\":"})		, "%s", "Failed to generate output message.");
	if_fail_fef(output.append(status)						, "%s", "Failed to generate output message.");
	if_fail_fef(output.append(::llc::vcst_t{", \"description\" :\""}), "%s", "Failed to generate output message.");
	if_fail_fef(output.append(textToLog)						, "%s", "Failed to generate output message.");
	if_fail_fef(output.append(::llc::vcst_t{"\" }\r\n"})				, "%s", "Failed to generate output message.");
	return 0;
}

sttc ::llc::err_t	requestEnvironment
	( cnst ::llc::SRuntimeValues						& runtimeValues
	, ::llc::aobj<::llc::keyval<::llc::vcst_t>>	& environment
	) {
	::llc::aobj<::llc::vcst_t> environmentText = runtimeValues.EntryPointArgs.Environment;
	if(0 == environmentText.size())
		if_fail_fe(::llc::viewsFromEnvp(environmentText, environ));
	for(cnst ::llc::vcst_t entryText : environmentText) {
		::llc::keyval<::llc::vcst_t> entry = {};
		if(0 <= ::llc::keyval_split(entryText, entry))
			if_fail_fe(environment.push_back(entry));
	}
	return 0;
}

sttc ::llc::err_t	requestQuerySplit(::llc::SHTTPAPIRequest & request) {
	if_zero_vi(0, request.QueryString.size());
	if_fail_fe(::llc::split(request.QueryString, '&', request.QueryStringElements));
	if_fail_fe(request.QueryStringKeyVals.resize(request.QueryStringElements.size()));
	for(u2_t iKeyVal = 0; iKeyVal < request.QueryStringKeyVals.size(); ++iKeyVal)
		if_fail_fe(::llc::keyval_split(request.QueryStringElements[iKeyVal], request.QueryStringKeyVals[iKeyVal]));
	return 0;
}

sttc ::llc::err_t	requestFromRuntime
	( cnst ::llc::SRuntimeValues	& runtimeValues
	, ::llc::SHTTPAPIRequest		& request
	, ::llc::string					& contentBody
	) {
	::llc::aobj<::llc::keyval<::llc::vcst_t>> environmentStorage;
	if_fail_fe(::requestEnvironment(runtimeValues, environmentStorage));
	cnst ::llc::view<cnst ::llc::keyval<::llc::vcst_t>> environment = environmentStorage;

	::llc::vcst_t value;
	request.IsCGIEnviron = 0 <= ::llc::find(::llc::vcst_t{"REMOTE_ADDR"}, environment, value);
	request.Method = ::llc::HTTP_METHOD_GET;
	if(0 <= ::llc::find(::llc::vcst_t{"REQUEST_METHOD"}, environment, value)) {
		cnst ::llc::HTTP_METHOD method = ::llc::get_value<::llc::HTTP_METHOD>(value);
		if(::llc::HTTP_METHOD_UNKNOWN != method)
			request.Method = method;
	}
	::llc::find(::llc::vcst_t{"SCRIPT_NAME"}, environment, request.Script);
	::llc::find(::llc::vcst_t{"PATH_INFO"}, environment, request.Path);
	::llc::find(::llc::vcst_t{"QUERY_STRING"}, environment, request.QueryString);
	::llc::find(::llc::vcst_t{"REMOTE_ADDR"}, environment, request.Ip);
	if(0 > ::llc::find(::llc::vcst_t{"REMOTE_PORT"}, environment, request.Port))
		::llc::find(::llc::vcst_t{"SERVER_PORT"}, environment, request.Port);

	if(false == request.IsCGIEnviron && runtimeValues.EntryPointArgs.Positionals.size()) {
		request.Path = runtimeValues.EntryPointArgs.Positionals[0];
		if(2 < request.Path.size() && '"' == request.Path[0] && '"' == request.Path[request.Path.size() - 1])
			if_fail_fe(request.Path.slice(request.Path, 1, request.Path.size() - 2));
		cnst ::llc::err_t queryStringStart = ::llc::find('?', request.Path);
		if(0 <= queryStringStart) {
			request.QueryString = {&request.Path[(u2_t)queryStringStart + 1], request.Path.size() - (u2_t)queryStringStart - 1};
			request.Path = {request.Path.begin(), (u2_t)queryStringStart};
		}
	}
	if(request.Path.size() && '/' == request.Path[0])
		if_fail_fe(request.Path.slice(request.Path, 1));
	if_fail_fe(::requestQuerySplit(request));

	::llc::vcst_t contentLengthText;
	if(0 <= ::llc::find(::llc::vcst_t{"CONTENT_LENGTH"}, environment, contentLengthText) && contentLengthText.size()) {
		u2_t contentLength = 0;
		if_fail_fe(::llc::parseIntegerDecimal(contentLengthText, contentLength));
		if_fail_fe(contentBody.resize(contentLength));
		if(contentLength != ::fread(contentBody.Data, 1, contentLength, stdin))
			return -1;
	}
	request.ContentBody = contentBody;
	return 0;
}

sttc ::llc::err_t	blitEntryPoint(::llc::SRuntimeValues & runtimeValues) {
	::llc::SHTTPAPIRequest requestReceived = {};
	::llc::string contentBody;
	if_fail_fe(::requestFromRuntime(runtimeValues, requestReceived, contentBody));

	::llc::string output;
	if(requestReceived.IsCGIEnviron) {
		if_fail_fe(output.append(::llc::vcst_t{"Content-type: application/json\r\n"}));
		if_fail_fe(output.append(::llc::vcst_t{"\r\n"}));
	}

	::blt::SBlitter app = {};
	if_fail_fef(::blt::requestProcess(app.ExpressionReader, app.Query, requestReceived, app.ExpansionKeyStorage), "%s", "Failed to process request.");
	if_fail_fef(::blt::loadConfig(app, "./blitter.json"), "%s", "Failed to load blitter configuration.");
	cnst s3_t result = ::blt::queryProcess(app.LoadCache, app.Databases, app.ExpressionReader, app.Query, app.Folder, output);
	if_true_fef(0 > result, "%s", "Failed to load blitter databases.");
	if(output.size()) {
#if defined(LLC_WINDOWS)
		OutputDebugStringA(output.begin());
		OutputDebugStringA("\n");
#endif
		if(output.size() != ::fwrite(output.begin(), 1, output.size(), stdout))
			return -1;
		::fflush(stdout);
	}
	return 0;
}

LLC_OS_ENTRY_POINT(::blitEntryPoint);
