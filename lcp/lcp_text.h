#include "llc_string_compose.h"

#ifndef LCP_TEXT_H
#define LCP_TEXT_H

stin	::llc::err_t	lcpJSONString		(::llc::string & body, ::llc::vcst_t text) {
	if_fail_fe(body.push_back('"'));
	for(cnst ::llc::sc_t character : text) {
		if('"' == character || '\\' == character) {
			if_fail_fe(body.push_back('\\'));
		}
		else if((::llc::u0_t)character < 0x20) {
			cnst ::llc::sc_t escaped[] = {'\\', 'u', '0', '0', "0123456789abcdef"[(::llc::u0_t)character >> 4], "0123456789abcdef"[character & 15]};
			if_fail_fe(body.append(escaped, ::llc::size(escaped)));
			continue;
		}
		if_fail_fe(body.push_back(character));
	}
	rtrn body.push_back('"');
}


stin llc::err_t lcpTextBody(llc::string & body, llc::vcst_t text, bool isError) {
	if_fail_fe(body.append("\"result\":{\"resultType\":\"complete\",\"content\":[{\"type\":\"text\",\"text\":"));
	if_fail_fe(lcpJSONString(body, text));
	rtrn llc::append_strings(body, "}],\"isError\":", llc::b82vcs(isError), '}');
}

#endif
