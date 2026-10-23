# llt
Command line tools collection based on llc

## LLS command service

LCP is the MCP connector; headless LLS owns persistent jobs. `lls_dashboard` is a separate snapshot consumer: closing or restarting it does not shut down LLS. Start LCP with `--service=Release` for normal use or `--service=Debug` for development. Without the argument it operates locally. Debug and Release have separate service identities and loopback ports.

When an app-hosted MCP connection is unavailable, use the same connector directly from PowerShell:

```powershell
$client = 'D:/enterprise/programming_sciences/codebases/llb/llt/lcp/Invoke-LLS.ps1'
& $client -Tool help -Arguments @{path='vs build'}
& $client -Tool vs -Arguments @{
    action='build'
    msbuild='C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe'
    solution='D:/enterprise/programming_sciences/codebases/llb/llt/llt.slnx'
    project='llc'
    configuration='Debug-MT|x64'
}
```

The client defaults to the Release service. `-Configuration Debug` selects the Debug service; the tool's `configuration` argument independently selects the solution build configuration. Supply the installed MSBuild path explicitly. Builds still go through the solution, retaining its mappings and dependencies. The returned object contains the full MCP response, including `result.isError` and captured diagnostics in `result.content[0].text`.

`lls` lifecycle calls remain local to LCP, so a stopped service can be launched:

```powershell
& $client -Tool lls -Arguments @{
    action='launch'
    configuration='Release'
    executable='D:/enterprise/programming_sciences/codebases/llb/x64.Release-MT/lrt.exe'
}
& $client -Tool lls -Arguments @{action='status';configuration='Release'}
```

Launch does not duplicate an already-running instance. Shutdown/restart drain queued work; neither forces termination. Keep Release running while developing Debug. A timeout is an unknown outcome, not permission to replay a command. The one-call wrapper uses a fresh connector session, so prior job IDs cannot be queried through its next invocation; inspect the persistent dashboard or retain a connected MCP client for session-scoped job status.

For app integration, use `x64.Release-MT/lcp.exe`, argument `--service=Release`, working directory `x64.Release-MT`. Updating binaries does not reconnect an existing app session. Keep Release available while developing Debug; production can build Debug without depending on the Debug service. There is no Windows-service autorun yet. LLS runs as an ordinary interactive-user process, not SYSTEM; its trusted-local transport is unauthenticated and must not be exposed remotely or elevated.

Checkpoint verification: LLS, LCP and dashboard built in x64 Debug, Release, Debug-MT and Release-MT. Debug regression suites cover relay, separate lifetimes, persistent history and inboxes; Release dashboard reconnection was observed, not a full Release regression run. Intermittent disconnections and slow standalone dashboard loading with large history remain unresolved. Watchdog alerts and a dashboard service-restart button are not implemented; header Restart restarts only the dashboard. Win32 runtime behavior is unverified.

### Dashboard and diagnostics

Select `lls_dashboard` in `llt.slnx` to build/run the GUI. Its existing -MT launch settings use LRT with the `lls_dashboard` module; standalone configurations use the executable. The default service target matches Debug/Release; `--service=Debug` or `--service=Release` overrides only the target, not the dashboard build. LLS must be launched separately. Exit/Restart in the header affect the dashboard only.

`session` returns read-only `result.structuredContent` with both a service-session identifier and revision, identity, diagnostics/log path/status and job snapshots. Optional `session`/`revision` arguments suppress unchanged job lists. Polling does not inflate request counts. A consumer must replace history on session change and label its last snapshot stale on disconnection. This is the service/GUI boundary; GUI code must not access service SApplication or own its lifetime.

`failures` returns current-session completed failures across connector sessions, newest accepted job first (default 10; `limit` 1..100). Dashboard Results/Failures also include restored history. This command works with the one-call wrapper, unlike connector-scoped `status`.

Service diagnostics are appended under the service working directory's `lls_logs/session_<id>.log`, with a recent 64 KiB snapshot in `session`. Each completed job saves its combined stdout/stderr to a separate file; `outputPath` and `logStatus` report the result. Files are not automatically removed. Capture is not a live stdout stream: job output is saved at completion, and service diagnostics use LLC's logging callbacks. Always-on lifecycle records do not depend on Release info macros. The reusable diagnostics GUI is a separate workstream.

### Durable Results

New jobs are checkpointed under `../.lls-history/Debug` or `Release`: before queueing, before execution, and at completion before the reply. Records include command/request, output/response, process outcome, log path and origin session. Startup loads them into Results; restored records cannot be executed by the worker. A previously queued/executing record is displayed as **interrupted / outcome unknown**, not success or a fresh pending job. Saved elapsed time is frozen. `session.historyError` and the dashboard statistics expose persistence/recovery errors; unreadable records stay on disk. Failed acceptance persistence rejects the command before execution. A completion-save failure cannot undo executed work and is recorded in diagnostics.

Updates use a flushed temporary file and same-volume replacement. No history pruning or permanent-delete operation is implemented. A crash may leave a `.pending` file, which blocks replacing that exact record until inspected; do not delete records blindly. Old versions' transient Results cannot be reconstructed by this change; their existing log files remain. Loading currently reads all saved records into memory (64 MiB maximum per record); historical pagination/indexing and GUI layout persistence remain future work.

### Inboxes

The inbox regression suite was verified against Debug. Use explicit `-Configuration Debug` for the existing agent coordination store; Release has a separate store, not a fallback copy:

```powershell
& $client -Configuration Debug -Tool inbox -Arguments @{action='list';mailbox='conduit';unread=$true}
& $client -Configuration Debug -Tool inbox -Arguments @{action='profile';mailbox='conduit';task='Service delivery';status='testing';next='checkpoint'}
& $client -Configuration Debug -Tool inbox -Arguments @{action='send';mailbox='conduit';to='axiom';subject='Coordination';text='Ready for consumer integration.'}
```

Pablo uses `pablo`. Names are lowercase ASCII letters/digits/underscore/hyphen, 1..48 characters. Default folders are inbox/sent/trash; custom folders and filters are metadata-backed views. Each mailbox owns `metadata.json` and individual `messages/<id>.json` files under `../.lls-mail/Debug/box_<name>` (Release is isolated). Messages persist independently of the service and dashboard. Files are not automatically removed. This is a trusted-local store, not authenticated identity or private multi-user mail.

`read` is side-effect-free on message state; `mark` explicitly sets read/unread. `move`, `trash`, and `restore` preserve content. `filter_set` replaces first-match routing rules for future messages, never silently routes them to trash. `list` supports folder/query/unread/offset/limit; results omit message text until read. `profile` stores explicit task/status/next, echoed as one short line in send replies. `help inbox` lists exact arguments and bounds.

Delivery and sender-copy saves are separate: `delivered=true, sentCopy=false` is a delivered message with a missing sent copy, not permission to resend. No automatic retries, push wakeups, execution of message contents, or permanent-delete command. Agents must poll and treat messages as coordination, not new authorization. Dashboard inbox widgets remain a separate consumer task.
