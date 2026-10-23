#include "lcp_help.h"

stct SHelpTopic {
	llc::vcst_t		Path, Text;
	llc::u0_t		Servers = 3;	// 1: LCP, 2: LLS.
};

sttc cnst SHelpTopic HelpTopics[] = {
	{{}, R"help(LCP help - index
Open a topic with {"name":"help","arguments":{"path":"vs"}}
Use the full space-separated path shown below. Empty path returns here.

help  - How to navigate and call tools.
vs    - Visual Studio build/run actions.
tree  - Directory listing formats.
shell - CMD, PowerShell and WSL.
faq   - Common questions.
)help", 1},
	{{}, R"help(LLS help - index
Open a topic with {"name":"help","arguments":{"path":"vs"}}
Use the full space-separated path shown below. Empty path returns here.

help   - How to navigate and call tools.
vs     - Visual Studio build/run actions.
status - Inspect queued, executing and completed requests.
faq    - Common questions.
)help", 2},
	{"help", R"help(help - navigation
Optional argument: path (string, default empty).
{"name":"help","arguments":{}}
{"name":"help","arguments":{"path":"vs"}}
{"name":"help","arguments":{"path":"vs build"}}
Each index lists immediate children with full paths. Leaves show arguments,
examples, results and relevant limitations. Parent names the level above.
Paths are exact and case-sensitive. Unknown paths return a tool error.
Examples are tools/call params, not shell commands. Help never runs them.
Only tools implemented by this server appear in its index.
Parent: <root> (empty path).
)help"},
	{"vs", R"help(vs - index
vs build - Build one project target through its solution.
vs run   - Launch the legacy solution startup-project route.
{"name":"help","arguments":{"path":"vs build"}}
Parent: <root> (empty path).
)help"},
	{"vs build", R"help(vs build
Required: action="build", msbuild, solution, project, configuration.
msbuild: caller-supplied executable path; no automatic VS discovery.
solution: solution path using forward slashes.
project: solution target, including solution folders when needed; NOT project:Build.
configuration: exact Configuration|Platform, e.g. Debug|x64 or Debug-MT|x64.
{"name":"vs","arguments":{"action":"build","msbuild":"C:/path/to/MSBuild.exe","solution":"D:/enterprise/programming_sciences/codebases/llb/llt/llt.slnx","project":"lcp","configuration":"Debug|x64"}}
Replace the example MSBuild path with the installed executable.
Builds through the solution, preserving mappings and dependencies.
Result: process-operation status and child exit code; nonzero exit is a build failure.
Output goes to stderr after capture; stdout contains protocol responses.
No build timeout or cancellation. LCP handles calls synchronously.
LLS queues builds; use its status tool with the original request id while waiting.
Parent: vs.
)help"},
	{"vs run", R"help(vs run
Required: action="run", solution.
{"name":"vs","arguments":{"action":"run","solution":"D:/enterprise/programming_sciences/codebases/llb/llt/llt.slnx"}}
Legacy route: devenv.exe /Run launches the solution startup project.
Requires devenv.exe on the process search path; may open Visual Studio.
Success means launched, NOT application completed. No captured application exit code.
Explicit executable selection and debugger-launch overrides are not implemented.
Parent: vs.
)help"},
	{"status", R"help(status
LLS only. Required: id (original vs request id, integer or string; not a PID).
{"name":"status","arguments":{"id":6}}
Answers on the input thread without waiting behind a build.
structuredContent contains id, state, elapsedMs, status and exitCode.
state: queued, executing or completed. Executing is not proof of OS liveness/progress.
elapsedMs: time since acceptance, including queue time; frozen at completion.
status: process-operation result when completed; null beforehand.
exitCode: known build child exit code, otherwise null (including launch failure/run).
A successful status query does not mean the build succeeded.
Unknown/missing ids are errors. Use unique vs request ids within the server session;
numeric 6 differs from string "6". Retained records disappear on server restart.
Parent: <root> (empty path).
)help", 2},
	{"tree", R"help(tree - index
Native directory scan; no shell or file-content reads.
Required: path. Optional: files=false, maxDepth=32 (1..32), format="ascii".
tree ascii - DOS-style branches, like tree /F when files=true.
tree json  - Compact rows with file byte sizes.
{"name":"help","arguments":{"path":"tree json"}}
Each call rescans; storage is reused, not a cached filesystem snapshot.
Directory links follow the scanner; no cycle deduplication. Depth cutoffs are marked.
Parent: <root> (empty path).
)help", 1},
	{"tree ascii", R"help(tree ascii
Required: path. Optional: files=false, maxDepth=32 (1..32), format="ascii".
{"name":"tree","arguments":{"path":"D:/enterprise/programming_sciences/codebases/llb/llt/lcp","files":true,"maxDepth":4,"format":"ascii"}}
Returns ASCII branches here, not stderr. Directories precede files; each group
retains filesystem enumeration order. files=true includes files like DOS tree /F.
Depth limits are marked. Scan errors return isError and the available partial listing;
diagnostics go to stderr. No file contents, index or directory-size totals.
Parent: tree.
)help", 1},
	{"tree json", R"help(tree json
Required: path, format="json". Optional: files=false, maxDepth=32 (1..32).
{"name":"tree","arguments":{"path":"D:/enterprise/programming_sciences/codebases/llb/llt/lcp","files":true,"maxDepth":4,"format":"json"}}
Returned text is JSON with root once, columns=["depth","folder","name","bytes"],
entries, truncated and incomplete. Rows follow depth-first filesystem order.
Depth 0 is directly under root; names are basenames, not repeated full paths.
folder is boolean. bytes is file size, or null for folders/unavailable sizes.
files=true is required to include files; directory sizes are not summed.
truncated means a boundary folder was included without descending at maxDepth.
incomplete and isError indicate scan/size failure, retaining available entries.
No file contents or cached filesystem snapshot.
Parent: tree.
)help", 1},
	{"shell", R"help(shell - index
shell cmd        - Execute a CMD script.
shell powershell - Execute a PowerShell script.
shell wsl        - Execute /bin/sh in the default WSL distribution.
{"name":"help","arguments":{"path":"shell powershell"}}
These execute caller-supplied code, not a sandbox. Use only authorized commands.
Normal calls wait and return combined stdout/stderr, operation status and exit code.
No persistent session, interactive input, timeout or cancellation.
Commands inherit server working directory; change directories in the script if needed.
Parent: <root> (empty path).
)help", 1},
	{"shell cmd", R"help(shell cmd
Tool name: cmd. Required: command (string). Optional: elevated=false.
{"name":"cmd","arguments":{"command":"echo hello & exit /b 0"}}
Runs cmd.exe /D /S /C. Normal calls wait and return combined output and exit code.
elevated=true requests Windows UAC; approval launches without waiting/capture.
Elevated success means launched, not completed. Denied UAC is an error.
Windows host required. JSON command escapes are decoded locally.
Output encoding follows the producer; non-ASCII normalization is not implemented.
No persistent session, interactive input, timeout or cancellation.
Parent: shell.
)help", 1},
	{"shell powershell", R"help(shell powershell
Tool name: powershell. Required: command (string). Optional: elevated=false.
{"name":"powershell","arguments":{"command":"Write-Output 'hello'; exit 0"}}
{"name":"powershell","arguments":{"command":"Write-Output 'elevated'","elevated":true}}
Runs powershell.exe without profiles, noninteractively; accepts multiline scripts.
Normal calls wait and return combined output and exit code.
elevated=true requests UAC; approval launches without waiting/capture.
Elevated success means launched, not completed. Denied UAC is an error.
Windows host required. JSON command escapes are decoded locally.
Output encoding follows the producer; non-ASCII normalization is not implemented.
No persistent session, interactive input, timeout or cancellation.
Parent: shell.
)help", 1},
	{"shell wsl", R"help(shell wsl
Tool name: wsl. Required: command (string).
{"name":"wsl","arguments":{"command":"printf 'hello'; exit 0"}}
Runs wsl.exe --exec /bin/sh -s; supplies the script on stdin.
Uses the installed default distribution and its configured user, including WSL2.
Does not install/convert distributions or select root. Windows elevation is unsupported.
Normal calls wait and return combined output and exit code.
No persistent session, interactive input, timeout or cancellation.
If WSL_E_CONSOLE appears, the current Windows legacy console mode blocks WSL;
this command does not change host settings. Output encoding is not normalized.
Parent: shell.
)help", 1},
	{"faq", R"help(faq
How do I call a tool?
Send tools/call with the example object as params.
Which path spelling?
Forward slashes for filesystem paths. Help topics use spaces, e.g. "vs build".
Where is build output?
stderr; stdout is reserved for protocol responses.
Does success mean the command worked?
Check both process-operation status and child exit code. Launch-only operations
cannot report completion. A successful status query only means the lookup succeeded.
Can I inspect a running build?
LLS exposes status; LCP is synchronous and does not expose it.
Will help execute its examples?
No. Help only returns static text; invalid paths do not dispatch commands.
Are file-content search/index commands ready?
No; this catalog only describes implemented tools.
Parent: <root> (empty path).
)help"},
};

llc::vcst_t help(llc::vcst_t path, bool service) {
	for(cnst SHelpTopic & topic : HelpTopics)
		if((topic.Servers & (service ? 2 : 1)) && topic.Path == path)
			rtrn topic.Text;
	rtrn {};
}
