param([ValidatePattern('^[a-z0-9][a-z0-9-]{0,63}$')][string]$OutputName='build-v021',[switch]$DryGuideGuard,[switch]$SelectionPreview,[switch]$LiveTrial,[switch]$TallPreview,[switch]$Repeatable,[switch]$LongWall,[switch]$RuntimeWall,[switch]$AirVault,[switch]$VaultTrace)
$ErrorActionPreference = 'Stop'
if ($DryGuideGuard -and !$OutputName.StartsWith('dry-guide-')) { throw 'Use a separate dry-guide- output name' }
$definition = if ($DryGuideGuard) { '/DKT_LEDGE_DRY_GUARD' } else { '' }
if ($SelectionPreview) {
    if (!$DryGuideGuard -or !$OutputName.StartsWith('dry-guide-selection-')) { throw 'Use the dry guard and a dry-guide-selection- output name' }
    $definition += ' /DKT_LEDGE_DRY_SELECTION'
}
$mode = if ($DryGuideGuard) { 'dry-guide-guard' } else { 'observe-only' }
$guideProfile = 'guide-append'
if ($TallPreview) {
    if (!$DryGuideGuard -or !$SelectionPreview) { throw 'Tall preview requires guarded selection' }
    $definition += ' /DKT_LEDGE_TALL_PREVIEW'
    $mode = 'dry-tall-preview'
    $guideProfile = 'guide-tall'
}
if ($LiveTrial) {
    if (!$DryGuideGuard -or !$SelectionPreview) { throw 'Live trial requires both guide guards' }
    $definition += ' /DKT_LEDGE_LIVE_TRIAL'
    $mode = if ($TallPreview) { 'bounded-tall-vault' } else { 'bounded-vault-onto' }
}
if ($Repeatable) {
    if (!$LiveTrial -or !$TallPreview) { throw 'Repeatable traversal requires the guarded tall live path' }
    $definition += ' /DKT_LEDGE_REPEATABLE'
    $mode = 'repeatable-tall-vault'
    $guideProfile = 'guide-walls'
}
if ($LongWall) {
    if (!$Repeatable) { throw 'The longer wall requires repeatable traversal guards' }
    $definition += ' /DKT_LEDGE_LONG_WALL'
    $mode = 'long-wall-session-guide'
}
if ($RuntimeWall) {
    if (!$LongWall) { throw 'The runtime wall trial requires the guarded long-wall profile' }
    $definition += ' /DKT_LEDGE_RUNTIME_WALL'
    $mode = 'runtime-wall-pool'
    $guideProfile = 'guide-runtime'
}
if ($AirVault) {
    if (!$RuntimeWall) { throw 'The airborne vault trial requires the runtime guide pool' }
    $definition += ' /DKT_LEDGE_AIR_VAULT'
    $mode = 'jump-into-stock-vault'
}
if ($VaultTrace) {
    if (!$AirVault) { throw 'The crest trace requires the guarded airborne vault build' }
    $definition += ' /DKT_LEDGE_VAULT_TRACE'
    $mode = 'crest-profile-crossing'
}
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
cl /nologo /std:c++17 /O2 /MT /W4 /WX /EHsc /LD $definition /I"$dependency\include" "$taskRoot\native\LedgeProbe\Admission.cpp" buffer.obj hook.obj trampoline.obj hde64.obj /link /OUT:LedgeProbe.dll /INCREMENTAL:NO
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /MT /W4 /WX /EHsc $definition /I"$dependency\include" "$taskRoot\native\LedgeProbe\AdmissionTest.cpp" buffer.obj hook.obj trampoline.obj hde64.obj /link /OUT:LedgeProbeTest.exe /INCREMENTAL:NO
exit /b %errorlevel%
"@
$batchPath = Join-Path $output 'build.cmd'
[IO.File]::WriteAllText($batchPath, $batch, [Text.Encoding]::ASCII)
& cmd.exe /d /c $batchPath
if ($LASTEXITCODE -ne 0) { throw 'Native ledge probe build failed' }
Copy-Item -LiteralPath (Join-Path $dependency 'LICENSE.txt') -Destination (Join-Path $output 'MinHook-LICENSE.txt')
Push-Location -LiteralPath $output
try {
    if ($DryGuideGuard) {
        @'
import sys
from pathlib import Path

sys.path.insert(0, sys.argv[1])
from guide_member import append_inactive, configured_archive, read_member, _parse_records

payload, receipt = append_inactive(read_member(configured_archive())[0], sys.argv[2])
ledges, markers = _parse_records(payload, (receipt["ledge_count"], receipt["marker_count"]))
with Path("dry-member-fixture.bin").open("wb") as fixture:
    fixture.write(ledges)
    for offset in range(0, len(markers), 25):
        fixture.write(markers[offset:offset + 25] + bytes(3))
'@ | & python - $PSScriptRoot $guideProfile
        if ($LASTEXITCODE -ne 0) { throw 'Dry guide member fixture staging failed' }
    }
    & (Join-Path $output 'LedgeProbeTest.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Native ledge probe fixture failed' }
} finally { Pop-Location }
if (!$DryGuideGuard) {
    & python (Join-Path $taskRoot 'tools/analyze_ledge_admission.py') (Join-Path $output 'admission-conformance.jsonl') --output (Join-Path $output 'admission-conformance-review.json')
    if ($LASTEXITCODE -ne 0) { throw 'Native admission parser conformance failed' }
    @'
import json
import sys
from pathlib import Path

report = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
assert not report["review_problems"]
assert report["allows_live_movement"] is False
assert report["framework_acceptance_records"] == 1
'@ | & python - (Join-Path $output 'admission-conformance-review.json')
    if ($LASTEXITCODE -ne 0) { throw 'Native admission emitted capture did not validate' }
}
$sources = [ordered]@{}
$sourceFiles = @(
    'native/LedgeProbe/Admission.cpp','native/LedgeProbe/AdmissionTest.cpp','native/LedgeProbe/Selection.h',
    'native/StateBridge/AuditQueue.h','native/StateBridge/WindowsTarget.h','native/StateBridge/Retail156.h',
    'tools/build_ledge_probe.ps1','tools/ledge_probe.py','tools/native_probe.py','tools/analyze_ledge_admission.py')
if ($DryGuideGuard) {
    $sourceFiles += @('native/LedgeProbe/DryGuide.h','native/GuideProbe/Guide.h','tools/guide_member.py',
        'tools/dry_guide_probe.py','tools/project.py','tools/observer_patch.py')
}
if ($LiveTrial) {
    $sourceFiles += @('native/LedgeProbe/VaultTrial.h','native/ClearanceProbe/CapsuleControl.h',
        'native/ClearanceProbe/Capsule.h','tools/vault_onto_trial.py')
}
if ($TallPreview) { $sourceFiles += @('native/LedgeProbe/TallPreview.h','tools/tall_guide_probe.py') }
if ($TallPreview -and $LiveTrial) { $sourceFiles += 'tools/tall_vault_trial.py' }
if ($Repeatable) { $sourceFiles += @('native/LedgeProbe/RepeatableVault.h','tools/tall_wall.py') }
if ($LongWall) { $sourceFiles += 'native/LedgeProbe/LongWall.h' }
if ($RuntimeWall) { $sourceFiles += 'native/LedgeProbe/RuntimeWall.h' }
if ($AirVault) { $sourceFiles += 'native/LedgeProbe/AirVault.h' }
if ($VaultTrace) { $sourceFiles += 'native/LedgeProbe/VaultTrace.h' }
foreach ($name in $sourceFiles) {
    $sources[$name] = (Get-FileHash -LiteralPath (Join-Path $taskRoot $name) -Algorithm SHA256).Hash.ToLowerInvariant()
}
$probeHash = (Get-FileHash -LiteralPath (Join-Path $output 'LedgeProbe.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
$hooks = @(
    @{name='attempt';rva='0x831b20';prologue='488bc448895808488970184889782055'},
    @{name='selection';rva='0x831eac';prologue='488bc448895810488970184889782055'},
    @{name='query';rva='0x832188';prologue='40534883ec40488bc1488bda4883c118'},
    @{name='framework';rva='0x85612c';prologue='48895c24104889742418574883ec2048'}
)
if ($DryGuideGuard) { $hooks += @{name='index';rva='0x7e6a50';prologue='405355565741544155415641574883ec'} }
if ($TallPreview) { $hooks += @{name='params';rva='0x832310';prologue='8954241053574883ec38488bd98bfa48'} }
if ($RuntimeWall) { $hooks += @{name='body';rva='0x1331224';prologue='48895c240848897c242055488bec4883'} }
if ($AirVault) { $hooks += @{name='movement_request';rva='0x837e7c';prologue='48895c240848896c2410488974241857'} }
if ($VaultTrace) {
    $hooks += @{name='grab_create';rva='0xf45298';prologue='4c8bdc49895b084989731049897b184d'}
    $hooks += @{name='grab_exit';rva='0x702b7c';prologue='48895c2410574881eca0000000488b05'}
    $hooks += @{name='living_action';rva='0x845b90';prologue='488bc448895810488970185557415441'}
}
[ordered]@{
    mode=$mode; probe='ledge-admission'; probe_revision=4; fixtures='passed'; dll_sha256=$probeHash;
    target_sha256='bdf8f9e4a11257a72b64c84700e284c29e4c4ccaf5b8d4bfa7d0b2a7294479f7';
    selection_rva='0x831eac'; return_rva='0x831ccd'; prologue='488bc448895810488970184889782055';
    minhook_sha256=$archivePin; record_limit=60000; sources=$sources;
    hooks=$hooks;
    built=(Get-Date).ToUniversalTime().ToString('o')
} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'probe-build.json') -Encoding UTF8
Write-Output "NATIVE LEDGE PROBE BUILD PASS: $probeHash"
