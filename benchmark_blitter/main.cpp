#include "blitter.h"
#include "llc_file.h"
#include "llc_crc.h"
#include "llc_parse.h"
#include "llc_runtime.h"
#include "llc_timer.h"
#include "llc_view.h"
#include <cstdio>

sttc ::llc::err_t benchmark_blitter_entry_point(::llc::SRuntimeValues & runtimeValues);
LLC_OS_ENTRY_POINT(::benchmark_blitter_entry_point);

LLC_USING_TYPEINT();
LLC_USING_VIEW();
LLC_USING_APOD();
LLC_USING_STRING();

sttc ::llc::err_t benchmark_blitter_entry_point(::llc::SRuntimeValues & runtimeValues) {
	cnst ::llc::SCommandLineArgs & args = runtimeValues.EntryPointArgs;
	::llc::vcst_t configPath, expectedPath, dbName, offsetText, limitText, requestsText, searchText;
	if_fail_fe(::llc::argsOptionValue(args, "config", configPath));
	if_fail_fe(::llc::argsOptionValue(args, "expected", expectedPath));
	if_fail_fe(::llc::argsOptionValue(args, "db", dbName));
	cnst bool search = 0 <= ::llc::argsOptionIndex(args, "search");
	if(search)
		if_fail_fe(::llc::argsOptionValue(args, "search", searchText));
	if(false == search) {
		if_fail_fe(::llc::argsOptionValue(args, "offset", offsetText));
		if_fail_fe(::llc::argsOptionValue(args, "limit", limitText));
	}
	if_fail_fe(::llc::argsOptionValue(args, "requests", requestsText));
	::blt::SBlitter app;
	app.Query.Database = dbName;
	app.Query.Command = search ? ::llc::vcst_t{"search"} : ::llc::vcst_t{"get"};
	app.Query.SearchText = searchText;
	if(false == search) {
		if_true_fe(::llc::parseIntegerDecimal(offsetText, app.Query.Range.Offset) != (::llc::err_t)offsetText.size());
		if_true_fe(::llc::parseIntegerDecimal(limitText, app.Query.Range.Count) != (::llc::err_t)limitText.size());
	}
	::llc::u3_t requests = 0;
	if_true_fe(::llc::parseIntegerDecimal(requestsText, requests) != (::llc::err_t)requestsText.size() || !requests || requests > 1000000);
	::llc::string expected, response;
	if_fail_fe(::llc::fileToMemory(expectedPath, expected));
	::llc::vcst_t expectedView;
	::llc::rtrim(expectedView, expected);
	::llc::STimer setupTimer;
	if_fail_fe(::blt::loadConfig(app, configPath));
	cnst double setupSeconds = setupTimer.Frame();
	cnst auto request = [&]() -> ::llc::err_t {
		response.clear();
		rtrn ::blt::queryProcess(app.LoadCache, app.Databases, app.ExpressionReader, app.Query, app.Folder, response) < 0 ? -1 : 0;
	};
	::llc::STimer firstTimer;
	if_fail_fe(request());
	cnst double firstSeconds = firstTimer.Frame();
	::llc::vcst_t responseView = {response.begin(), response.size()};
	if(expectedView != responseView) {
		::llc::fileFromMemory("benchmark_blitter_response.json", responseView);
		::std::fprintf(stderr, "response mismatch: expected=%u actual=%u\n", expectedView.size(), responseView.size());
		rtrn -1;
	}
	::llc::STimer warmTimer;
	::llc::u3_t checksum = 0;
	for(::llc::u3_t i = 0; i < requests; ++i) {
		if_fail_fe(request());
		::llc::u3_t crc = 0;
		if_fail_fe(::llc::crcGenerate(response.as<u0_c>(), crc));
		checksum += crc;
	}
	cnst double warmSeconds = warmTimer.Frame();
	responseView = {response.begin(), response.size()};
	if_true_fe(warmSeconds <= 0 || expectedView != responseView);
	::std::printf("phase,seconds,requests,requests_per_second,response_bytes,checksum_sum\n");
	::std::printf("setup,%.6f,0,0,%u,0\n", setupSeconds, response.size());
	::std::printf("first_request,%.6f,1,%.2f,%u,0\n", firstSeconds, 1.0 / firstSeconds, response.size());
	::std::printf("cached_requests,%.6f,%llu,%.2f,%u,%llu\n", warmSeconds, requests, requests / warmSeconds, response.size(), checksum);
	rtrn 0;
}
