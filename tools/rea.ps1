# Project-local REA entry point. Paths are configured in ignored .local/rea-toolchain.json.
$ErrorActionPreference = 'Stop'
$reaRoot = Split-Path -Parent $PSScriptRoot
$reaConfig = Get-Content -LiteralPath (Join-Path $reaRoot '.local/rea-toolchain.json') -Raw | ConvertFrom-Json
$reaPrevious = @{}
foreach ($reaProperty in $reaConfig.environment.PSObject.Properties) {
    $reaPrevious[$reaProperty.Name] = [Environment]::GetEnvironmentVariable($reaProperty.Name, 'Process')
    [Environment]::SetEnvironmentVariable($reaProperty.Name, [string]$reaProperty.Value, 'Process')
}
$reaExitCode = 1
try {
    & $reaConfig.node $reaConfig.entry @args
    $reaExitCode = $LASTEXITCODE
} finally {
    foreach ($reaName in $reaPrevious.Keys) {
        [Environment]::SetEnvironmentVariable($reaName, $reaPrevious[$reaName], 'Process')
    }
}
exit $reaExitCode
