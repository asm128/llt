#include "lcp_tools.h"
#include "llc_json.h"
#include "llc_file.h"
#include "llc_json_expression.h"

sttc llc::string Tools;
sttc llc::aobj<llc::string> ToolNames, ToolTitles;

llc::err_t lcpToolsLoad(llc::vcst_t fileName) {
	llc::SJSONFile file;
	if_fail_fe(llc::fileToMemory(fileName, file.Bytes));
	if_fail_fe(llc::jsonParse(file.Reader, file.Bytes, llc::JSON_PARSE_STRICT));
	if_fail_fe(Tools.clear());
	if_fail_fe(Tools.append("\"result\":"));
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
	}

	rtrn llc::jsonWrite(file.Reader[0], file.Reader.View, Tools);
}

llc::vcst_t lcpTools() { rtrn Tools; }
llc::vcst_t lcpToolTitle(llc::vcst_t name) {
	for(llc::u2_t i = 0; i < ToolNames.size(); ++i)
		if(ToolNames[i] == name) rtrn ToolTitles[i];
	rtrn {};
}
