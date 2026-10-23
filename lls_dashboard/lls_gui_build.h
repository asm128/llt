#include "llc_gui.h"
#include "llc_json_expression.h"

#ifndef LLS_GUI_BUILD_H
#define LLS_GUI_BUILD_H

stct SGUIFamily {
	llc::rectu1_t Margin = {8, 8, 4, 4}, Border = {1, 1, 1, 1};
};
llc::err_t llsControl(llc::SGUI & gui, llc::cid_t & id, llc::vcst_t text, cnst SGUIFamily & family, bool label = false);
llc::err_t llsControlPool(llc::SGUI & gui, llc::acid & ids, llc::u2_t count, cnst SGUIFamily & family, bool label = false);
llc::err_t llsJSONLabels(cnst llc::SJSONReader & reader, llc::vcst_t expression, llc::vcst_t format, llc::aobj<llc::string> & labels, llc::au2_t & values);

#endif
