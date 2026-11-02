[CmdletBinding()]
param(
    [string]$VcVars64 = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
)
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$manifestPath = Join-Path $PSScriptRoot 'lua51_source_hashes.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$source = Join-Path $taskRoot '.local/references/lua-5.1.5/src'
$output = Join-Path $taskRoot '.local/tools/lua-5.1.5-float32'
$derived = Join-Path $output 'src'
$interpreter = Join-Path $output 'lua-float32.exe'
$existingLua = Join-Path $taskRoot '.local/tools/lua.exe'

function Hash-Bytes([byte[]]$Bytes) {
    $algorithm = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($algorithm.ComputeHash($Bytes)).Replace('-', '').ToLowerInvariant() }
    finally { $algorithm.Dispose() }
}
function Hash-File([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}
function Write-Utf8([string]$Path, [string]$Text) {
    [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false))
}
# No deletion/moving occurs. Refuse write paths redirected outside this checkout.
foreach ($path in @((Join-Path $taskRoot '.local'), (Join-Path $taskRoot '.local/tools'), $output, $derived)) {
    $absolute = [IO.Path]::GetFullPath($path)
    if (!$absolute.StartsWith($taskRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Fixture destination escaped the checkout: $absolute"
    }
    if ((Test-Path -LiteralPath $absolute) -and
        ((Get-Item -LiteralPath $absolute -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Refusing redirected fixture destination: $absolute"
    }
}
if (!(Test-Path -LiteralPath $source -PathType Container)) {
    throw "Pinned Lua source is absent. Extract official Lua 5.1.5 into .local/references/lua-5.1.5 first; this script never downloads it."
}
if (!(Test-Path -LiteralPath $VcVars64 -PathType Leaf)) { throw 'The selected existing MSVC x64 toolchain is absent.' }
if ($taskRoot -match '[%\r\n]' -or $VcVars64 -match '[%\r\n]') { throw 'Batch build paths cannot contain percent signs or newlines.' }
$originalLuaHash = if (Test-Path -LiteralPath $existingLua) { Hash-File $existingLua } else { $null }
$inputs = [ordered]@{}
$pins = [ordered]@{}
foreach ($entry in ($manifest.files_sha256.PSObject.Properties | Sort-Object Name)) {
    if ($entry.Name -notmatch '^[a-z0-9_]+\.[ch]$' -or $entry.Value -notmatch '^[a-f0-9]{64}$') { throw 'Invalid source-hash manifest entry.' }
    $path = Join-Path $source $entry.Name
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing pinned Lua input: $($entry.Name)" }
    $bytes = [IO.File]::ReadAllBytes($path)
    if ((Hash-Bytes $bytes) -ne $entry.Value) { throw "Lua source hash mismatch: $($entry.Name)" }
    $inputs[$entry.Name] = $bytes
    $pins[$entry.Name] = $entry.Value
}
if ($inputs.Count -ne 55 -or !$inputs.Contains('luaconf.h') -or !$inputs.Contains('lua.c')) { throw 'Incomplete Lua source manifest.' }
$text = [Text.Encoding]::ASCII.GetString($inputs['luaconf.h']).Replace("`r`n", "`n")
foreach ($expected in @('#define LUA_NUMBER_DOUBLE', "#define LUA_NUMBER`tdouble", "#define LUA_NUMBER_SCAN`t`t`"%lf`"")) {
    if (!$text.Contains($expected)) { throw "Unexpected pinned Lua configuration: $expected" }
}
$text = $text.Replace('#define LUA_NUMBER_DOUBLE', '/* Fixture variant: LUA_NUMBER_DOUBLE deliberately not defined. */')
$text = $text.Replace("#define LUA_NUMBER`tdouble", "#define LUA_NUMBER`tfloat")
$text = $text.Replace("#define LUA_NUMBER_SCAN`t`t`"%lf`"", "#define LUA_NUMBER_SCAN`t`t`"%f`"")
# Keep LUAI_UACNUMBER=double: C varargs promote float to double for printf.
# Normalize only the derived luaconf to the existing inspected Windows recipe.
$patchedConfig = [Text.Encoding]::ASCII.GetBytes($text.Replace("`n", "`r`n"))
$configHash = Hash-Bytes $patchedConfig
if (Test-Path -LiteralPath $derived) {
    foreach ($file in (Get-ChildItem -LiteralPath $derived -File -Force)) {
        if ($file.Extension -notin @('.c', '.h')) { continue }
        if (!$inputs.Contains($file.Name)) { throw "Unknown derived Lua source; refusing overwrite: $($file.Name)" }
        if ($file.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Refusing redirected derived file: $($file.Name)" }
        $hash = Hash-File $file.FullName
        if ($hash -ne $pins[$file.Name] -and !($file.Name -eq 'luaconf.h' -and $hash -eq $configHash)) {
            throw "Changed derived Lua source; refusing overwrite: $($file.Name)"
        }
    }
}
New-Item -ItemType Directory -Path $derived -Force | Out-Null
foreach ($entry in $inputs.GetEnumerator()) {
    [IO.File]::WriteAllBytes((Join-Path $derived $entry.Key), $entry.Value)
}
[IO.File]::WriteAllBytes((Join-Path $derived 'luaconf.h'), $patchedConfig)
$translationUnits = @($inputs.Keys | Where-Object { $_.EndsWith('.c') -and $_ -notin @('luac.c', 'print.c') })
$sourceArguments = ($translationUnits | ForEach-Object { '"src\' + $_ + '"' }) -join ' '
$batch = @"
@echo off
call "$VcVars64" >nul
if errorlevel 1 exit /b 1
cl /nologo /TC /O2 /fp:precise /MT /W3 /D_CRT_SECURE_NO_WARNINGS $sourceArguments /Fe:lua-float32.exe /link /INCREMENTAL:NO
exit /b %errorlevel%
"@
[IO.File]::WriteAllText((Join-Path $output 'build.cmd'), $batch, [Text.Encoding]::ASCII)
Push-Location -LiteralPath $output
try {
    & cmd.exe /d /v:off /c 'build.cmd'
    if ($LASTEXITCODE -ne 0) { throw 'Float32 Lua compilation failed.' }
} finally { Pop-Location }

$smoke = @'
assert(_VERSION=='Lua 5.1')
assert(string.byte(string.dump(function()end),11)==4,'four-byte Lua number expected')
assert(16777216+1==16777216,'float32 arithmetic cutoff')
local known=0
for i=0,24 do known=known+2^i end
assert(string.format('%.0f',known)=='33554432','retail numeric mask failure reproduced')
local f=assert(io.open('.local/tools/lua-5.1.5-float32/scan-value.txt','r'))
assert(f:read('*n')==1.25,'float scanf configuration');f:close()
assert(string.format('%.2f',1.25)=='1.25','printf default argument promotion')
print('FLOAT32_SMOKE_PASS number_bytes=4 mask25='..string.format('%.0f',known))
'@
Write-Utf8 (Join-Path $output 'smoke.lua') $smoke
Write-Utf8 (Join-Path $output 'scan-value.txt') "1.25`n"
$tests = [ordered]@{
    smoke = @((Join-Path $output 'smoke.lua'))
    native_bridge = @('tests/native_bridge_test.lua', (Join-Path $output 'v2-wire-packets.txt'))
    native_session = @('tests/native_session_test.lua')
}
$results = [ordered]@{}
Push-Location -LiteralPath $taskRoot
try {
    foreach ($test in $tests.GetEnumerator()) {
        $arguments = $test.Value
        $lines = @(& $interpreter @arguments 2>&1)
        $code = $LASTEXITCODE
        $captured = ($lines | ForEach-Object { $_.ToString() }) -join "`n"
        Write-Utf8 (Join-Path $output ($test.Key + '-result.txt')) ($captured + "`n")
        $results[$test.Key] = @{ exit_code = $code; output = $captured }
        Write-Output $captured
        if ($code -ne 0) { throw "Float32 Lua fixture failed: $($test.Key)" }
    }
} finally { Pop-Location }
foreach ($entry in $pins.GetEnumerator()) {
    if ((Hash-File (Join-Path $source $entry.Key)) -ne $entry.Value) { throw 'Original Lua source changed during the fixture build.' }
}
if ($originalLuaHash -and (Hash-File $existingLua) -ne $originalLuaHash) { throw 'Existing lua.exe changed during the fixture build.' }
$testHashes = [ordered]@{}
foreach ($path in @('tests/native_bridge_test.lua', 'tests/native_session_test.lua', 'src/Data/Scripts/KuttenbergThief/NativeBridge.lua', 'src/Data/Scripts/Mods/kuttenberg_thief.lua')) {
    $testHashes[$path] = Hash-File (Join-Path $taskRoot $path)
}
$receipt = [ordered]@{
    protocol = 2; number_bytes = 4; fixtures = 'passed'; created_utc = [DateTime]::UtcNow.ToString('o')
    interpreter_sha256 = (Hash-File $interpreter); existing_lua_sha256 = $originalLuaHash
    original_source_unchanged = $true; input_manifest_sha256 = (Hash-File $manifestPath)
    patched_luaconf_sha256 = $configHash; source_directory = $source; toolchain = $VcVars64
    compiler_flags = '/TC /O2 /fp:precise /MT /W3 /D_CRT_SECURE_NO_WARNINGS'
    source_hashes = $testHashes; tests = $results
    limitation = 'Owned standalone numeric-model fixtures; no retail ABI, hook, collision or gameplay validation.'
}
Write-Utf8 (Join-Path $output 'reproducible-build.json') (($receipt | ConvertTo-Json -Depth 6) + "`n")
Write-Output "FLOAT32 LUA BUILD AND FIXTURES PASS: $interpreter"
