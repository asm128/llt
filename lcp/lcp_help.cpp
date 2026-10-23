#include "lcp_help.h"
#include "lcp_text.h"
#include "llc_file.h"

stct SHelpTopic {
	llc::string			Path, Text;
	llc::u0_t			Servers = 3;	// 1: LCP, 2: LLS.
};

sttc llc::aobj<SHelpTopic> HelpTopics;

llc::err_t helpLoad(llc::vcst_t fileName) {
	llc::SJSONFile file;
	if_fail_fe(llc::fileToMemory(fileName, file.Bytes));
	if_fail_fe(llc::jsonParse(file.Reader, file.Bytes, llc::JSON_PARSE_STRICT));
	cnst auto & reader = file.Reader;
	cnst llc::err_t count = llc::jsonArraySize(reader, 0);
	if_fail_fe(count);
	if_fail_fe(HelpTopics.resize(count));
	for(llc::u2_t i = 0; i < HelpTopics.size(); ++i) {
		cnst llc::err_t item = llc::jsonArrayValueGet(reader, 0, i);
		if_fail_fe(item);
		SHelpTopic & topic = HelpTopics[i];
		if_fail_fe(llc::clear(topic.Path, topic.Text));
		llc::vcst_t path;
		if_fail_fe(llc::jsonObjectGetAsString(reader, item, "path", path));
		llc::string decoded;
		if_fail_fe(lcpStringDecode(path, decoded));
		if_fail_fe(topic.Path.append(path));
		if_fail_fe(llc::jsonObjectGetAsInteger(reader, item, "servers", topic.Servers));
		cnst llc::err_t lines = llc::jsonObjectValueGet(reader, item, "lines");
		if_fail_fe(lines);
		cnst llc::err_t lineCount = llc::jsonArraySize(reader, lines);
		if_fail_fe(lineCount);
		for(llc::u2_t j = 0; j < (llc::u2_t)lineCount; ++j) {
			cnst llc::err_t line = llc::jsonArrayValueGet(reader, lines, j);
			if_fail_fe(line);
			if(reader.Token[line].Type == llc::JSON_TYPE_STRING) {
				llc::vcst_t text;
				if_fail_fe(llc::jsonObjectGetString(reader, line, text));
				if_fail_fe(decoded.clear());
				if_fail_fe(lcpStringDecode(text, decoded));
				if_fail_fe(topic.Text.append(text));
			}
			else {
				if_fail_fe(llc::jsonWrite(reader[line], reader.View, topic.Text));
			}
			if_fail_fe(topic.Text.push_back('\n'));
		}
	}
	rtrn 0;
}

llc::vcst_t help(llc::vcst_t path, bool service) {
	for(cnst SHelpTopic & topic : HelpTopics)
		if((topic.Servers & (service ? 2 : 1)) && topic.Path == path)
			rtrn topic.Text;
	rtrn {};
}

llc::err_t helpPaths(llc::aobj<llc::vcst_t> & paths, bool service) {
	if_fail_fe(paths.clear());
	for(cnst SHelpTopic & topic : HelpTopics)
		if(topic.Servers & (service ? 2 : 1)) {
			if_fail_fe(paths.push_back(topic.Path));
		}
	rtrn paths.size();
}
