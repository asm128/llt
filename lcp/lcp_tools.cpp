#include "lcp_tools.h"
#include "llc_json.h"
#include "llc_file.h"
#include "llc_json_expression.h"
#include "llc_string_compose.h"
#include "llc_stdstring.h"

sttc llc::string Tools;
sttc llc::aobj<llc::string> ToolNames, ToolTitles;
stct SToolWord {
	llc::string Word;
	llc::au2_t Tools;
};
sttc llc::aobj<SToolWord> ToolWordDict;

sttc bool lcpWordCharacter(char character) {
	rtrn (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z')
		|| (character >= '0' && character <= '9') || character == '_' || character == '-';
}

sttc llc::err_t lcpWordAdd(llc::u2_t tool, llc::vcst_t word) {
	if(!word.size()) rtrn 0;
	llc::string normalized;
	if_fail_fe(normalized.append(word));
	if_fail_fe(llc::tolower(normalized));
	for(SToolWord & entry : ToolWordDict)
		if(entry.Word == normalized) {
			if(entry.Tools.find(tool) < 0) { if_fail_fe(entry.Tools.push_back(tool)); }
			rtrn 0;
		}
	SToolWord entry;
	if_fail_fe(entry.Word.append(normalized));
	if_fail_fe(entry.Tools.push_back(tool));
	rtrn ToolWordDict.push_back(entry);
}

sttc llc::err_t lcpWordIndex(llc::u2_t tool, llc::vcst_t text) {
	llc::u2_t begin = 0;
	for(llc::u2_t i = 0; i <= text.size(); ++i)
		if(i == text.size() || !lcpWordCharacter(text[i])) {
			if(i > begin) { if_fail_fe(lcpWordAdd(tool, {text.begin() + begin, i - begin})); }
			begin = i + 1;
		}
	rtrn 0;
}

llc::err_t lcpToolsLoad(llc::vcst_t fileName) {
	llc::SJSONFile file;
	if_fail_fe(llc::fileToMemory(fileName, file.Bytes));
	if_fail_fe(llc::jsonParse(file.Reader, file.Bytes, llc::JSON_PARSE_STRICT));
	if_fail_fe(Tools.clear());
	if_fail_fe(ToolWordDict.clear());
	if_fail_fe(Tools.append_string("\"result\":"));
	cnst llc::err_t array = llc::jsonExpressionResolve("tools", file.Reader, 0);
	if_fail_fe(array);
	cnst llc::err_t count = llc::jsonArraySize(file.Reader, array);
	if_fail_fe(count);
	if_fail_fe(llc::resize(count, ToolNames, ToolTitles));
	for(llc::u2_t i = 0; i < ToolNames.size(); ++i) {
		cnst llc::err_t item = llc::jsonArrayValueGet(file.Reader, array, i);
		if_fail_fe(item);
		if_fail_fe(llc::clear(ToolNames[i], ToolTitles[i]));
		if_fail_fe(llc::jsonStringFormat("{name}", file.Reader, item, ToolNames[i]));
		if_fail_fe(llc::jsonStringFormat("{title}", file.Reader, item, ToolTitles[i]));
		llc::string searchable;
		if_fail_fe(llc::jsonWrite(file.Reader[(llc::u2_t)item], file.Reader.View, searchable));
		if_fail_fe(lcpWordIndex(i, searchable));
	}

	rtrn llc::jsonWrite(file.Reader[0], file.Reader.View, Tools);
}

llc::vcst_t lcpTools() { rtrn Tools; }
llc::vcst_t lcpToolTitle(llc::vcst_t name) {
	for(llc::u2_t i = 0; i < ToolNames.size(); ++i)
		if(ToolNames[i] == name) rtrn ToolTitles[i];
	rtrn {};
}

llc::err_t lcpToolsAvailable(llc::vcst_t query, llc::string & output) {
	llc::au0_t matched;
	if_fail_fe(matched.resize(ToolNames.size()));
	matched.fill(query.size() ? 0 : 1);
	if(query.size()) {
		llc::aobj<llc::string> words;
		llc::u2_t begin = 0;
		for(llc::u2_t i = 0; i <= query.size(); ++i)
			if(i == query.size() || !lcpWordCharacter(query[i])) {
				if(i > begin) {
					llc::string word;
					if_fail_fe(word.append({query.begin() + begin, i - begin}));
					if_fail_fe(llc::tolower(word));
					if_fail_fe(words.push_back(word));
				}
				begin = i + 1;
			}
		if(!words.size()) matched.fill(1);
		for(cnst SToolWord & entry : ToolWordDict)
			for(cnst llc::string & word : words)
				if(entry.Word.find(word) >= 0) {
					for(llc::u2_t tool : entry.Tools) matched[tool] = 1;
					break;
				}
	}
	if_fail_fe(output.clear());
	if_fail_fe(output.append_string("available commands:"));
	for(llc::u2_t i = 0; i < ToolNames.size(); ++i)
		if(matched[i]) { if_fail_fe(llc::append_strings(output, '\n', ToolNames[i])); }
	rtrn output.size();
}
