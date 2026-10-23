#include "lls_explorer.h"
#include "llc_file.h"
#include "llc_string_compose.h"
#ifdef LLC_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <sys/stat.h>
#endif

sttc llc::err_t pathKind(llc::vcst_t path) {
	llc::string terminated = path;
#ifdef LLC_WINDOWS
	cnst DWORD attributes = GetFileAttributesA(terminated.begin());
	if(attributes == INVALID_FILE_ATTRIBUTES) rtrn -1;
	rtrn (attributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
#else
	struct stat info = {};
	if(stat(terminated.begin(), &info)) rtrn -1;
	rtrn S_ISDIR(info.st_mode) ? 1 : 0;
#endif
}

sttc llc::err_t scanBranch(SExplorerBranch & branch, llc::vcst_t path) {
	branch.Status = llc::pathList(branch.Tree, path, [](bool, llc::vcst_c &) -> llc::err_t { rtrn 2; });
	if_fail_fe(branch.Children.resize(branch.Tree.Name.size()));
	for(auto & child : branch.Children) child = (llc::u2_t)-1;
	rtrn 0; // A failed/partial scan stays visible, with its status.
}

sttc llc::err_t visibleRows(SExplorer & explorer, llc::u2_t branchId, llc::u2_t depth) {
	auto & branch = *explorer.Branches[branchId];
	for(int folders = 1; folders >= 0; --folders)
		for(llc::u2_t i = 0; i < branch.Tree.Name.size(); ++i) {
			if(branch.Tree.Info[i].Folder != folders) continue;
			if_fail_fe(explorer.Rows.push_back({branchId, i, depth}));
			cnst llc::u2_t child = branch.Children[i];
			if(child != (llc::u2_t)-1 && explorer.Branches[child]->Expanded) {
				if_fail_fe(visibleRows(explorer, child, depth + 1));
			}
		}
	rtrn 0;
}

sttc llc::err_t describePath(SExplorer & explorer, llc::vcst_t path, llc::err_t kind) {
	explorer.SelectedPath = path;
	if_fail_fe(explorer.Summary.clear());
	if_fail_fe(llc::append_strings(explorer.Summary, kind == 1 ? llc::vcst_t{"FOLDER\n\""} : llc::vcst_t{"FILE\n\""}, path, "\"\n"));
	if(kind == 0) {
		cnst llc::s3_t bytes = llc::fileSize(path);
		if(bytes >= 0) { if_fail_fe(llc::append_strings(explorer.Summary, "\nSize: ", llc::str(bytes), " bytes\n")); }
		else { if_fail_fe(explorer.Summary.append("\nSize unavailable.\n")); }
	}
	if_fail_fe(explorer.Crumbs.clear());
	llc::string normalized;
	if_fail_fe(llc::pathNormalize(path, normalized));
	for(llc::u2_t i = 0; i < normalized.size(); ++i)
		if(normalized[i] == '/' && i && normalized[i - 1] != '/') {
			if_fail_fe(explorer.Crumbs.push_back(llc::string{llc::vcst_t{normalized.begin(), i + 1}}));
		}
	if(kind == 1 && normalized.size() && normalized[normalized.size() - 1] != '/') {
		if_fail_fe(explorer.Crumbs.push_back(normalized));
	}
	rtrn explorer.Summary.append("\nClick folders to expand/collapse. Breadcrumbs change the scan root.\nRead-only: files are never executed or modified.");
}

llc::err_t llsExplorerOpen(SExplorer & explorer, llc::vcst_t path) {
	llc::string absolute;
	if_fail_fe(llc::pathAbsolute(path, absolute));
	cnst llc::err_t kind = pathKind(absolute);
	if(kind < 0) {
		explorer.Summary = "Path not found or inaccessible:\n";
		if_fail_fe(explorer.Summary.append(absolute));
		rtrn 0;
	}
	llc::vcst_t folder = absolute;
	if(!kind) { if_fail_fe(llc::pathDirectory(absolute, folder)); }
	if_fail_fe(explorer.Rows.clear());
	if_fail_fe(explorer.Branches.resize(1));
	if(!explorer.Branches[0]) { if_null_fe(explorer.Branches[0].create()); }
	if_fail_fe(scanBranch(*explorer.Branches[0], folder));
	explorer.Branches[0]->Expanded = true;
	if_fail_fe(visibleRows(explorer, 0, 0));
	explorer.Selected = (llc::u2_t)-1;
	if_fail_fe(describePath(explorer, absolute, kind));
	if(explorer.Branches[0]->Status < 0) {
		if_fail_fe(explorer.Summary.append("\nFolder scan incomplete or denied."));
	}
	rtrn 0;
}

llc::err_t llsExplorerSelect(SExplorer & explorer, llc::u2_t rowId) {
	if_true_fe(rowId >= explorer.Rows.size());
	cnst auto row = explorer.Rows[rowId];
	auto & branch = *explorer.Branches[row.Branch];
	llc::string path;
	if_fail_fe(llc::pathNameCompose(branch.Tree.Root, branch.Tree.Name[row.Entry], path));
	cnst bool folder = branch.Tree.Info[row.Entry].Folder;
	if(folder) {
		llc::u2_t & child = branch.Children[row.Entry];
		if(child == (llc::u2_t)-1) {
			cnst llc::err_t added = explorer.Branches.push_back({});
			if_fail_fe(added);
			child = (llc::u2_t)added;
			if_null_fe(explorer.Branches[child].create());
			if_fail_fe(scanBranch(*explorer.Branches[child], path));
		}
		else explorer.Branches[child]->Expanded = !explorer.Branches[child]->Expanded;
		if_fail_fe(explorer.Rows.clear());
		if_fail_fe(visibleRows(explorer, 0, 0));
	}
	explorer.Selected = rowId;
	if_fail_fe(describePath(explorer, path, folder));
	if(folder && explorer.Branches[branch.Children[row.Entry]]->Status < 0) {
		if_fail_fe(explorer.Summary.append("\nFolder scan incomplete or denied. Collapse and reopen the root to retry."));
	}
	rtrn 0;
}

llc::err_t llsExplorerLabel(cnst SExplorer & explorer, llc::u2_t rowId, llc::string & output) {
	cnst auto & row = explorer.Rows[rowId];
	cnst auto & branch = *explorer.Branches[row.Branch];
	if_fail_fe(output.clear());
	for(llc::u2_t i = 0; i < llc::min(row.Depth, 12U); ++i) { if_fail_fe(output.append("  ")); }
	cnst llc::u2_t child = branch.Children[row.Entry];
	cnst bool expanded = child != (llc::u2_t)-1 && explorer.Branches[child]->Expanded;
	if_fail_fe(output.append(branch.Tree.Info[row.Entry].Folder ? (expanded ? llc::vcst_t{"[-] "} : llc::vcst_t{"[+] "}) : llc::vcst_t{"    "}));
	rtrn output.append(branch.Tree.Name[row.Entry]);
}

llc::err_t llsPathLinks(llc::vcst_t text, llc::aobj<llc::vcst_t> & links) {
	if_fail_fe(links.clear());
	for(llc::u2_t i = 0; i + 2 < text.size(); ++i) {
		cnst bool drive = ((text[i] >= 'A' && text[i] <= 'Z') || (text[i] >= 'a' && text[i] <= 'z')) && text[i + 1] == ':' && (text[i + 2] == '/' || text[i + 2] == '\\');
		if(!drive) continue;
		llc::u2_t end = i + 3;
		cnst char quote = i && (text[i - 1] == '"' || text[i - 1] == '\'') ? text[i - 1] : 0;
		while(end < text.size() && text[end] != '\n' && text[end] != '\r' && text[end] != '"' && text[end] != '\'' && (quote || text[end] != ' ') && text[end] != '<' && text[end] != '>') ++end;
		while(end > i && (text[end - 1] == ',' || text[end - 1] == ')' || text[end - 1] == ';')) --end;
		llc::vcst_t path = {text.begin() + i, end - i};
		if(links.find(path) < 0) { if_fail_fe(links.push_back(path)); }
		i = end;
	}
	rtrn links.size();
}

#ifdef _DEBUG
llc::err_t llsExplorerTest() {
	llc::aobj<llc::vcst_t> links;
	if_fail_fe(llsPathLinks("Open \"D:/a folder/test.txt\" or D:/other/file.txt,", links));
	if_true_fe(links.size() != 2 || links[0] != "D:/a folder/test.txt" || links[1] != "D:/other/file.txt");
	if_fail_fe(llsPathLinks("No path here", links));
	if_true_fe(links.size());
	SExplorer explorer;
	if_fail_fe(llsExplorerOpen(explorer, "."));
	if_true_fe(explorer.Branches.size() != 1 || explorer.Branches[0]->Status < 0);
	for(cnst auto & row : explorer.Rows) { if_true_fe(row.Branch || row.Depth); }
	if_fail_fe(llsExplorerOpen(explorer, "lls_gui.json"));
	if_true_fef(explorer.Summary.find(llc::vcst_t{"Size: "}) < 0 || !explorer.Crumbs.size(), "Crumbs:%u, summary:%.*s", explorer.Crumbs.size(), LLC_VIEW_ARGS(explorer.Summary));
	// Cached synthetic child proves collapse/re-expand does not enumerate again.
	if_fail_fe(explorer.Branches.resize(2));
	if_null_fe(explorer.Branches[1].create());
	auto & root = *explorer.Branches[0];
	root.Tree.Root = "D:/";
	if_fail_fe(llc::clear(root.Tree.Name, root.Tree.Info, root.Children));
	if_fail_fe(root.Tree.Name.push_back("cached"));
	if_fail_fe(root.Tree.Info.push_back({0, 1}));
	if_fail_fe(root.Children.push_back(1));
	auto & child = *explorer.Branches[1];
	child.Tree.Root = "D:/cached";
	if_fail_fe(child.Tree.Name.push_back("example.txt"));
	if_fail_fe(child.Tree.Info.push_back({0, 0}));
	if_fail_fe(child.Children.push_back((llc::u2_t)-1));
	if_fail_fe(explorer.Rows.clear());
	if_fail_fe(visibleRows(explorer, 0, 0));
	if_true_fe(explorer.Rows.size() != 2 || explorer.Rows[1].Depth != 1);
	if_fail_fe(llsExplorerSelect(explorer, 0));
	if_true_fe(explorer.Rows.size() != 1 || child.Expanded);
	if_fail_fe(llsExplorerSelect(explorer, 0));
	if_true_fe(explorer.Rows.size() != 2 || !child.Expanded || explorer.Branches.size() != 2);
	llc::string label;
	if_fail_fe(llsExplorerLabel(explorer, 0, label));
	if_true_fe(label != "[-] cached");
	info_printf("%s", "Explorer checks passed: links, shallow scan, file size, breadcrumbs, cached collapse/expand.");
	rtrn 0;
}
#endif
