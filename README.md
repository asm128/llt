# llt
Command line tools collection based on llc

## LLS command service

LCP is the MCP connector; LLS owns the persistent jobs and dashboard. Start LCP with `--service=Release` for normal use or `--service=Debug` for development. Without the argument it operates locally. Debug and Release have separate service identities and loopback ports.

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

For app integration, use `x64.Release-MT/lcp.exe`, argument `--service=Release`, working directory `x64.Release-MT`. Updating binaries does not reconnect an existing app session. There is no inbox tool or Windows-service autorun yet. LLS currently runs as an ordinary interactive-user process, not SYSTEM; its trusted-local transport is unauthenticated and must not be exposed remotely or elevated.
