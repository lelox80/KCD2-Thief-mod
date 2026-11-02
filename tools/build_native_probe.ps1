$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$dependency = Join-Path $taskRoot '.local/native/probe/deps/minhook-1.3.4'
$archive = Join-Path $taskRoot '.local/native/probe/deps/minhook-v1.3.4.zip'
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne '172708123daa0c98d20d3a980b16a50be14af243dc95dee6f79c24193ad010e4') { throw 'MinHook archive pin mismatch' }
# Re-extract the verified archive rather than trusting a modified local checkout.
Expand-Archive -LiteralPath $archive -DestinationPath (Split-Path -Parent $dependency) -Force
$output = Join-Path $taskRoot '.local/native/probe/build'
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
cl /nologo /std:c++17 /O2 /MT /W4 /EHsc /LD /I"$dependency\include" "$taskRoot\native\MovementProbe\Probe.cpp" buffer.obj hook.obj trampoline.obj hde64.obj /link /OUT:MovementProbe.dll /INCREMENTAL:NO
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /MT /W4 /EHsc /I"$dependency\include" "$taskRoot\native\MovementProbe\ProbeTest.cpp" buffer.obj hook.obj trampoline.obj hde64.obj /link /OUT:ProbeTest.exe /INCREMENTAL:NO
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /MT /W4 /EHsc "$taskRoot\native\SwoopCore\TrajectoryTest.cpp" /link /OUT:TrajectoryTest.exe /INCREMENTAL:NO
exit /b %errorlevel%
"@
$batchPath = Join-Path $output 'build.cmd'
[IO.File]::WriteAllText($batchPath, $batch, [Text.Encoding]::ASCII)
& cmd.exe /d /c $batchPath
if ($LASTEXITCODE -ne 0) { throw 'Native probe build failed' }
Copy-Item -LiteralPath (Join-Path $dependency 'LICENSE.txt') -Destination (Join-Path $output 'MinHook-LICENSE.txt')
Push-Location -LiteralPath $output
try {
    & (Join-Path $output 'ProbeTest.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Native fixture failed' }
    & (Join-Path $output 'TrajectoryTest.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Trajectory fixture failed' }
} finally {
    Pop-Location
}
$probeHash = (Get-FileHash -LiteralPath (Join-Path $output 'MovementProbe.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
@{mode='observe-only'; dll_sha256=$probeHash; minhook_sha256='172708123daa0c98d20d3a980b16a50be14af243dc95dee6f79c24193ad010e4'; fixtures='passed'} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'probe-build.json') -Encoding UTF8
Write-Output "NATIVE PROBE BUILD PASS: $probeHash"
