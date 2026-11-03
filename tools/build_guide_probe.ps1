param([ValidatePattern('^[a-z0-9][a-z0-9-]{0,63}$')][string]$OutputName='build-v022')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$archive=Join-Path $taskRoot '.local/native/probe/deps/minhook-v1.3.4.zip'
$archivePin='172708123daa0c98d20d3a980b16a50be14af243dc95dee6f79c24193ad010e4'
if((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $archivePin){throw 'MinHook archive pin mismatch'}
$dependencyRoot=Join-Path $taskRoot '.local/native/guide-probe/deps'
New-Item -ItemType Directory -Path $dependencyRoot -Force | Out-Null
Expand-Archive -LiteralPath $archive -DestinationPath $dependencyRoot -Force
$dependency=Join-Path $dependencyRoot 'minhook-1.3.4'
$output=Join-Path $taskRoot ('.local/native/guide-probe/'+$OutputName)
New-Item -ItemType Directory -Path $output -Force | Out-Null
$vcvars='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
if(!(Test-Path -LiteralPath $vcvars)){throw 'Expected inspected MSVC toolchain missing'}
$batch=@"
@echo off
call "$vcvars" >nul
if errorlevel 1 exit /b 1
cd /d "$output"
cl /nologo /c /O2 /MT /W3 /I"$dependency\include" "$dependency\src\buffer.c" "$dependency\src\hook.c" "$dependency\src\trampoline.c" "$dependency\src\hde\hde64.c"
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /MT /W4 /WX /EHsc /LD /I"$dependency\include" "$taskRoot\native\GuideProbe\Probe.cpp" buffer.obj hook.obj trampoline.obj hde64.obj /link /OUT:GuideProbe.dll /INCREMENTAL:NO
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /MT /W4 /WX /EHsc /I"$dependency\include" "$taskRoot\native\GuideProbe\ProbeTest.cpp" buffer.obj hook.obj trampoline.obj hde64.obj /link /OUT:GuideProbeTest.exe /INCREMENTAL:NO
exit /b %errorlevel%
"@
$batchPath=Join-Path $output 'build.cmd'
[IO.File]::WriteAllText($batchPath,$batch,[Text.Encoding]::ASCII)
& cmd.exe /d /c $batchPath
if($LASTEXITCODE -ne 0){throw 'Native guide observation build failed'}
Push-Location -LiteralPath $output
try {& (Join-Path $output 'GuideProbeTest.exe');if($LASTEXITCODE -ne 0){throw 'Native guide fixtures failed'}} finally {Pop-Location}
& python (Join-Path $taskRoot 'tools/analyze_guide_probe.py') (Join-Path $output 'guide-conformance.jsonl') --output (Join-Path $output 'guide-conformance-review.json')
if($LASTEXITCODE -ne 0){throw 'Guide parser conformance failed'}
& python -c 'import json,sys; r=json.load(open(sys.argv[1])); assert not r["review_problems"] and r["registry_observations"]==1 and r["allows_registration"] is False and r["allows_live_movement"] is False' (Join-Path $output 'guide-conformance-review.json')
if($LASTEXITCODE -ne 0){throw 'Guide emitted capture failed validation'}
Copy-Item -LiteralPath (Join-Path $dependency 'LICENSE.txt') -Destination (Join-Path $output 'MinHook-LICENSE.txt')
$sources=[ordered]@{}
foreach($name in @('native/GuideProbe/Guide.h','native/GuideProbe/Probe.cpp','native/GuideProbe/ProbeTest.cpp','native/StateBridge/AuditQueue.h','native/StateBridge/WindowsTarget.h','native/StateBridge/Retail156.h','tools/native_probe.py','tools/guide_probe.py','tools/build_guide_probe.ps1','tools/analyze_guide_probe.py')){
 $sources[$name]=(Get-FileHash -LiteralPath (Join-Path $taskRoot $name) -Algorithm SHA256).Hash.ToLowerInvariant()
}
$dllHash=(Get-FileHash -LiteralPath (Join-Path $output 'GuideProbe.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
[ordered]@{
 mode='observe-only';probe='guide-load';probe_revision=1;fixtures='passed';dll_sha256=$dllHash;
 target_sha256='bdf8f9e4a11257a72b64c84700e284c29e4c4ccaf5b8d4bfa7d0b2a7294479f7';
 record_limit=1024;minhook_sha256=$archivePin;sources=$sources;
 hooks=@(
  @{name='source';rva='0x170ff94';prologue='48895c240848897c2418554156415748'},
  @{name='body';rva='0x1331224';prologue='48895c240848897c242055488bec4883'},
  @{name='index';rva='0x7e6a50';prologue='405355565741544155415641574883ec'}
 );built=(Get-Date).ToUniversalTime().ToString('o')
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'probe-build.json') -Encoding UTF8
Write-Output "NATIVE GUIDE OBSERVATION BUILD PASS: $dllHash"
