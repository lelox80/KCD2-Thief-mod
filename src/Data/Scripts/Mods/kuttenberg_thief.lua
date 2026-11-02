-- Official mod entry point. A generated XGen observer is packaged separately.
-- Sticky process-lifetime isolation: a hot reload over a prior experiment is
-- never equivalent to a clean native-only startup. Only normal exit clears it.
if _KTMovementSession==nil then
    _KTMovementSession={protocol=2,nativeOnly=true,
        clean=KuttenbergThief==nil and _KTLegacySpeedTrialSeen==nil}
elseif type(_KTMovementSession)~="table" then
    _KTMovementSession={protocol=2,nativeOnly=true,clean=false}
end
local session=_KTMovementSession
if _KTLegacySpeedTrialSeen~=nil or (type(KuttenbergThief)=="table" and KuttenbergThief.speedTrial~=nil) then
    session.clean=false
end
Script.ReloadScript("Scripts/KuttenbergThief/Probe.lua")
KuttenbergThief.movementSession=session
Script.ReloadScript("Scripts/KuttenbergThief/NativeBridge.lua")
Script.ReloadScript("Scripts/KuttenbergThief/TerrainProbe.lua")
Script.ReloadScript("Scripts/KuttenbergThief/StealthHud.lua")
Script.ReloadScript("Scripts/KuttenbergThief/LedgeProbe.lua")
KuttenbergThief.Log("native_only_session clean="..tostring(session.clean).." legacy_packaged=false")
