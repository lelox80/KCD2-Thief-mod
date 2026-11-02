param([ValidatePattern('^[a-z0-9][a-z0-9-]{0,63}$')][string]$OutputName='build-v021')
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$archive = Join-Path $taskRoot '.local/native/probe/deps/minhook-v1.3.4.zip'
$archivePin = '172708123daa0c98d20d3a980b16a50be14af243dc95dee6f79c24193ad010e4'
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $archivePin) { throw 'MinHook archive pin mismatch' }
$dependencyRoot = Join-Path $taskRoot '.local/native/ledge-probe/deps'
$dependency = Join-Path $dependencyRoot 'minhook-1.3.4'
New-Item -ItemType Directory -Path $dependencyRoot -Force | Out-Null
Expand-Archive -LiteralPath $archive -DestinationPath $dependencyRoot -Force
$output = Join-Path $taskRoot ('.local/native/ledge-probe/'+$OutputName)
New-Item -ItemType Directory -Path $output -Force | Out-Null
$vcvars = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
if (!(Test-Path -LiteralPath $vcvars)) { throw 'Expected inspected Visual Studio toolchain is absent' }
$batch = @"
@echo off
call "$vcvars" >nul
if errorlevel 1 exit /b 1
cd /d "$output"
cl /nologo /c /O2 /MT /W3 /I"$dependency\include" "$dependency\src\buffer.c" "$dependency\src\hook.c" "$dependency\src\trampoline.c" "$dependency\src\hde\hde64.c"
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /MT /W4 /WX /EHsc /LD /I"$dependency\include" "$taskRoot\native\LedgeProbe\Admission.cpp" buffer.obj hook.obj trampoline.obj hde64.obj /link /OUT:LedgeProbe.dll /INCREMENTAL:NO
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /MT /W4 /WX /EHsc /I"$dependency\include" "$taskRoot\native\LedgeProbe\AdmissionTest.cpp" buffer.obj hook.obj trampoline.obj hde64.obj /link /OUT:LedgeProbeTest.exe /INCREMENTAL:NO
exit /b %errorlevel%
"@
$batchPath = Join-Path $output 'build.cmd'
[IO.File]::WriteAllText($batchPath, $batch, [Text.Encoding]::ASCII)
& cmd.exe /d /c $batchPath
if ($LASTEXITCODE -ne 0) { throw 'Native ledge probe build failed' }
Copy-Item -LiteralPath (Join-Path $dependency 'LICENSE.txt') -Destination (Join-Path $output 'MinHook-LICENSE.txt')
Push-Location -LiteralPath $output
try {
    & (Join-Path $output 'LedgeProbeTest.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Native ledge probe fixture failed' }
} finally { Pop-Location }
& python (Join-Path $taskRoot 'tools/analyze_ledge_admission.py') (Join-Path $output 'admission-conformance.jsonl') --output (Join-Path $output 'admission-conformance-review.json')
if ($LASTEXITCODE -ne 0) { throw 'Native admission parser conformance failed' }
& python -c 'import json,sys; r=json.load(open(sys.argv[1])); assert not r["review_problems"] and r["allows_live_movement"] is False and r["framework_acceptance_records"]==1' (Join-Path $output 'admission-conformance-review.json')
if ($LASTEXITCODE -ne 0) { throw 'Native admission emitted capture did not validate' }
$sources = [ordered]@{}
foreach ($name in @(
    'native/LedgeProbe/Admission.cpp','native/LedgeProbe/AdmissionTest.cpp','native/LedgeProbe/Selection.h',
    'native/StateBridge/AuditQueue.h','native/StateBridge/WindowsTarget.h','native/StateBridge/Retail156.h',
    'tools/build_ledge_probe.ps1','tools/ledge_probe.py','tools/native_probe.py','tools/analyze_ledge_admission.py')) {
    $sources[$name] = (Get-FileHash -LiteralPath (Join-Path $taskRoot $name) -Algorithm SHA256).Hash.ToLowerInvariant()
}
$probeHash = (Get-FileHash -LiteralPath (Join-Path $output 'LedgeProbe.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
[ordered]@{
    mode='observe-only'; probe='ledge-admission'; probe_revision=4; fixtures='passed'; dll_sha256=$probeHash;
    target_sha256='bdf8f9e4a11257a72b64c84700e284c29e4c4ccaf5b8d4bfa7d0b2a7294479f7';
    selection_rva='0x831eac'; return_rva='0x831ccd'; prologue='488bc448895810488970184889782055';
    minhook_sha256=$archivePin; record_limit=60000; sources=$sources;
    hooks=@(
        @{name='attempt';rva='0x831b20';prologue='488bc448895808488970184889782055'},
        @{name='selection';rva='0x831eac';prologue='488bc448895810488970184889782055'},
        @{name='query';rva='0x832188';prologue='40534883ec40488bc1488bda4883c118'},
        @{name='framework';rva='0x85612c';prologue='48895c24104889742418574883ec2048'}
    );
    built=(Get-Date).ToUniversalTime().ToString('o')
} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'probe-build.json') -Encoding UTF8
Write-Output "NATIVE LEDGE PROBE BUILD PASS: $probeHash"
