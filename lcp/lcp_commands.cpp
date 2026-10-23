#include "lcp_commands.h"
#include "lcp_help.h"
#include "lcp_tools.h"
#include "lcp_vs.h"
#include "lcp_service.h"
#include "lcp_text.h"
#include "llc_process.h"
#include "llc_path.h"
#include "llc_file.h"
#include "llc_cstdio.h"
#include "llc_json_expression.h"
#include "llc_string_compose.h"
#include "llc_encoding.h"
#include "llc_base64.h"

#ifdef LLC_WINDOWS
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#endif

sttc	::llc::err_t	lcpTreeASCII		(cnst ::llc::SPathTree & tree, cnst ::llc::SPathNode * parent, ::llc::u2_t depth, ::llc::string & prefix, ::llc::string & output) {
	if(0 == depth)
		rtrn ::llc::append_strings(output, prefix, "[depth limit]\n");
	cnst auto & nodes = parent ? parent->Children : tree.Tree;
	::llc::u2_t count = 0, emitted = 0;
	for(cnst auto & node : nodes)
		count += node->Parent == parent;
	cnst ::llc::u2_t prefixSize = prefix.size();
	for(int folders = 1; folders >= 0; --folders) {
		for(cnst auto & node : nodes) {
			if(node->Parent != parent || node->Info->Folder != folders)
				continue;
			cnst bool last = ++emitted == count;
			if_fail_fe(::llc::append_strings(output, prefix, last ? "\\---" : "+---", tree.Name[node->ObjectIndex]));
			if(folders) {
				if_fail_fe(output.push_back('/'));
			}
			if_fail_fe(output.push_back('\n'));
			if(folders) {
				if_fail_fe(::llc::append_strings(prefix, last ? "    " : "|   "));
				if_fail_fe(::lcpTreeASCII(tree, node, depth - 1, prefix, output));
				if_fail_fe(prefix.resize(prefixSize));
			}
		}
	}
	rtrn 0;
}

sttc	::llc::err_t	lcpTreeJSON			(cnst ::llc::SPathTree & tree, ::llc::vcs3_t bytes, bool truncated, bool incomplete, ::llc::string & output) {
	if_fail_fe(::llc::append_strings(output, "{\"root\":"));
	if_fail_fe(::lcpJSONString(output, tree.Root));
	if_fail_fe(::llc::append_strings(output, ",\"columns\":[\"depth\",\"folder\",\"name\",\"bytes\"],\"entries\":["));
	for(::llc::u2_t i = 0; i < tree.Info.size(); ++i) {
		if(i) {
			if_fail_fe(output.push_back(','));
		}
		if_fail_fe(::llc::append_strings(output, '[', ::llc::str((::llc::u2_t)tree.Info[i].NestLevel), ',', ::llc::b82vcs(tree.Info[i].Folder), ','));
		if_fail_fe(::lcpJSONString(output, tree.Name[i]));
		if_fail_fe(output.push_back(','));
		if(bytes[i] < 0) {
			if_fail_fe(::llc::append_strings(output, "null"));
		}
		else {
			if_fail_fe(::llc::append_strings(output, ::llc::str(bytes[i])));
		}
		if_fail_fe(output.push_back(']'));
	}
	rtrn ::llc::append_strings(output, "],\"truncated\":", ::llc::b82vcs(truncated), ",\"incomplete\":", ::llc::b82vcs(incomplete), '}');
}

::llc::err_t	lcpTreeCall			(cnst ::llc::SJSONReader & reader, ::llc::string & body, ::llc::SPathTree & tree) {
	::llc::vcst_t path = {};
	if_fail_fe(::llc::jsonExpressionResolve("params.arguments.path", reader, 0, path));
	cnst ::llc::err_t args = ::llc::jsonExpressionResolve("params.arguments", reader, 0);
	if_fail_fe(args);
	::llc::b8_t files = false;
	::llc::s3_t depth = 32;
	::llc::err_t field = ::llc::jsonObjectValueGet(reader, (::llc::u2_t)args, "files");
	if(0 <= field) {
		if_fail_fe(::llc::jsonObjectGetBoolean(reader, (::llc::u2_t)field, files));
	}
	field = ::llc::jsonObjectValueGet(reader, (::llc::u2_t)args, "maxDepth");
	if(0 <= field) {
		if_fail_fe(::llc::jsonObjectGetInteger(reader, (::llc::u2_t)field, depth));
	}
	if(depth < 1 || depth > 32)
		rtrn ::lcpTextBody(body, "maxDepth must be between 1 and 32.", true);
	::llc::vcst_t format = "ascii";
	field = ::llc::jsonObjectValueGet(reader, (::llc::u2_t)args, "format");
	if(0 <= field) {
		if_fail_fe(::llc::jsonObjectGetString(reader, (::llc::u2_t)field, format));
	}
	if(format != "ascii" && format != "json")
		rtrn ::lcpTextBody(body, "format must be ascii or json.", true);
	cnst bool json = format == "json";
	::llc::string root, prefix, output;
	cnst ::llc::err_t normalized = ::llc::pathAbsolute(path, root);
	if(::llc::failed(normalized))
		rtrn ::lcpTextBody(body, "Cannot resolve the tree path.", true);
	::llc::as3_t bytes;
	bool truncated = false, sizeFailed = false;
	cnst ::llc::u2_t relativeStart = root.size() + (root.size() && root[root.size() - 1] != '/');
	cnst ::llc::err_t status = ::llc::pathList(tree, root, [&](bool folder, ::llc::vcst_c & entry) -> ::llc::err_t {
		if(!folder && !files)
			rtrn 1;
		::llc::u2_t level = 0;
		for(::llc::u2_t i = relativeStart; i < entry.size(); ++i)
			level += entry[i] == '/';
		if(json) {
			cnst ::llc::s3_t size = folder ? -1 : ::llc::fileSize(entry);
			sizeFailed |= !folder && size < 0;
			if_fail_fe(bytes.push_back(size));
		}
		if(folder && level + 1 >= (::llc::u2_t)depth) {
			truncated = true;
			rtrn 2;
		}
		rtrn 0;
	});
	cnst bool incomplete = ::llc::failed(status) || sizeFailed;
	if(json) {
		if_fail_fe(::lcpTreeJSON(tree, bytes, truncated, incomplete, output));
	}
	else {
		if_fail_fe(::llc::append_strings(output, tree.Root, '\n'));
		if_fail_fe(::lcpTreeASCII(tree, nullptr, (::llc::u2_t)depth, prefix, output));
		if(incomplete) {
			if_fail_fe(::llc::append_strings(output, "[scan failed; listing may be incomplete]\n"));
		}
	}
	rtrn ::lcpTextBody(body, output, incomplete);
}

llc::err_t lcpShellCall(cnst llc::SJSONReader & reader, llc::string & body, llc::vcst_t shell, cnst FCommandExecute & execute) {
#ifndef LLC_WINDOWS
	(void)execute;
	rtrn lcpTextBody(body, "These shell tools require a Windows host; wsl uses the installed WSL distribution.", true);
#else
	llc::vcst_t script;
	if(llc::failed(llc::jsonExpressionResolve("params.arguments.command", reader, 0, script)))
		rtrn lcpTextBody(body, "command is required.", true);
	llc::string decoded;
	if(llc::failed(lcpStringDecode(script, decoded)))
		rtrn lcpTextBody(body, "Cannot decode command string.", true);
	if(script.size() && memchr(script.begin(), 0, script.size()))
		rtrn lcpTextBody(body, "command cannot contain NUL.", true);
	llc::b8_t elevated = false;
	cnst llc::err_t args = llc::jsonExpressionResolve("params.arguments", reader, 0);
	if_fail_fe(args);
	cnst llc::err_t field = llc::jsonObjectValueGet(reader, (llc::u2_t)args, "elevated");
	if(field >= 0) {
		if_fail_fe(llc::jsonObjectGetBoolean(reader, (llc::u2_t)field, elevated));
	}
	if(elevated && shell == "wsl")
		rtrn lcpTextBody(body, "WSL uses its configured Linux user; Windows elevation is not supported for this tool.", true);
	llc::string application = shell == "cmd" ? "cmd.exe" : shell == "powershell" ? "powershell.exe" : "wsl.exe";
	llc::string arguments, input, command, result;
	if(shell == "cmd") {
		if_fail_fe(llc::append_strings(arguments, "/D /S /C \"", script, '"'));
	}
	else if(shell == "powershell") {
		llc::apod<wchar_t> wide;
		if(script.size()) {
			cnst int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, script.begin(), (int)script.size(), 0, 0);
			if(count <= 0) rtrn lcpTextBody(body, "command must be UTF-8.", true);
			if_fail_fe(wide.resize(count));
			if_zero_fe(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, script.begin(), (int)script.size(), wide.begin(), count));
		}
		if_fail_fe(llc::append_strings(arguments, "-NoLogo -NoProfile -NonInteractive -EncodedCommand "));
		if_fail_fe(llc::base64Encode({(cnst llc::u0_t*)wide.begin(), wide.byte_count()}, arguments));
	}
	else {
		if_fail_fe(llc::append_strings(arguments, "--exec /bin/sh -s"));
		if_fail_fe(llc::append_strings(input, script, '\n'));
	}
	if(elevated) {
		SHELLEXECUTEINFOA launch = {sizeof(SHELLEXECUTEINFOA)};
		launch.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
		launch.lpVerb = "runas";
		launch.lpFile = application.begin();
		launch.lpParameters = arguments.begin();
		launch.nShow = SW_HIDE;
		cnst bool launched = 0 != ShellExecuteExA(&launch);
		if(launched) rtrn lcpTextBody(body, "Elevated command launched; output and completion are not tracked.", false);
		cnst llc::u2_t error = GetLastError();
		if_fail_fe(llc::append_strings(result, "Elevated launch failed; Windows error ", llc::str(error)));
		rtrn lcpTextBody(body, result, true);
	}
	if_fail_fe(llc::append_strings(command, application, ' ', arguments));
	llc::au0_t output;
	llc::u2_t exitCode = 0;
	llc::SProcessManager processes;
	llc::err_t status;
	cnst llc::vcu0_t stdinBytes = {(cnst llc::u0_t*)input.begin(), input.size()};
	if(execute)
		status = execute(command, stdinBytes, output, exitCode);
	else {
		cnst llc::err_t process = processes.Create({}, command, {}, CREATE_NO_WINDOW);
		status = llc::failed(process) ? process : processes.Execute((llc::u2_t)process, stdinBytes, output, exitCode);
	}
	if_fail_fe(llc::append_strings(result, "status ", llc::str(status), ", exit code ", llc::str(exitCode), '\n', llc::vcst_t{(cnst llc::sc_t*)output.begin(), output.size()}));
	rtrn lcpTextBody(body, result, llc::failed(status) || exitCode != 0);
#endif
}
