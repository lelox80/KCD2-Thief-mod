param([ValidatePattern('^[a-z0-9][a-z0-9-]{0,63}$')][string]$OutputName='build-v026',[switch]$Collider,[switch]$CapsuleControl,[switch]$GroundCapture,[switch]$TallRoute)
$ErrorActionPreference = 'Stop'
if ($Collider -and !$OutputName.StartsWith('collider-')) { throw 'Use a collider- output name' }
if ($CapsuleControl -and ($Collider -or !$OutputName.StartsWith('capsule-'))) { throw 'Use a separate capsule- output name' }
$define = if ($CapsuleControl) { '/DKT_CAPSULE_CONTROL' } else { '' }
if ($GroundCapture) {
    if (!$Collider -or $CapsuleControl) { throw 'Ground capture requires the collider probe' }
    $define = '/DKT_GROUND_CAPTURE'
}
if ($CapsuleControl) { $Collider = $true }
$source = if ($Collider) { 'Collider' } else { 'Probe' }
$binary = if ($Collider) { 'ColliderProbe' } else { 'ClearanceProbe' }
$probe = if ($Collider) { 'player-collider' } else { 'clearance' }
$loader = if ($Collider) { 'collider_probe.py' } else { 'clearance_probe.py' }
$limit = if ($Collider) { 128 } else { 4096 }
$revision=1
if ($CapsuleControl) { $binary='CapsuleControl'; $probe='capsule-control'; $loader='capsule_control.py'; $revision=2 }
if ($TallRoute) {
    if (!$CapsuleControl) { throw 'Tall route requires the capsule control probe' }
    $define += ' /DKT_TALL_ROUTE'
    $loader='tall_route.py'
    $revision=3
}
if ($GroundCapture) { $loader='ground_capture.py'; $revision=2 }
$hook = if ($Collider) { @{name='request';rva='0x837e7c';prologue='48895c240848896c2410488974241857'} } else {
    @{name='sweep';rva='0x8115bc';prologue='488bc448895808488968104889701848'}
}
$root = Split-Path -Parent $PSScriptRoot
$archive = Join-Path $root '.local/native/probe/deps/minhook-v1.3.4.zip'
$pin = '172708123daa0c98d20d3a980b16a50be14af243dc95dee6f79c24193ad010e4'
if ((Get-FileHash -LiteralPath $archive).Hash.ToLowerInvariant() -ne $pin) { throw 'MinHook archive pin mismatch' }
$buildRoot = Join-Path $root '.local/native/clearance-probe'
$output = Join-Path $buildRoot $OutputName
$dependency = Join-Path $buildRoot 'deps/minhook-1.3.4'
New-Item -ItemType Directory -Path $output -Force | Out-Null
Expand-Archive -LiteralPath $archive -DestinationPath (Split-Path -Parent $dependency) -Force
$vcvars = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
if (!(Test-Path -LiteralPath $vcvars)) { throw 'Expected Visual Studio toolchain is absent' }
$batch = @"
@echo off
call "$vcvars" >nul
if errorlevel 1 exit /b 1
cd /d "$output"
cl /nologo /c /O2 /MT /W3 /I"$dependency\include" "$dependency\src\buffer.c" "$dependency\src\hook.c" "$dependency\src\trampoline.c" "$dependency\src\hde\hde64.c"
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /MT /W4 /WX /EHsc $define /LD /I"$dependency\include" "$root\native\ClearanceProbe\$source.cpp" buffer.obj hook.obj trampoline.obj hde64.obj /link /OUT:$binary.dll /INCREMENTAL:NO
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /MT /W4 /WX /EHsc $define /I"$dependency\include" "$root\native\ClearanceProbe\${source}Test.cpp" buffer.obj hook.obj trampoline.obj hde64.obj /link /OUT:${binary}Test.exe /INCREMENTAL:NO
exit /b %errorlevel%
"@
$batchPath = Join-Path $output 'build.cmd'
[IO.File]::WriteAllText($batchPath, $batch, [Text.Encoding]::ASCII)
& cmd.exe /d /c $batchPath
if ($LASTEXITCODE -ne 0) { throw 'Clearance probe build failed' }
& (Join-Path $output "${binary}Test.exe")
if ($LASTEXITCODE -ne 0) { throw 'Clearance probe fixture failed' }
Copy-Item -LiteralPath (Join-Path $dependency 'LICENSE.txt') -Destination (Join-Path $output 'MinHook-LICENSE.txt')
$sources = [ordered]@{}
$files = @(
    'native/ClearanceProbe/Clearance.h',"native/ClearanceProbe/$source.cpp","native/ClearanceProbe/${source}Test.cpp",
    'native/ClearanceProbe/Capture.h','native/ClearanceProbe/Session.inl',
    'native/StateBridge/WindowsTarget.h','native/StateBridge/Retail156.h',
    'tools/build_clearance_probe.ps1',"tools/$loader",'tools/native_probe.py')
if ($CapsuleControl) { $files += @('tools/collider_probe.py','native/ClearanceProbe/Capsule.h','native/ClearanceProbe/CapsuleControl.h') }
if ($TallRoute) { $files += 'tools/capsule_control.py' }
if ($GroundCapture) {
    $files += @('tools/collider_probe.py','native/ClearanceProbe/Capsule.h','native/ClearanceProbe/CapsuleControl.h',
        'native/LedgeProbe/VaultTrial.h')
}
foreach ($name in $files) {
    $sources[$name] = (Get-FileHash -LiteralPath (Join-Path $root $name)).Hash.ToLowerInvariant()
}
$hash = (Get-FileHash -LiteralPath (Join-Path $output "$binary.dll")).Hash.ToLowerInvariant()
[ordered]@{
    mode='observe-only'; probe=$probe; probe_revision=$revision; fixtures='passed'; dll_sha256=$hash;
    target_sha256='bdf8f9e4a11257a72b64c84700e284c29e4c4ccaf5b8d4bfa7d0b2a7294479f7';
    minhook_sha256=$pin; record_limit=$limit; sources=$sources;
    hooks=@($hook);
} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'probe-build.json') -Encoding UTF8
Write-Output "CLEARANCE PROBE BUILD PASS: $hash"
