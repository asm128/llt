#include "llc_runtime.h"
#include "lcp_help.h"
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

sttc ::llc::err_t lcp_entry_point(::llc::SRuntimeValues & runtimeValues);
LLC_OS_ENTRY_POINT(::lcp_entry_point);

sttc	::llc::err_t	lcpWrite			(::llc::vcst_t message, FILE * stream = stdout) {
	if_zero_vw(0, message.size());
	if_true_ve(::llc::OS_ERROR, message.size() != fwrite(message.begin(), 1, message.size(), stream));
	if_true_ve(::llc::OS_ERROR, fflush(stream));
	rtrn message.size();
}

sttc	::llc::err_t	lcpRespond			(cnst ::llc::SJSONReader & reader, ::llc::err_t id, ::llc::vcst_t body) {
	::llc::string response = "{\"jsonrpc\":\"2.0\",\"id\":";
	if_fail_fe(::llc::jsonWrite(reader.Tree[(::llc::u2_t)id], reader.View, response));
	if_fail_fe(::llc::append_strings(response, ',', body, "}\n"));
	rtrn ::lcpWrite(response);
}

sttc	::llc::err_t	lcpText				(cnst ::llc::SJSONReader & reader, ::llc::err_t id, ::llc::vcst_t text, bool isError) {
	llc::string body;
	if_fail_fe(lcpTextBody(body, text, isError));
	rtrn ::lcpRespond(reader, id, body);
}

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

sttc	::llc::err_t	lcpTreeCall			(cnst ::llc::SJSONReader & reader, ::llc::err_t id, ::llc::SPathTree & tree) {
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
		rtrn ::lcpText(reader, id, "maxDepth must be between 1 and 32.", true);
	::llc::vcst_t format = "ascii";
	field = ::llc::jsonObjectValueGet(reader, (::llc::u2_t)args, "format");
	if(0 <= field) {
		if_fail_fe(::llc::jsonObjectGetString(reader, (::llc::u2_t)field, format));
	}
	if(format != "ascii" && format != "json")
		rtrn ::lcpText(reader, id, "format must be ascii or json.", true);
	cnst bool json = format == "json";
	::llc::string root, prefix, output;
	cnst ::llc::err_t normalized = ::llc::pathAbsolute(path, root);
	if(::llc::failed(normalized))
		rtrn ::lcpText(reader, id, "Cannot resolve the tree path.", true);
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
	rtrn ::lcpText(reader, id, output, incomplete);
}

sttc llc::err_t lcpShellString(llc::vcst_t & text, llc::string & cache) {
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

sttc llc::err_t lcpShellCall(cnst llc::SJSONReader & reader, llc::err_t id, llc::vcst_t shell) {
#ifndef LLC_WINDOWS
	rtrn lcpText(reader, id, "These shell tools require a Windows host; wsl uses the installed WSL distribution.", true);
#else
	llc::vcst_t script;
	if(llc::failed(llc::jsonExpressionResolve("params.arguments.command", reader, 0, script)))
		rtrn lcpText(reader, id, "command is required.", true);
	llc::string decoded;
	if(llc::failed(lcpShellString(script, decoded)))
		rtrn lcpText(reader, id, "Cannot decode command string.", true);
	if(script.size() && memchr(script.begin(), 0, script.size()))
		rtrn lcpText(reader, id, "command cannot contain NUL.", true);
	llc::b8_t elevated = false;
	cnst llc::err_t args = llc::jsonExpressionResolve("params.arguments", reader, 0);
	if_fail_fe(args);
	cnst llc::err_t field = llc::jsonObjectValueGet(reader, (llc::u2_t)args, "elevated");
	if(field >= 0) {
		if_fail_fe(llc::jsonObjectGetBoolean(reader, (llc::u2_t)field, elevated));
	}
	if(elevated && shell == "wsl")
		rtrn lcpText(reader, id, "WSL uses its configured Linux user; Windows elevation is not supported for this tool.", true);
	llc::string application = shell == "cmd" ? "cmd.exe" : shell == "powershell" ? "powershell.exe" : "wsl.exe";
	llc::string arguments, input, command, result;
	if(shell == "cmd") {
		if_fail_fe(llc::append_strings(arguments, "/D /S /C \"", script, '"'));
	}
	else if(shell == "powershell") {
		llc::apod<wchar_t> wide;
		if(script.size()) {
			cnst int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, script.begin(), (int)script.size(), 0, 0);
			if(count <= 0) rtrn lcpText(reader, id, "command must be UTF-8.", true);
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
		if(launched) rtrn lcpText(reader, id, "Elevated command launched; output and completion are not tracked.", false);
		cnst llc::u2_t error = GetLastError();
		if_fail_fe(llc::append_strings(result, "Elevated launch failed; Windows error ", llc::str(error)));
		rtrn lcpText(reader, id, result, true);
	}
	if_fail_fe(llc::append_strings(command, application, ' ', arguments));
	llc::au0_t output;
	llc::u2_t exitCode = 0;
	llc::SProcessManager processes;
	cnst llc::err_t process = processes.Create({}, command, {}, CREATE_NO_WINDOW);
	cnst llc::err_t status = llc::failed(process) ? process : processes.Execute((llc::u2_t)process, {(cnst llc::u0_t*)input.begin(), input.size()}, output, exitCode);
	if_fail_fe(llc::append_strings(result, "status ", llc::str(status), ", exit code ", llc::str(exitCode), '\n', llc::vcst_t{(cnst llc::sc_t*)output.begin(), output.size()}));
	rtrn lcpText(reader, id, result, llc::failed(status) || exitCode != 0);
#endif
}

sttc	::llc::err_t	lcpToolCall			(cnst ::llc::SJSONReader & reader, ::llc::err_t id, ::llc::SPathTree & tree) {
	::llc::vcst_t name = {};
	if_fail_fe(::llc::jsonExpressionResolve("params.name", reader, 0, name));
	if("help" == name) {
		llc::vcst_t path;
		cnst llc::err_t field = llc::jsonExpressionResolve("params.arguments.path", reader, 0);
		if(field >= 0 && llc::failed(llc::jsonObjectGetString(reader, (llc::u2_t)field, path)))
			rtrn lcpText(reader, id, "Help path must be a string.", true);
		cnst llc::vcst_t text = help(path);
		rtrn lcpText(reader, id, text.size() ? text : llc::vcst_t{"Unknown help path. Open help with an empty path for the index."}, !text.size());
	}
	if("tree" == name)
		rtrn ::lcpTreeCall(reader, id, tree);
	if(name == "cmd" || name == "powershell" || name == "wsl")
		rtrn lcpShellCall(reader, id, name);
	if("vs" != name)
		rtrn ::lcpRespond(reader, id, "\"error\":{\"code\":-32602,\"message\":\"Unknown tool. Call help for available commands.\"}");
	::llc::vcst_t action = {}, msbuild = {};
	if_fail_fe(::llc::jsonExpressionResolve("params.arguments.action", reader, 0, action));
	cnst bool build = "build" == action;
	::llc::string command = {}, result = {};
	if_fail_fe(::llc::jsonStringFormat(build
		? ::llc::vcst_t{"MSBuild.exe \"{params.arguments.solution}\" /t:\"{params.arguments.project}\""}
		: ::llc::vcst_t{"devenv.exe /Run \"{params.arguments.solution}\""}, reader, 0, command));
	if(build) {
		if_fail_fe(::llc::jsonExpressionResolve("params.arguments.msbuild", reader, 0, msbuild));
		::llc::vcst_t configuration = {}, platform = {};
		if_fail_fe(::llc::jsonExpressionResolve("params.arguments.configuration", reader, 0, configuration));
		if_fail_fe(::llc::split<::llc::sc_c>('|', configuration, configuration, platform));
		if_fail_fe(::llc::append_strings(command, " /p:Configuration=\"", configuration, "\" /p:Platform=\"", platform, '"'));
	}
	::llc::au0_t output = {};
	::llc::u2_t exitCode = 0;
	cnst ::llc::err_t status = build
		? ::llc::SProcessManager{}.Execute(msbuild, command, {}, output, exitCode, {})
		: ::llc::SProcessManager{}.Launch({}, command, {});
	if_fail_fe(::lcpWrite({(cnst ::llc::sc_t*)output.begin(), output.size()}, stderr));
	if_fail_fe(::llc::append_strings(result, "\"result\":{\"resultType\":\"complete\",\"content\":[{\"type\":\"text\",\"text\":\"Visual Studio ", action, ": status ", ::llc::str(status), ", exit code ", ::llc::str(exitCode), ". Output is on stderr.\"}],\"isError\":", ::llc::b82vcs(::llc::failed(status) || 0 != exitCode), '}'));
	rtrn ::lcpRespond(reader, id, result);
}

sttc	::llc::err_t	lcpRequest			(::llc::SJSONReader & reader, ::llc::vcst_t request, ::llc::SPathTree & tree) {
	if_fail_fe(reader.Reset());
	if(::llc::failed(::llc::jsonParse(reader, request, ::llc::JSON_PARSE_STRICT)))
		rtrn ::lcpWrite("{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32700,\"message\":\"Parse error\"}}\n");
	cnst ::llc::err_t id = ::llc::jsonObjectValueGet(reader, 0, "id");
	if(0 > id) rtrn 0;
	::llc::vcst_t method = {};
	if_fail_fe(::llc::jsonExpressionResolve("method", reader, 0, method));
	if("server/discover" == method)
		rtrn ::lcpRespond(reader, id, "\"result\":{\"resultType\":\"complete\",\"supportedVersions\":[\"2026-07-28\"],\"capabilities\":{\"tools\":{}},\"_meta\":{\"io.modelcontextprotocol/serverInfo\":{\"name\":\"lcp\",\"version\":\"0.1.0\"}},\"instructions\":\"Build a selected solution target with MSBuild or run a solution through devenv. Use forward slashes in paths.\",\"ttlMs\":3600000,\"cacheScope\":\"public\"}");
	if("initialize" == method)
		rtrn ::lcpRespond(reader, id, "\"result\":{\"protocolVersion\":\"2025-11-25\",\"capabilities\":{\"tools\":{}},\"serverInfo\":{\"name\":\"lcp\",\"version\":\"0.1.0\"},\"instructions\":\"Supply the MSBuild executable path for builds; running through devenv requires it on PATH.\"}");
	if("ping" == method)
		rtrn ::lcpRespond(reader, id, "\"result\":{\"resultType\":\"complete\"}");
	if("tools/list" == method)
		rtrn ::lcpRespond(reader, id, "\"result\":{\"resultType\":\"complete\",\"tools\":[{\"name\":\"tree\",\"title\":\"Directory tree\",\"description\":\"Scan a directory natively; return ASCII or compact JSON with file byte sizes. No shell or child process.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"description\":\"Directory path using forward slashes.\"},\"files\":{\"type\":\"boolean\",\"default\":false,\"description\":\"Include files, like DOS tree /F.\"},\"format\":{\"type\":\"string\",\"enum\":[\"ascii\",\"json\"],\"default\":\"ascii\",\"description\":\"JSON text contains root, columns, depth-first entries, truncated and incomplete.\"},\"maxDepth\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":32,\"default\":32,\"description\":\"Maximum directory levels to list; depth cutoffs are marked.\"}},\"required\":[\"path\"],\"additionalProperties\":false}},{\"name\":\"help\",\"title\":\"Help\",\"description\":\"Browse help indexes and command details by path. Empty path opens the root.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"description\":\"Full space-separated topic path, e.g. vs build or shell cmd. Omit for root.\"}},\"additionalProperties\":false}},{\"name\":\"vs\",\"title\":\"Visual Studio\",\"description\":\"Build one solution target with MSBuild or run the solution through devenv.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"action\":{\"type\":\"string\",\"enum\":[\"build\",\"run\"]},\"solution\":{\"type\":\"string\",\"description\":\"Solution path using forward slashes.\"},\"msbuild\":{\"type\":\"string\",\"description\":\"Full MSBuild executable path using forward slashes; required for build.\"},\"project\":{\"type\":\"string\",\"description\":\"MSBuild solution target name, including solution folders when applicable; required for build.\"},\"configuration\":{\"type\":\"string\",\"description\":\"Exact solution configuration such as Debug|x64; required for build.\"}},\"required\":[\"action\",\"solution\"],\"additionalProperties\":false}},{\"name\":\"cmd\",\"title\":\"Command Prompt\",\"description\":\"Execute caller-supplied cmd code. Optional elevation requests UAC; not a sandbox.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"command\":{\"type\":\"string\",\"description\":\"Shell command/script. Normal calls wait and return combined stdout/stderr and exit code.\"},\"elevated\":{\"type\":\"boolean\",\"default\":false,\"description\":\"Request Windows UAC elevation. Launch-only: no captured output or completion status.\"}},\"required\":[\"command\"],\"additionalProperties\":false}},{\"name\":\"powershell\",\"title\":\"PowerShell\",\"description\":\"Execute caller-supplied powershell code. Optional elevation requests UAC; not a sandbox.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"command\":{\"type\":\"string\",\"description\":\"Shell command/script. Normal calls wait and return combined stdout/stderr and exit code.\"},\"elevated\":{\"type\":\"boolean\",\"default\":false,\"description\":\"Request Windows UAC elevation. Launch-only: no captured output or completion status.\"}},\"required\":[\"command\"],\"additionalProperties\":false}},{\"name\":\"wsl\",\"title\":\"WSL Linux\",\"description\":\"Run /bin/sh in the installed default WSL distribution as its configured user. Does not install or convert WSL.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"command\":{\"type\":\"string\",\"description\":\"Shell command/script. Normal calls wait and return combined stdout/stderr and exit code.\"}},\"required\":[\"command\"],\"additionalProperties\":false}}],\"ttlMs\":3600000,\"cacheScope\":\"public\"}");
	if("tools/call" == method) {
		rtrn ::lcpToolCall(reader, id, tree);
	}
	rtrn ::lcpRespond(reader, id, "\"error\":{\"code\":-32601,\"message\":\"Method not found\"}");
}

sttc ::llc::err_t lcp_entry_point(::llc::SRuntimeValues & runtimeValues) {
	(void)runtimeValues;
	if_fail_fe(::llc::setupDefaultLogCallbacks());
	::llc::SJSONReader reader;
	::llc::SPathTree tree;
	::llc::string request;
	for(int character; EOF != (character = fgetc(stdin));) {
		if('\n' != character) {
			if_fail_fe(request.push_back((::llc::sc_t)character));
			continue;
		}
		if_fail_fe(::lcpRequest(reader, request, tree));
		if_fail_fe(request.clear());
	}
	if_true_ve(::llc::OS_ERROR, ferror(stdin));
	if(request.size()) {
		if_fail_fe(::lcpRequest(reader, request, tree));
	}
	rtrn 0;
}
