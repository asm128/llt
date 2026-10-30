#include "llc_string_compose.h"

#ifndef LCP_TEXT_H
#define LCP_TEXT_H

#include "llc_json.h"
#include "llc_encoding.h"

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
	if_fail_fe(body.append_string("\"result\":{\"resultType\":\"complete\",\"content\":[{\"type\":\"text\",\"text\":"));
	if_fail_fe(lcpJSONString(body, text));
	rtrn llc::append_strings(body, "}],\"isError\":", llc::b82st(isError), '}');
}

stin llc::err_t lcpStringDecode(llc::vcst_t & text, llc::string & cache) {
	if(-1 == text.find('\\')) rtrn 0;
	for(llc::u2_t i = 0; i < text.size(); ++i) {
		llc::sc_t value = text[i];
		if(value == '\\') {
			if_true_fe(++i >= text.size());
			value = text[i];
			if(value == 'u') {
				llc::u2_t codePoint;
				if_fail_fe(llc::jsonToCodePoint({text.begin() + i + 1, text.size() - i - 1}, codePoint));
				if_fail_fe(llc::utf8FromCodePoint(codePoint, cache));
				i += codePoint > 0xFFFF ? 10 : 4;
				continue;
			}
			switch(value) {
			case 'b': value = '\b'; break; case 'f': value = '\f'; break;
			case 'n': value = '\n'; break; case 'r': value = '\r'; break; case 't': value = '\t'; break;
			case '"': case '\\': case '/': break;
			default: rtrn -1;
			}
		}
		if_fail_fe(cache.push_back(value));
	}
	text = cache;
	rtrn 0;
}

#endif
