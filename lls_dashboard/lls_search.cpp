#include "llc_stdstring.h"
#include "llc_string_compose.h"
#include "lls_gui.h"
#include "../lcp/lcp_help.h"
#include "../lcp/lcp_tools.h"

struct SWordIndices {
	::llc::string			Text;
	::llc::au2_t			Indices;
};

struct SSearchCache {
	::llc::string				Query;
	::llc::aobj<::llc::vcst_t>	Words;
	::llc::au0_t				Matched;
};

// Indices address records in [0, itemCount); query and output storage must be separate from the cache.
sttc ::llc::err_t wordSearch
	( ::llc::vcst_t textToFind, ::llc::view<cnst SWordIndices> dictionary, ::llc::u2_t itemCount
	, ::llc::au2_t & output, SSearchCache & cache
	) {
	if_fail_fe(output.clear());
	if_fail_fe(cache.Words.clear());
	if_fail_fe(cache.Query.clear());
	if_fail_fe(cache.Query.append(textToFind));
	if_fail_fe(::llc::tolower(cache.Query));
	cnst ::llc::vcst_t query = cache.Query;
	if_fail_fe(::llc::split(query, ' ', cache.Words));
	if_fail_fe(cache.Matched.resize(itemCount));
	cache.Matched.fill(0);
	for(cnst SWordIndices & word : dictionary)
		for(::llc::vcst_t toFind : cache.Words) {
			if(0 > word.Text.find(toFind))
				continue;
			for(::llc::u2_t id : word.Indices)
				cache.Matched[id] = 1;
			break;
		}
	for(::llc::u2_t id = 0; id < itemCount; ++id) {
		if(!cache.Matched[id])
			continue;
		if_fail_fe(output.push_back(id));
	}
	rtrn output.size();
}

llc::err_t llsHelpFind(llc::vcst_t query, llc::au2_t & matches) {
	sttc llc::aobj<SWordIndices> titles, dictionary;
	sttc llc::u2_t itemCount = 0;
	sttc SSearchCache cache;
	sttc llc::au2_t contents;
	if(!itemCount) {
		llc::aobj<llc::vcst_t> paths;
		if_fail_fe(helpPaths(paths, true));
		if_fail_fe(titles.resize(paths.size()));
		if_fail_fe(dictionary.resize(paths.size()));
		for(llc::u2_t i = 0; i < paths.size(); ++i) {
			cnst llc::vcst_t page = help(paths[i], true);
			llc::vcst_t title, body;
			if_fail_fe(llc::split<llc::sc_c>('\n', page, title, body));
			llc::vcst_t family = paths[i], child;
			llc::split<llc::sc_c>(' ', paths[i], family, child);
			if_fail_fe(llc::append_strings(titles[i].Text, paths[i], ' ', title, ' ', lcpToolTitle(family)));
			if_fail_fe(llc::tolower(titles[i].Text));
			if_fail_fe(titles[i].Indices.push_back(i));
			if_fail_fe(dictionary[i].Text.append(body));
			if_fail_fe(llc::tolower(dictionary[i].Text));
			if_fail_fe(dictionary[i].Indices.push_back(i));
		}
		itemCount = paths.size();
	}
	if(query.size()) {
		// Keep catalog hierarchy within each tier: titles first, body-only matches last.
		if_fail_fe(wordSearch(query, titles, itemCount, matches, cache));
		if_fail_fe(wordSearch(query, dictionary, itemCount, contents, cache));
		for(llc::u2_t id : contents)
			if(matches.find(id) < 0) { if_fail_fe(matches.push_back(id)); }
		rtrn matches.size();
	}
	if_fail_fe(matches.clear());
	for(llc::u2_t i = 0; i < itemCount; ++i) {
		if_fail_fe(matches.push_back(i));
	}
	rtrn matches.size();
}

#ifdef _DEBUG
llc::err_t llsSearchTest() {
	cnst llc::u2_t itemCount = 3;
	cnst SWordIndices dictionary[] = {{"process", {2, 0}}, {"processor", {1, 0}}, {"service", {2}}};
	cnst struct { ::llc::vcst_t Query; ::llc::u2_t Expected[3], Count; } cases[] = {
		{"PROC", {0, 1, 2}, 3}, {"cessor", {0, 1}, 2},
		{"service processor", {0, 1, 2}, 3}, {"proc proc", {0, 1, 2}, 3},
		{"  PROC   service ", {0, 1, 2}, 3}, {"missing", {}, 0},
		{"", {}, 0}, {"   ", {}, 0},
		{"proc\tservice", {}, 0}, {"processor", {0, 1}, 2},
	};
	SSearchCache cache;
	::llc::au2_t output;
	for(cnst auto & test : cases) {
		if_fail_fe(output.push_back(99)); // Every search replaces the previous result.
		::llc::err_t count;
		if_fail_fe(count = ::wordSearch(test.Query, dictionary, itemCount, output, cache));
		if_true_fef(count != (::llc::err_t)test.Count || output.size() != test.Count, "Search '%.*s', count:%i, expected:%u.", LLC_VIEW_ARGS(test.Query), count, test.Count);
		for(::llc::u2_t i = 0; i < test.Count; ++i) {
			if_true_fef(output[i] != test.Expected[i], "Search '%.*s', result:%u, id:%u, expected:%u.", LLC_VIEW_ARGS(test.Query), i, output[i], test.Expected[i]);
		}
	}
	if_fail_fe(::wordSearch("proc", {}, itemCount, output, cache));
	if_true_fef(output.size(), "%s", "Empty dictionary produced matches.");
	if_fail_fe(::wordSearch("proc", {}, 0, output, cache));
	if_true_fef(output.size(), "%s", "Empty input produced matches.");
	llc::aobj<llc::vcst_t> paths;
	if_fail_fe(helpPaths(paths, true));
	if_fail_fe(llsHelpFind("BuIlD", output));
	if_true_fe(!output.size() || paths[output[0]] != "vs build");
	if_fail_fe(llsHelpFind("visu", output));
	bool foundBuild = false, foundRun = false;
	for(llc::u2_t id : output) { foundBuild |= paths[id] == "vs build"; foundRun |= paths[id] == "vs run"; }
	if_true_fe(!foundBuild || !foundRun);
	for(llc::u2_t i = 0; i < output.size(); ++i)
		for(llc::u2_t j = i + 1; j < output.size(); ++j) { if_true_fe(output[i] == output[j]); }
	if_fail_fe(llsHelpFind({}, output));
	if_true_fe(output.size() != paths.size());
	info_printf("Search tests: %u cases and help title-priority checks passed.", (::llc::u2_t)::llc::size(cases) + 2);
	rtrn 0;
}
#endif
