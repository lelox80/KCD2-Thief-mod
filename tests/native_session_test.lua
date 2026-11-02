-- Session provenance must survive reload and may only become less permissive.
local source='src/Data/Scripts/Mods/kuttenberg_thief.lua'
local function reset()
    KuttenbergThief=nil;_KTMovementSession=nil;_KTLegacySpeedTrialSeen=nil
end
local loaded={}
Script={ReloadScript=function(path)
    loaded[#loaded+1]=path
    assert(not string.find(path,'SpeedTrial',1,true),'legacy route loaded')
    if path=='Scripts/KuttenbergThief/Probe.lua' then KuttenbergThief={Log=function()end} end
end}
reset();dofile(source)
assert(_KTMovementSession.clean==true and _KTMovementSession.nativeOnly==true)
assert(KuttenbergThief.movementSession==_KTMovementSession and #loaded==5)
assert(loaded[3]=='Scripts/KuttenbergThief/TerrainProbe.lua')
assert(loaded[4]=='Scripts/KuttenbergThief/StealthHud.lua')
assert(loaded[5]=='Scripts/KuttenbergThief/LedgeProbe.lua')
local original=_KTMovementSession;dofile(source)
assert(_KTMovementSession==original and original.clean==true,'clean reload must preserve provenance')
KuttenbergThief.speedTrial={};dofile(source)
assert(original.clean==false,'legacy table must poison existing session')
dofile(source);assert(original.clean==false,'reload must not unpoison')
reset();KuttenbergThief={};dofile(source)
assert(_KTMovementSession.clean==false,'upgrade over an existing module needs process restart')
reset();_KTLegacySpeedTrialSeen=true;dofile(source);assert(_KTMovementSession.clean==false)
reset();_KTMovementSession=false;dofile(source);assert(_KTMovementSession.clean==false)
reset();dofile(source);_KTLegacySpeedTrialSeen=true;dofile(source);assert(_KTMovementSession.clean==false)
print('PASS: native-only clean startup, reload provenance, legacy poisoning and fresh-process requirement')
