param(
    [Parameter(Mandatory)][string]$Tool,
    [hashtable]$Arguments = @{},
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [ValidateSet('Default','Debug','Debug-MT','Release','Release-MT')][string]$ClientConfiguration = 'Default',
    [ValidateRange(1,3600)][int]$TimeoutSeconds = 1800
)
$ErrorActionPreference = 'Stop'
# Same MCP connector and service as the app integration; no local command fallback.
$client = if($ClientConfiguration -ne 'Default') { $ClientConfiguration } else { "$Configuration-MT" }
$directory = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "../../../../../../operations/lls/x64.$client"))
$start = [Diagnostics.ProcessStartInfo]::new((Join-Path $directory 'lcp.exe'))
$start.WorkingDirectory = $directory
$start.Arguments = "--service=$Configuration"
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$start.RedirectStandardInput = $true
$start.RedirectStandardOutput = $true
$start.RedirectStandardError = $true
$start.StandardInputEncoding = [Text.UTF8Encoding]::new($false)
$start.StandardOutputEncoding = [Text.UTF8Encoding]::new($false)
$process = [Diagnostics.Process]::Start($start)
$errors = $process.StandardError.ReadToEndAsync()
try {
    $id = [Guid]::NewGuid().ToString('N')
    $request = @{jsonrpc='2.0';id=$id;method='tools/call';params=@{name=$Tool;arguments=$Arguments}}
    $process.StandardInput.WriteLine(($request | ConvertTo-Json -Depth 30 -Compress))
    $process.StandardInput.Close()
    $line = $process.StandardOutput.ReadLineAsync()
    if(-not $line.Wait($TimeoutSeconds * 1000)) {
        throw "LLS reply timed out. No retry or termination was performed. Inspect the service's Results before retrying; connector PID $($process.Id)."
    }
    if(-not $line.Result) {
        $diagnostic = if($errors.IsCompleted) { $errors.GetAwaiter().GetResult() } else { 'Connector produced no reply.' }
        throw "LLS connector closed: $diagnostic"
    }
    $reply = $line.Result | ConvertFrom-Json
    if($reply.id -ne $id) { throw 'Unexpected LLS response ID.' }
    if(-not $process.WaitForExit(10000)) { throw "Reply received but connector $($process.Id) has not exited. No retry performed." }
    if($process.ExitCode -ne 0) { throw $errors.GetAwaiter().GetResult() }
    $reply
}
finally {
    # Disposing a Process wrapper never terminates the service or an in-flight job.
    $process.Dispose()
}
