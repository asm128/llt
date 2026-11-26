#include "lls_gui_build.h"
#include "../lcp/lcp_text.h"

llc::err_t llsControlPalette(llc::SGUI & gui, llc::cid_t id) {
	auto & draw = gui.Controls.Draw[id];
	draw.UseNewPalettes = true;
	for(llc::u2_t i = 0; i < llc::GUI_CONTROL_PALETTE_COUNT; ++i) draw.Palettes[i] = (llc::s1_t)i;
	rtrn 0;
}

llc::err_t llsControl(llc::SGUI & gui, llc::cid_t & id, llc::vcst_t text, cnst SGUIFamily & family, bool label, llc::cid_t parent) {
	id = parent == llc::CID_INVALID ? llc::controlCreate(gui) : llc::controlCreateChild(gui, parent);
	if_fail_fe(id);
	gui.Controls.Placement[id].Margin = family.Margin;
	gui.Controls.Placement[id].Border = family.Border;
	gui.Controls.Text[id].Align = llc::ALIGN_CENTER_LEFT;
	gui.Controls.States[id].Mask = llc::bit_set(gui.Controls.States[id].Mask, llc::GUI_CONTROL_FLAG_DrawBorder, false == label);
	gui.Controls.States[id].Mask = llc::bit_set(gui.Controls.States[id].Mask, llc::GUI_CONTROL_FLAG_DrawClient, false == label);
	if_fail_fe(llsControlPalette(gui, id));
	rtrn llc::controlTextSet(gui, id, text);
}

llc::err_t llsControlPool(llc::SGUI & gui, llc::acid & ids, llc::u2_t count, cnst SGUIFamily & family, bool label, llc::cid_t parent) {
	while(ids.size() < count) {
		llc::cid_t id;
		if_fail_fe(llsControl(gui, id, {}, family, label, parent));
		if_true_block_log(error_printf, llc::failed(ids.push_back(id)), { if_fail_e(llc::controlDelete(gui, id)); rtrn -1; });
	}
	rtrn 0;
}

llc::err_t llsJSONLabels(cnst llc::SJSONReader & reader, llc::vcst_t expression, llc::vcst_t format, llc::aobj<llc::string> & labels, llc::au2_t & values) {
	cnst llc::err_t array = llc::jsonExpressionResolve(expression, reader, 0);
	if_fail_fe(array);
	cnst llc::err_t count = llc::jsonArraySize(reader, array);
	if_fail_fe(count);
	if_fail_fe(llc::resize(count, labels, values));
	for(llc::u2_t i = 0; i < labels.size(); ++i) {
		cnst llc::err_t item = llc::jsonArrayValueGet(reader, array, i);
		if_fail_fe(item);
		llc::string formatted, decoded;
		if_fail_fe(llc::jsonStringFormat(format, reader, item, formatted));
		llc::vcst_t text = formatted;
		if_fail_fe(lcpStringDecode(text, decoded));
		labels[i] = text;
		if_fail_fe(llc::jsonObjectGetAsInteger(reader, item, "value", values[i]));
	}
	rtrn labels.size();
}
