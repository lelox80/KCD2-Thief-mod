-- Opt-in M1 experiment, not swoop. KCD2 retains input, physics and animation.
-- One owned RPG buff; no position/velocity/input/CVar/camera writes.
-- Historical experiment, excluded from the native-only runtime package. An
-- explicit source load permanently poisons native permission for this process.
_KTLegacySpeedTrialSeen=true
if type(_KTMovementSession)=="table" then _KTMovementSession.clean=false end
local M = KuttenbergThief
local A = M.access
local get, call, fmt = A.member, A.invoke, A.scalar
local T = {buffId="eacd8708-9dde-43a6-bddb-76d56b9338d1", intervalMs=50,
    armSeconds=3, baselineSeconds=2, boostSeconds=1, recoverySeconds=2,
    maxGap=0.35, maxObserverAge=0.3, maxBoostTravel=3, cooldownSeconds=2}
M.speedTrial = T

local function now() return call(System, "GetCurrAsyncTime", false) end
local function finite(n) return type(n)=="number" and n==n and math.abs(n)<1e12 end
local function vector(v)
    return type(v)=="table" and finite(v.x) and finite(v.y) and finite(v.z)
end
local function distance(a,b)
    return math.sqrt((a.x-b.x)^2+(a.y-b.y)^2+(a.z-b.z)^2)
end
local function log(s) M.Log("speed_trial "..s) end
local function samePlayer()
    local p=A.currentPlayer()
    -- Table identity prevents releasing a saved handle on a new player with reused id.
    if p and p==T.owner and p.id==T.ownerId then return p end
end
local function present(p) return call(get(p,"soul"),"HasBuffDebug",true,T.buffId) end
local function rms(p) return call(get(p,"soul"),"GetDerivedStat",true,"rms") end

function T.Remove(reason)
    local p=samePlayer()
    if not T.ownsBuff then return true end
    if not p then
        -- Never dereference an old entity. Nonpersistent, finite engine lifetime is
        -- a secondary safeguard, whose actual expiration must be measured in-game.
        log("cleanup=owner_lost reason="..reason.." runtime_expiry_unverified=true")
        T.ownsBuff=false; T.locked=true; return false
    end
    call(p.soul,"RemoveAllBuffsByGuid",true,T.buffId) -- only our unique definition
    local value=present(p)
    local ok=value==false
    log("cleanup="..(ok and "verified_absent" or "unconfirmed").." reason="..reason
        .." rms="..fmt(rms(p)))
    T.ownsBuff=not ok
    if not ok then T.locked=true end
    return ok
end

function T.Cancel(reason)
    if T.timer then call(Script,"KillTimer",false,T.timer); T.timer=nil end
    if T.state then
        local p=samePlayer()
        log("end reason="..reason.." state="..T.state
            .." pos="..fmt(p and call(p,"GetWorldPos",true)))
    end
    T.Remove(reason)
    T.state=nil; T.lastEnd=now()
    if not T.ownsBuff then T.owner=nil; T.ownerId=nil end
end

function T.Gate(p, stamp)
    if not p or p~=T.owner or p.id~=T.ownerId or M.generation~=T.generation then return "player_generation" end
    local s=M.actorStates
    if not finite(stamp) or not finite(M.actorStatesTime) or stamp<M.actorStatesTime
        or stamp-M.actorStatesTime>T.maxObserverAge then return "observer_stale" end
    if type(s)~="table" or s.crouch~=true then return "not_crouched" end
    for _,key in ipairs({"inAir","cutscene","interactiveScene","dialogue","horse","minigame","actorAnim","onBed"}) do
        if s[key]~=false then return "actor_state_"..key end
    end
    local a,h,pi,so=p.actor,p.human,p.player,p.soul
    local checks={
        {Game,"IsLoadingEngineSaveGame",false,false}, {a,"IsFlying",true,false},
        {a,"IsDead",true,false}, {a,"IsUnconscious",true,false},
        {a,"IsCarryingCorpse",true,false}, {h,"IsMounted",true,false},
        {h,"IsInDialog",true,false}, {h,"IsPickpocketing",true,false},
        {h,"IsWeaponDrawn",true,false}, {so,"IsInCombatDanger",true,false},
        {pi,"IsSitting",true,false}, {pi,"IsLaying",true,false},
        {pi,"GetFlyMode",true,0}, {h,"IsOnLadder",true,0},
        {a,"GetPhysicalizationProfile",true,"alive"}}
    for _,c in ipairs(checks) do
        local value,status=call(c[1],c[2],c[3])
        if status~="ok" or value~=c[4] then return "gate_"..c[2].."_"..fmt(value) end
    end
    local anim=call(a,"GetCurrentAnimationState",true)
    if anim~="MotionIdle" and anim~="MotionMovement" then return "animation_"..fmt(anim) end
    local pos=call(p,"GetWorldPos",true)
    local camera=call(System,"GetViewCameraPos",false)
    if not vector(pos) or not vector(camera) then return "camera_unknown" end
    if math.abs(camera.x-pos.x)>0.6 or math.abs(camera.y-pos.y)>0.6
        or camera.z-pos.z<0.6 or camera.z-pos.z>2.2 then return "camera_detached" end
    if T.state=="boost" and distance(pos,T.boostStart)>T.maxBoostTravel then return "travel_limit" end
    return nil
end

function T.Begin()
    if T.state then log("refused=already_active"); return end
    if T.locked then log("refused=cleanup_unconfirmed_restart_required"); return end
    local stamp=now()
    if not finite(stamp) then log("refused=clock_unknown"); return end
    if T.lastEnd and stamp-T.lastEnd<T.cooldownSeconds then log("refused=cooldown"); return end
    local p=A.currentPlayer()
    if not p then log("refused=no_player");return end
    for _,name in ipairs({"AddBuff","RemoveAllBuffsByGuid","HasBuffDebug","GetDerivedStat"}) do
        if type(get(p.soul,name))~="function" then log("refused=missing_"..name);return end
    end
    if present(p)~=false then log("refused=buff_presence_not_false");return end
    local value=rms(p)
    if not finite(value) or value<=0 then log("refused=rms_unknown");return end
    T.owner=p; T.ownerId=p.id; T.generation=M.generation
    local reason=T.Gate(p,stamp)
    if reason then log("refused="..reason);T.owner=nil;T.ownerId=nil;return end
    T.state="arm"; T.started=stamp; T.phaseStart=stamp;T.lastTime=stamp;T.nextLog=stamp
    T.baselineRms=value
    log("begin phase=arm delay="..T.armSeconds.." rms="..fmt(value).." movement_owner=kcd2")
    T.Schedule()
end

function T.Sample(p,stamp)
    log("sample phase="..T.state.." elapsed="..fmt(stamp-T.started)
        .." pos="..fmt(call(p,"GetWorldPos",true)).." velocity="..fmt(call(p,"GetVelocity",true))
        .." rms="..fmt(rms(p)).." buff="..fmt(present(p))
        .." grounded=true crouch=true observer_age="..fmt(stamp-M.actorStatesTime))
end

function T.Tick()
    T.timer=nil
    if not T.state then return end
    local stamp=now()
    if not finite(stamp) or stamp<T.lastTime or stamp-T.lastTime>T.maxGap then T.Cancel("pause_or_time_gap");return end
    T.lastTime=stamp
    local p=samePlayer()
    local reason=T.Gate(p,stamp)
    if reason then T.Cancel(reason);return end
    local elapsed=stamp-T.phaseStart
    if T.state=="arm" and elapsed>=T.armSeconds then
        T.state="baseline"; T.phaseStart=stamp
    elseif T.state=="baseline" and elapsed>=T.baselineSeconds then
        -- Register intent BEFORE a potentially failing call so cleanup also runs
        -- when the native method applies an effect and then Lua reports an error.
        T.ownsBuff=true
        local _,status=call(p.soul,"AddBuff",true,T.buffId)
        if status=="error" or status=="missing" or present(p)~=true then T.Cancel("add_not_confirmed");return end
        T.state="boost";T.phaseStart=stamp
        T.boostStart=call(p,"GetWorldPos",true)
        log("applied rms_before="..fmt(T.baselineRms).." rms_after="..fmt(rms(p)))
    elseif T.state=="boost" then
        if present(p)~=true or elapsed>=T.boostSeconds then
            local why=elapsed>=T.boostSeconds and "duration" or "engine_expired_or_unknown"
            if not T.Remove(why) then T.Cancel("cleanup_failed");return end
            T.state="recovery";T.phaseStart=stamp
        end
    elseif T.state=="recovery" and elapsed>=T.recoverySeconds then
        T.Sample(p,stamp);T.Cancel("complete");return
    end
    if stamp>=T.nextLog then T.Sample(p,stamp);T.nextLog=stamp+0.2 end
    T.Schedule()
end

function T.Schedule()
    local token,status=call(Script,"SetTimer",false,T.intervalMs,function()
        if KuttenbergThief~=M then return end
        local ok=pcall(T.Tick)
        if not ok then T.Cancel("callback_error") end
    end)
    T.timer=token
    if status~="ok" then T.Cancel("timer_unavailable") end
end

-- Explicit emergency cleanup only removes this mod's one buff, never all buffs.
function T.Reset()
    T.Cancel("manual_reset")
    local p=A.currentPlayer()
    if not p then log("reset=no_player");return end
    call(p.soul,"RemoveAllBuffsByGuid",true,T.buffId)
    local absent=present(p)==false
    T.locked=not absent
    if absent then T.ownsBuff=false;T.owner=nil;T.ownerId=nil end
    log("reset_absent="..tostring(absent))
end

for _,cmd in ipairs({
    {"kt_speed_test","KuttenbergThief.speedTrial.Begin()","KT: opt-in crouched speed comparison; keep ordinary movement input"},
    {"kt_speed_reset","KuttenbergThief.speedTrial.Reset()","KT: remove only our trial effect"}}) do
    call(System,"AddCCommand",false,cmd[1],cmd[2],cmd[3])
end
log("available=true auto_activation=false runtime_verified=false")
