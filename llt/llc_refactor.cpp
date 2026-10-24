#include "llc_refactor.h"
#include "llc_minmax.h"
#include <cstring>

namespace {
	bool startsWith(::llc::vcst_t text, ::llc::vcst_t prefix) {
		rtrn text.size() >= prefix.size() && 0 == ::memcmp(text.begin(), prefix.begin(), prefix.size());
	}

	::llc::vcst_t pathFromHeader(::llc::vcst_t line) {
		if(line.size() < 4) rtrn {};
		::llc::vcst_t path = {line.begin() + 4, line.size() - 4};
		cnst ::llc::err_t tab = path.find('\t');
		if(tab >= 0) path = {path.begin(), (::llc::u2_t)tab};
		if(path.size() > 2 && path[1] == '/') path = {path.begin() + 2, path.size() - 2};
		rtrn path;
	}

	::llc::err_t parseUnsigned(::llc::vcst_t text, ::llc::u2_t & cursor, ::llc::u2_t & value) {
		if_true_fe(cursor >= text.size() || text[cursor] < '0' || text[cursor] > '9');
		::llc::u3_t parsed = 0;
		while(cursor < text.size() && text[cursor] >= '0' && text[cursor] <= '9') {
			parsed = parsed * 10 + (text[cursor++] - '0');
			if_true_fe(parsed > ::llc::u2_t(-1));
		}
		value = (::llc::u2_t)parsed;
		rtrn 0;
	}

	::llc::err_t parseRange(::llc::vcst_t text, ::llc::u2_t & cursor, char marker, ::llc::u2_t & line, ::llc::u2_t & count) {
		while(cursor < text.size() && text[cursor] != marker) ++cursor;
		if_true_fe(cursor >= text.size());
		++cursor;
		if_fail_fe(parseUnsigned(text, cursor, line));
		count = 1;
		if(cursor < text.size() && text[cursor] == ',') { ++cursor; if_fail_fe(parseUnsigned(text, cursor, count)); }
		rtrn 0;
	}

	::llc::err_t appendLine(::llc::string & output, ::llc::vcst_t line) {
		if_fail_fe(output.append_string(line));
		rtrn output.push_back('\n');
	}

	bool identifierStart(char value) { rtrn (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') || value == '_'; }
	bool identifierPart(char value) { rtrn identifierStart(value) || (value >= '0' && value <= '9'); }
	bool digit(char value) { rtrn value >= '0' && value <= '9'; }

	::llc::err_t appendToken(::llc::string & output, ::llc::vcst_t token) {
		if(output.size()) if_fail_fe(output.push_back(' '));
		rtrn output.append_string(token);
	}
}

::llc::err_t llc::refactorDiffParse(vcst_t unifiedDiff, SRefactorPatch & output) {
	if_fail_fe(output.Hunks.clear());
	string fileBefore = {}, fileAfter = {};
	u2_t hunk = u2_t(-1);
	for(u2_t offset = 0; offset < unifiedDiff.size();) {
		u2_t end = offset;
		while(end < unifiedDiff.size() && unifiedDiff[end] != '\n') ++end;
		u2_t length = end - offset;
		if(length && unifiedDiff[offset + length - 1] == '\r') --length;
		cnst vcst_t line = {unifiedDiff.begin() + offset, length};
		offset = end < unifiedDiff.size() ? end + 1 : end;
		if(startsWith(line, "diff --git ")) { fileBefore.clear(); fileAfter.clear(); hunk = u2_t(-1); continue; }
		if(hunk == u2_t(-1) && startsWith(line, "--- ")) { fileBefore = pathFromHeader(line); continue; }
		if(hunk == u2_t(-1) && startsWith(line, "+++ ")) { fileAfter = pathFromHeader(line); continue; }
		if(startsWith(line, "@@ ")) {
			SRefactorHunk next = {};
			next.FileBefore = fileBefore; next.FileAfter = fileAfter;
			u2_t cursor = 0;
			if_fail_fe(parseRange(line, cursor, '-', next.LineBefore, next.CountBefore));
			if_fail_fe(parseRange(line, cursor, '+', next.LineAfter, next.CountAfter));
			if_fail_fe(output.Hunks.push_back(next));
			hunk = output.Hunks.size() - 1;
			continue;
		}
		if(hunk == u2_t(-1) || !line.size() || line[0] == '\\') continue;
		cnst vcst_t code = {line.begin() + 1, line.size() - 1};
		if(line[0] == ' ' || line[0] == '-') if_fail_fe(appendLine(output.Hunks[hunk].Before, code));
		if(line[0] == ' ' || line[0] == '+') if_fail_fe(appendLine(output.Hunks[hunk].After, code));
	}
	rtrn output.Hunks.size();
}

::llc::err_t llc::refactorCodeNormalize(vcst_t code, string & output) {
	if_fail_fe(output.clear());
	for(u2_t i = 0; i < code.size();) {
		cnst char current = code[i];
		if(current == '/' && i + 1 < code.size() && code[i + 1] == '/') {
			i += 2; while(i < code.size() && code[i] != '\n') ++i; continue;
		}
		if(current == '/' && i + 1 < code.size() && code[i + 1] == '*') {
			i += 2; while(i + 1 < code.size() && !(code[i] == '*' && code[i + 1] == '/')) ++i;
			i = ::llc::min(code.size(), i + 2); continue;
		}
		if(current == ' ' || current == '\t' || current == '\r' || current == '\n') { ++i; continue; }
		if(identifierStart(current)) {
			cnst u2_t begin = i++;
			while(i < code.size() && identifierPart(code[i])) ++i;
			if_fail_fe(appendToken(output, {code.begin() + begin, i - begin}));
			continue;
		}
		if(digit(current)) {
			++i; while(i < code.size() && (identifierPart(code[i]) || code[i] == '.' || code[i] == '\'')) ++i;
			if_fail_fe(appendToken(output, "<number>"));
			continue;
		}
		if(current == '"' || current == '\'') {
			cnst char quote = current; ++i;
			while(i < code.size()) {
				if(code[i] == '\\' && i + 1 < code.size()) { i += 2; continue; }
				if(code[i++] == quote) break;
			}
			if_fail_fe(appendToken(output, quote == '"' ? vcst_t{"<string>"} : vcst_t{"<char>"}));
			continue;
		}
		if_fail_fe(appendToken(output, {code.begin() + i, 1}));
		++i;
	}
	rtrn output.size();
}

::llc::err_t llc::refactorCodeSimilarity(vcst_t left, vcst_t right, f3_t & similarity) {
	string normalizedLeft = {}, normalizedRight = {};
	if_fail_fe(refactorCodeNormalize(left, normalizedLeft));
	if_fail_fe(refactorCodeNormalize(right, normalizedRight));
	aobj<vcst_t> tokensLeft = {}, tokensRight = {};
	if_fail_fe(split(vcs0_t{normalizedLeft.begin(), normalizedLeft.size()}, ' ', tokensLeft));
	if_fail_fe(split(vcs0_t{normalizedRight.begin(), normalizedRight.size()}, ' ', tokensRight));
	if(!tokensLeft.size() && !tokensRight.size()) { similarity = 1; rtrn 0; }
	apod<u0_t> used;
	if_fail_fe(used.resize(tokensRight.size(), 0));
	u2_t matches = 0;
	for(cnst auto tokenLeft : tokensLeft)
		for(u2_t i = 0; i < tokensRight.size(); ++i)
			if(!used[i] && tokenLeft == tokensRight[i]) { used[i] = 1; ++matches; break; }
	similarity = 2.0 * matches / (tokensLeft.size() + tokensRight.size());
	rtrn 0;
}

::llc::err_t llc::refactorFindBest(vcst_t code, view<cnst SRefactorPatch> corpus, SRefactorMatch & match) {
	match = {};
	for(u2_t patch = 0; patch < corpus.size(); ++patch)
	for(u2_t hunk = 0; hunk < corpus[patch].Hunks.size(); ++hunk) {
		cnst auto & candidate = corpus[patch].Hunks[hunk];
		if(!candidate.Before.size() || !candidate.After.size()) continue;
		f3_t score = 0;
		if_fail_fe(refactorCodeSimilarity(code, candidate.Before, score));
		if(match.Patch == u2_t(-1) || score > match.Score) match = {patch, hunk, score};
	}
	if_true_fe(match.Patch == u2_t(-1));
	rtrn 0;
}
