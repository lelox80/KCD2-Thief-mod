param([ValidatePattern('^[a-z0-9][a-z0-9-]{0,63}$')][string]$OutputName='build-playable')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$dependency=Join-Path $taskRoot '.local/native/probe/deps/minhook-1.3.4'
$archive=Join-Path $taskRoot '.local/native/probe/deps/minhook-v1.3.4.zip'
if((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne '172708123daa0c98d20d3a980b16a50be14af243dc95dee6f79c24193ad010e4') {throw 'MinHook archive pin mismatch'}
Expand-Archive -LiteralPath $archive -DestinationPath (Split-Path -Parent $dependency) -Force
$output=Join-Path $taskRoot ('.local/native/swoop/'+$OutputName)
New-Item -ItemType Directory -Path $output -Force | Out-Null
$vcvars='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
if(!(Test-Path -LiteralPath $vcvars)) {throw 'Expected inspected MSVC toolchain missing'}
$batch=@"
@echo off
call "$vcvars" >nul
if errorlevel 1 exit /b 1
cd /d "$output"
cl /nologo /c /O2 /MT /W3 /I"$dependency\include" "$dependency\src\buffer.c" "$dependency\src\hook.c" "$dependency\src\trampoline.c" "$dependency\src\hde\hde64.c"
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /MT /W4 /WX /EHsc "$taskRoot\native\StateBridge\PacketTest.cpp" /Fe:PacketTest.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /MT /W4 /WX /EHsc "$taskRoot\native\StateBridge\AdapterTest.cpp" /Fe:AdapterTest.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /MT /W4 /WX /EHsc "$taskRoot\native\StateBridge\TerrainAdapterTest.cpp" /Fe:TerrainAdapterTest.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /MT /W4 /WX /EHsc "$taskRoot\native\StateBridge\AuditQueueTest.cpp" /Fe:AuditQueueTest.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /MT /W4 /WX /EHsc "$taskRoot\native\SwoopCore\BoundedSwoopTest.cpp" /Fe:BoundedSwoopTest.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /MT /W4 /WX /EHsc /I"$dependency\include" "$taskRoot\native\StateBridge\BridgeTest.cpp" buffer.obj hook.obj trampoline.obj hde64.obj /Fe:BridgeTest.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /MT /W4 /WX /EHsc /DKT_ALLOW_MOTION=1 /I"$dependency\include" "$taskRoot\native\StateBridge\BridgeTest.cpp" buffer.obj hook.obj trampoline.obj hde64.obj /Fe:BridgeMotionTest.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /MT /W4 /WX /EHsc /LD /DKT_ALLOW_MOTION=0 /I"$dependency\include" "$taskRoot\native\StateBridge\Bridge.cpp" buffer.obj hook.obj trampoline.obj hde64.obj /link /OUT:KuttenbergBridge.dll /INCREMENTAL:NO
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /MT /W4 /WX /EHsc /LD /DKT_ALLOW_MOTION=1 /I"$dependency\include" "$taskRoot\native\StateBridge\Bridge.cpp" buffer.obj hook.obj trampoline.obj hde64.obj /link /OUT:KuttenbergSwoop.dll /INCREMENTAL:NO
exit /b %errorlevel%
"@
$batchPath=Join-Path $output 'build.cmd'
[IO.File]::WriteAllText($batchPath,$batch,[Text.Encoding]::ASCII)
& cmd.exe /d /c $batchPath
if($LASTEXITCODE -ne 0) {throw 'Native bridge build failed'}
Push-Location -LiteralPath $output
try {
    foreach($test in @('PacketTest.exe','AdapterTest.exe','TerrainAdapterTest.exe','AuditQueueTest.exe','BoundedSwoopTest.exe','BridgeTest.exe','BridgeMotionTest.exe')) {
        & (Join-Path $output $test)
        if($LASTEXITCODE -ne 0) {throw "Native fixture failed: $test"}
    }
} finally {Pop-Location}
$floatLua=Join-Path $taskRoot '.local/tools/lua-5.1.5-float32/lua-float32.exe'
$doubleLua=Join-Path $taskRoot '.local/tools/lua.exe'
if(!(Test-Path -LiteralPath $floatLua)) {throw 'Retail float32 Lua fixture runtime is required'}
& $floatLua '-e' 'assert(16777216+1==16777216)'
if($LASTEXITCODE -ne 0) {throw 'Float32 fixture number model mismatch'}
Push-Location -LiteralPath $taskRoot
try {
    foreach($runtime in @($doubleLua,$floatLua)) {
        $wire=Join-Path $output ((Split-Path -Leaf $runtime)+'.wire.txt')
        & $runtime 'tests/native_bridge_test.lua' $wire
        if($LASTEXITCODE -ne 0) {throw 'Lua state bridge fixture failed'}
        & (Join-Path $output 'PacketTest.exe') $wire
        if($LASTEXITCODE -ne 0) {throw 'Actual Lua/native wire conformance failed'}
        & $runtime 'tests/native_session_test.lua'
        if($LASTEXITCODE -ne 0) {throw 'Lua native-only session fixture failed'}
        & $runtime 'tests/terrain_probe_test.lua'
        if($LASTEXITCODE -ne 0) {throw 'Passive terrain telemetry fixture failed'}
        & $runtime 'tests/stealth_hud_test.lua'
        if($LASTEXITCODE -ne 0) {throw 'Read-only stealth HUD fixture failed'}
        & $runtime 'tests/ledge_probe_test.lua'
        if($LASTEXITCODE -ne 0) {throw 'Read-only ledge geometry fixture failed'}
    }
} finally {Pop-Location}
Copy-Item -LiteralPath (Join-Path $dependency 'LICENSE.txt') -Destination (Join-Path $output 'MinHook-LICENSE.txt')
$sources=@{}
Get-ChildItem -LiteralPath (Join-Path $taskRoot 'native/StateBridge'),(Join-Path $taskRoot 'native/SwoopCore') -File | Sort-Object FullName | ForEach-Object {
    $name=$_.FullName.Substring($taskRoot.Length+1).Replace('\','/')
    $sources[$name]=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
}
$luaSource=Join-Path $taskRoot 'src/Data/Scripts/KuttenbergThief/NativeBridge.lua'
$sources['src/Data/Scripts/KuttenbergThief/NativeBridge.lua']=(Get-FileHash -LiteralPath $luaSource -Algorithm SHA256).Hash.ToLowerInvariant()
$entrySource=Join-Path $taskRoot 'src/Data/Scripts/Mods/kuttenberg_thief.lua'
$sources['src/Data/Scripts/Mods/kuttenberg_thief.lua']=(Get-FileHash -LiteralPath $entrySource -Algorithm SHA256).Hash.ToLowerInvariant()
$terrainSource=Join-Path $taskRoot 'src/Data/Scripts/KuttenbergThief/TerrainProbe.lua'
$sources['src/Data/Scripts/KuttenbergThief/TerrainProbe.lua']=(Get-FileHash -LiteralPath $terrainSource -Algorithm SHA256).Hash.ToLowerInvariant()
$stealthSource=Join-Path $taskRoot 'src/Data/Scripts/KuttenbergThief/StealthHud.lua'
$sources['src/Data/Scripts/KuttenbergThief/StealthHud.lua']=(Get-FileHash -LiteralPath $stealthSource -Algorithm SHA256).Hash.ToLowerInvariant()
$ledgeSource=Join-Path $taskRoot 'src/Data/Scripts/KuttenbergThief/LedgeProbe.lua'
$sources['src/Data/Scripts/KuttenbergThief/LedgeProbe.lua']=(Get-FileHash -LiteralPath $ledgeSource -Algorithm SHA256).Hash.ToLowerInvariant()
foreach($uiPath in @('src/UI/KTStealth.as','src/Data/Libs/UI/UIElements/KTStealth.xml','tools/stealth_movie.py','.local/assets/stealth/KTStealth.swf')) {
    $sources[$uiPath]=(Get-FileHash -LiteralPath (Join-Path $taskRoot $uiPath) -Algorithm SHA256).Hash.ToLowerInvariant()
}
$payloads=@{}
foreach($name in @('KuttenbergBridge.dll','KuttenbergSwoop.dll')) {
    $payloads[$name]=(Get-FileHash -LiteralPath (Join-Path $output $name) -Algorithm SHA256).Hash.ToLowerInvariant()
}
@{protocol=2;audit_revision=2;mode='bounded-prototype';fixtures='passed';target_sha256='bdf8f9e4a11257a72b64c84700e284c29e4c4ccaf5b8d4bfa7d0b2a7294479f7';sources=$sources;payloads=$payloads} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'native-build.json') -Encoding UTF8
Write-Output 'NATIVE STATE BRIDGE + BOUNDED PROTOTYPE BUILD PASS. First retail profile is observation-only.'
