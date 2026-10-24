#include "llc_path.h"

#ifndef LLS_EXPLORER_H
#define LLS_EXPLORER_H

stct SExplorerBranch {
	llc::SPathTree Tree;
	llc::au2_t Children;
	llc::err_t Status = 0;
	bool Expanded = true;
};
stct SExplorerRow {
	llc::u2_t Branch = 0, Entry = 0, Depth = 0;
};
stct SExplorer {
	llc::apobj<SExplorerBranch> Branches;
	llc::apod<SExplorerRow> Rows;
	llc::aobj<llc::string> Crumbs;
	llc::string SelectedPath, Summary;
	llc::u2_t Selected = (llc::u2_t)-1;
	bool SelectedFolder = false;
};

llc::err_t llsExplorerOpen(SExplorer & explorer, llc::vcst_t path);
llc::err_t llsExplorerSelect(SExplorer & explorer, llc::u2_t row);
llc::err_t llsExplorerLabel(cnst SExplorer & explorer, llc::u2_t row, llc::string & output);
llc::err_t llsPathLinks(llc::vcst_t text, llc::aobj<llc::vcst_t> & links);
#ifdef _DEBUG
llc::err_t llsExplorerTest();
#endif

#endif
