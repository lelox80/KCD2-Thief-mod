-- Lua 5.1. Main script-update thread only. This collector never writes gameplay.
-- Native expires the packet using its own QPC clock; async time is diagnostic.
-- Retail has no script IsGamePaused getter. The missing-only pause fallback
-- requires the protocol-2 native DLL pinned to retail WHGame SHA-256
-- bdf8f9e4a11257a72b64c84700e284c29e4c4ccaf5b8d4bfa7d0b2a7294479f7.
-- Original-byte evidence traces System.GetFrameTime to CTimer selector 0:
-- it returns zero when disabled or game-paused, unlike the separate UI timer.
local M = KuttenbergThief
local A = M.access
local get, call = A.member, A.invoke
-- The entry point establishes this sticky session before any mod reload. A
-- missing/replaced marker cannot be repaired by installing a new table later.
-- Native-only packaging excludes the legacy trial/buff definition; this gate
-- verifies provenance rather than guessing absence through an unavailable API.
local movementSession=M.movementSession
local function finite(v) return type(v)=="number" and v==v and math.abs(v)<1e12 end
local function integer(v) return finite(v) and v>=0 and v==math.floor(v) end
-- Retail Lua numbers are float32. Keep all counters inside this exact range;
-- never wrap an exhausted epoch or serial back to a previously accepted value.
local counterLimit=8388607
local function counter(v) return integer(v) and v<=counterLimit end
local epoch = _KTNativeBridgeEpoch
local epochFailed=false
if epoch==nil then epoch=0
elseif not counter(epoch) or epoch>=counterLimit then epochFailed=true end
if not epochFailed then epoch=epoch+1; _KTNativeBridgeEpoch=epoch end
if not counter(epoch) then epoch=0 end
local B={protocol=2, epoch=epoch, identityEpoch=0, sequence=0,
    requestSerial=0, resetSerial=0, enabled=true, dryRun=true,
    armSeconds=5, maxAge=0.15, requiredMask="b"..string.rep("1",25),
    counterLimit=counterLimit,counterFailed=epochFailed,
    nativeFailed=get(movementSession,"nativeFault")==true}
if epochFailed or B.nativeFailed then B.enabled=false end
M.nativeBridge=B

-- ASCII positions 1..25 encode gate bits 0..24 in that order. Do not build a
-- numeric mask: 25-bit words cannot be represented exactly in retail float32.
B.gates={"module", "generation", "player", "not_loading", "game_started",
    "not_paused", "frame_dt", "clock", "observer", "crouch", "contact",
    "alive", "resources", "not_carrying", "not_mounted", "not_ladder",
    "not_dialogue", "not_pickpocket", "weapon_sheathed", "not_combat",
    "ordinary_body", "normal_animation", "ordinary_xgen", "attached_camera",
    "native_only_session"}
local function log(s) M.Log("native_bridge "..s) end
local function failCounter(name)
    B.counterFailed=true; B.enabled=false; B.armed=nil
    if B[name]~=nil and not counter(B[name]) then B[name]=0 end
    B.lastInvalidation="counter_"..name
    return false
end
local function bump(name)
    if B.counterFailed or not counter(B[name]) or B[name]>=counterLimit then return failCounter(name) end
    B[name]=B[name]+1; return true
end
local function now() return call(System,"GetCurrAsyncTime",false) end
local function available() return type(_KTNativePublish)=="function" end
local function vec(v)
    return type(v)=="table" and finite(get(v,"x")) and finite(get(v,"y")) and finite(get(v,"z"))
end
local function read(object,name,method,...)
    local value,status=call(object,name,method,...)
    return value,status=="ok"
end
local function boolean(object,name,method,...)
    local value,ok=read(object,name,method,...)
    return value,ok and type(value)=="boolean"
end
local function numeric(object,name,method,...)
    local value,ok=read(object,name,method,...)
    return value,ok and finite(value)
end
local function entityToken(id)
    if counter(id) and id>0 then return string.format("%.0f",id),true end
    -- Native must distinguish LIGHTUSERDATA handles from full userdata and
    -- match the decoded entity ID to the current request owner. Never format
    -- a userdata storage address as an identifier or log the original handle.
    if type(id)=="userdata" then return "0",true end
    if type(id)=="string" then
        local ok,s=pcall(tostring,id)
        if ok and #s<=10 and (string.match(s,"^%d+$") or string.match(s,"^0[xX][%da-fA-F]+$")) then
            local n=tonumber(s)
            if counter(n) and n>0 then return string.format("%.0f",n),true end
        end
    end
    return "0",false
end
B.EntityToken=entityToken

local function diagnosticTime(v) return finite(v) and string.format("%.6f",v) or "unknown" end
function B.Invalidate(reason,stamp,delta)
    bump("resetSerial")
    B.armed=nil
    B.lastInvalidation=reason or "transition"
    if (B.invalidationLogs or 0)<40 then
        B.invalidationLogs=(B.invalidationLogs or 0)+1
        log("invalidate reason="..B.lastInvalidation.." reset="..string.format("%.0f",B.resetSerial)
            .." canonical="..diagnosticTime(B.lastStamp).." sample="..diagnosticTime(stamp)
            .." delta="..diagnosticTime(delta))
    end
end

function B.Capture(stamp,advanceClock)
    if integer(M.generation) and M.generation>counterLimit then failCounter("generation") end
    if integer(M.actorStatesSequence) and M.actorStatesSequence>counterLimit then failCounter("observerSequence") end
    local p=A.currentPlayer()
    local id=get(p,"id")
    if p~=B.owner or id~=B.ownerId or M.generation~=B.generation then
        B.owner=p; B.ownerId=id; B.generation=M.generation
        bump("identityEpoch"); B.Invalidate("player_generation")
    end
    local token,validId=entityToken(id)
    local a,h,pi,so=get(p,"actor"),get(p,"human"),get(p,"player"),get(p,"soul")
    local knownBits,safeBits,reason={},{},nil
    local function gate(i,k,s)
        knownBits[i]=k and "1" or "0"
        safeBits[i]=k and s and "1" or "0"
        if not (k and s) and not reason then reason=B.gates[i] end
    end
    gate(1,true,KuttenbergThief==M)
    gate(2,counter(M.generation) and type(M.hadPlayer)=="boolean",counter(M.generation) and M.generation>0 and M.hadPlayer==true)
    gate(3,p~=nil and validId,p~=nil and validId)
    local loading,kl=boolean(Game,"IsLoadingEngineSaveGame",false)
    local started,ks=boolean(CryAction,"IsGameStarted",false)
    local dt,kdt=numeric(System,"GetFrameTime",false)
    local paused,pauseStatus=call(CryAction,"IsGamePaused",false)
    local kp,notPaused
    if pauseStatus=="missing" then
        -- Derived gate, rather than treating a nonexistent getter as false.
        -- An existing getter that errors, returns nil or has the wrong type
        -- remains unknown and never falls back to the timer.
        kp=kdt; notPaused=kdt and dt>0; B.pauseSource="retail_game_timer_0"
    else
        kp=pauseStatus=="ok" and type(paused)=="boolean"
        notPaused=paused==false; B.pauseSource="explicit_pause_getter"
    end
    gate(4,kl,loading==false); gate(5,ks,started==true); gate(6,kp,notPaused)
    gate(7,kdt,kdt and dt>0 and dt<=B.maxAge)
    local clockKnown=finite(stamp)
    local continuous=clockKnown and (B.lastStamp==nil or (stamp>=B.lastStamp and stamp-B.lastStamp<=B.maxAge))
    gate(8,clockKnown,continuous)
    -- Console callbacks can report an async sample ahead of the next script
    -- frame. Only the canonical native-driven Tick may advance/checkpoint this
    -- clock or invalidate an arm because of a clock discontinuity.
    if advanceClock then
        if not continuous and (B.lastStamp~=nil or B.armed) and not B.clockBroken then
            local why=clockKnown and B.lastStamp and stamp<B.lastStamp and "clock_rollback" or "clock_gap"
            B.Invalidate(why,stamp,clockKnown and B.lastStamp and stamp-B.lastStamp or nil)
        end
        B.clockBroken=not continuous
        if clockKnown then B.lastStamp=stamp end
    end
    local s=M.actorStates
    local observerKnown=type(s)=="table" and finite(M.actorStatesTime) and clockKnown
        and counter(M.actorStatesSequence) and M.actorStatesSequence>0
    for _,key in ipairs({"crouch","inAir","cutscene","interactiveScene","dialogue","horse","minigame","actorAnim","onBed"}) do
        if type(get(s,key))~="boolean" then observerKnown=false end
    end
    local age=observerKnown and stamp-M.actorStatesTime or -1
    local fresh=observerKnown and age>=0 and age<=B.maxAge
    gate(9,observerKnown,fresh)
    gate(10,type(get(s,"crouch"))=="boolean",get(s,"crouch")==true)
    local flying,kf=boolean(a,"IsFlying",true)
    gate(11,kf and type(get(s,"inAir"))=="boolean",flying==false and get(s,"inAir")==false)
    local dead,kd=boolean(a,"IsDead",true)
    local unconscious,ku=boolean(a,"IsUnconscious",true)
    local profile,kpr=read(a,"GetPhysicalizationProfile",true)
    gate(12,kd and ku and kpr and type(profile)=="string",dead==false and unconscious==false and profile=="alive")
    local health,kh=numeric(so,"GetState",true,"health")
    local stamina,kst=numeric(so,"GetState",true,"stamina")
    gate(13,kh and kst,kh and kst and health>0 and stamina>0)
    local carry,kc=boolean(a,"IsCarryingCorpse",true)
    gate(14,kc,carry==false)
    local mounted,km=boolean(h,"IsMounted",true)
    gate(15,km and type(get(s,"horse"))=="boolean",mounted==false and get(s,"horse")==false)
    local ladder,kla=numeric(h,"IsOnLadder",true)
    gate(16,kla,ladder==0)
    local dialogue,kdi=boolean(h,"IsInDialog",true)
    gate(17,kdi and type(get(s,"dialogue"))=="boolean",dialogue==false and get(s,"dialogue")==false)
    local pick,kpi=boolean(h,"IsPickpocketing",true)
    gate(18,kpi,pick==false)
    local weapon,kw=boolean(h,"IsWeaponDrawn",true)
    gate(19,kw,weapon==false)
    local combat,kco=boolean(so,"IsInCombatDanger",true)
    gate(20,kco,combat==false)
    local sitting,ksi=boolean(pi,"IsSitting",true)
    local laying,kly=boolean(pi,"IsLaying",true)
    local flyMode,kfm=numeric(pi,"GetFlyMode",true)
    gate(21,ksi and kly and kfm,sitting==false and laying==false and flyMode==0)
    local animation,ka=read(a,"GetCurrentAnimationState",true)
    gate(22,ka and type(animation)=="string",animation=="MotionIdle" or animation=="MotionMovement")
    local xk,xs=true,true
    for _,key in ipairs({"cutscene","interactiveScene","minigame","actorAnim","onBed"}) do
        xk=xk and type(get(s,key))=="boolean"; xs=xs and get(s,key)==false
    end
    gate(23,xk,xs)
    local pos,kpos=read(p,"GetWorldPos",true)
    local camera,kcam=read(System,"GetViewCameraPos",false)
    local vectors=kpos and kcam and vec(pos) and vec(camera)
    gate(24,vectors,vectors and math.abs(camera.x-pos.x)<=0.6 and math.abs(camera.y-pos.y)<=0.6
        and camera.z-pos.z>=0.6 and camera.z-pos.z<=2.2)
    local currentSession=M.movementSession
    local sessionKnown=type(movementSession)=="table" and type(currentSession)=="table"
        and type(_KTMovementSession)=="table"
        and type(get(movementSession,"protocol"))=="number"
        and type(get(movementSession,"nativeOnly"))=="boolean"
        and type(get(movementSession,"clean"))=="boolean"
    gate(25,sessionKnown,sessionKnown and movementSession==currentSession
        and movementSession==_KTMovementSession and get(movementSession,"protocol")==2
        and get(movementSession,"nativeOnly")==true and get(movementSession,"clean")==true
        and _KTLegacySpeedTrialSeen==nil and M.speedTrial==nil)
    local known="b"..table.concat(knownBits)
    local safe="b"..table.concat(safeBits)
    if B.counterFailed then known="b"..string.rep("0",25);safe=known;reason="counter_exhausted" end
    return {known=known,safe=safe,reason=reason or "ok",entity=token,originalId=id,
        observerSequence=counter(M.actorStatesSequence) and M.actorStatesSequence or 0,
        stamp=clockKnown and stamp or -1,dt=kdt and dt or -1,age=age,
        health=kh and health or -1,stamina=kst and stamina or -1}
end

local function publish(state)
    if not available() then
        if B.armed then B.Invalidate("publisher_missing") end
        B.available=false; return false
    end
    B.available=true
    bump("sequence")
    if B.counterFailed then
        state.known="b"..string.rep("0",25);state.safe=state.known;state.reason="counter_exhausted"
    end
    local fields={"v2",string.format("%.0f",B.epoch),string.format("%.0f",counter(M.generation) and M.generation or 0),
        string.format("%.0f",B.identityEpoch),state.entity,string.format("%.0f",B.sequence),
        string.format("%.6f",state.stamp),string.format("%.6f",state.dt),
        string.format("%.6f",state.age),string.format("%.0f",state.observerSequence),state.known,state.safe,
        string.format("%.0f",B.requestSerial),string.format("%.0f",B.resetSerial),
        B.enabled and "1" or "0",B.dryRun and "1" or "0",
        string.format("%.6f",state.health),string.format("%.6f",state.stamina),state.reason}
    local packet=table.concat(fields,"|")
    if #packet>512 then B.enabled=false; log("refused=packet_length"); return false end
    local ok=pcall(_KTNativePublish,packet,state.originalId)
    if not ok then B.available=false; B.Invalidate("publisher_error") end
    B.last=state
    return ok
end

function B.Tick()
    local stamp=now()
    local state=B.Capture(stamp,true)
    B.lastFrame=state; B.frameIdentityEpoch=B.identityEpoch
    B.frameGeneration=M.generation; B.frameOwner=B.owner
    -- If delivery disappears before this frame, discard intent without
    -- advancing its serial. It must not surface when the publisher returns.
    if B.armed and not available() then B.Invalidate("publisher_missing") end
    if B.armed and finite(stamp) and (B.armed.immediate or stamp>=B.armed.deadline) then
        bump("requestSerial"); B.armed=nil
        log("request="..B.requestSerial.." gate="..state.reason.." dry_run="..tostring(B.dryRun))
    end
    local delivered=publish(state)
    -- Optional bounded telemetry runs after publication on this script thread.
    -- Its observations never enter the movement packet or promote safety gates.
    local terrain=get(M,"terrainProbe")
    local sample=get(terrain,"Tick")
    if delivered and type(sample)=="function" then pcall(sample,state) end
    -- Read-only HUD work follows packet delivery and never contributes gates.
    local stealth=get(M,"stealthHud")
    local draw=get(stealth,"Tick")
    if delivered and type(draw)=="function" then pcall(draw,state) end
    local ledge=get(M,"ledgeProbe")
    local scan=get(ledge,"Tick")
    if delivered and type(scan)=="function" then pcall(scan,state) end
    return delivered
end

local function publishCancellation()
    -- Commands can cancel immediately, but cannot publish an eligible frame or
    -- advance its clock. Reuse the last canonical sample even if command-time
    -- observation is ahead; the clock known/safe characters are both zero.
    local state=B.Capture(B.lastStamp,false)
    state.known=string.sub(state.known,1,8).."0"..string.sub(state.known,10)
    state.safe=string.sub(state.safe,1,8).."0"..string.sub(state.safe,10)
    state.reason="command_cancel"
    return publish(state)
end

function B.Begin(immediate)
    if B.nativeFailed then log("refused=native_fault restart_required=true"); return false end
    if B.counterFailed then log("refused=counter_exhausted"); return false end
    if not available() then log("refused=native_unavailable"); return false end
    if not B.enabled then log("refused=disabled"); return false end
    if B.armed then log("refused=already_armed"); return false end
    local stamp=now()
    if not finite(stamp) then log("refused=clock_unknown"); return false end
    -- Establish the owner before arming. A future table/id/generation change
    -- consumes/cancels the intent instead of carrying it to another player.
    local state=B.Capture(stamp,false)
    if B.counterFailed then log("refused=counter_exhausted"); return false end
    if immediate then
        if state.known~=B.requiredMask or state.safe~=B.requiredMask then
            log("refused="..state.reason.." immediate=true"); return false
        end
        local frame=B.lastFrame
        if not frame or frame.known~=B.requiredMask or frame.safe~=B.requiredMask then
            log("refused=native_frame_unavailable immediate=true"); return false
        end
        if B.frameIdentityEpoch~=B.identityEpoch or B.frameGeneration~=M.generation or B.frameOwner~=B.owner then
            log("refused=native_frame_identity immediate=true"); return false
        end
        if not finite(frame.stamp) or stamp<frame.stamp or stamp-frame.stamp>B.maxAge then
            log("refused=native_frame_stale immediate=true"); return false
        end
    end
    local delay=immediate and 0 or B.armSeconds
    B.armed={deadline=stamp+delay,immediate=immediate==true}
    -- Immediate is processed once on the next canonical Tick, even when its
    -- stamp trails this command sample. Native then consumes/refuses it using
    -- the actual current request, input and ownership; it never waits for input.
    log("armed delay="..delay.." dry_run="..tostring(B.dryRun)
        .." command="..diagnosticTime(stamp).." canonical="..diagnosticTime(B.lastStamp)
        .." lead="..diagnosticTime(finite(B.lastStamp) and stamp-B.lastStamp or nil))
    return true
end

function B.Reset()
    B.Invalidate("manual_reset"); publishCancellation()
    log("reset="..B.resetSerial)
end
function B.Mode(mode)
    B.Invalidate("mode_change")
    B.enabled=mode~="disabled" and not B.counterFailed and not B.nativeFailed; B.dryRun=mode~="enabled" or B.nativeFailed==true
    publishCancellation()
    log("enabled="..tostring(B.enabled).." dry_run="..tostring(B.dryRun).." native="..tostring(B.available))
end
-- Called only by the pinned script-thread hook after a native fault. Never
-- attempts to resume the failed writer or publish another movement request.
function B.NativeFault()
    if type(movementSession)=="table" then movementSession.nativeFault=true end
    B.nativeFailed=true;B.enabled=false;B.dryRun=true;B.available=false
    B.Invalidate("native_fault");B.lastFrame=nil
    local terrain=get(M,"terrainProbe")
    local stop=get(terrain,"Stop")
    if type(stop)=="function" then pcall(stop,"native_fault") end
    local hudStop=get(get(M,"stealthHud"),"Stop")
    if type(hudStop)=="function" then pcall(hudStop,"native_fault") end
    local ledgeStop=get(get(M,"ledgeProbe"),"Stop")
    if type(ledgeStop)=="function" then pcall(ledgeStop,"native_fault") end
    log("native_fault movement_disabled=true restart_required=true")
end
for _,cmd in ipairs({
    {"kt_native_bridge","KuttenbergThief.nativeBridge.Mode('observe')","KT: native bridge diagnostics, no swoop writes"},
    {"kt_native_enable","KuttenbergThief.nativeBridge.Mode('enabled')","KT: explicitly enable bounded native prototype"},
    {"kt_native_disable","KuttenbergThief.nativeBridge.Mode('disabled')","KT: cancel and disable native prototype"},
    {"kt_native_swoop","KuttenbergThief.nativeBridge.Begin()","KT: one native swoop request after 5 seconds"},
    {"kt_swoop","KuttenbergThief.nativeBridge.Begin(true)","KT: one immediate native swoop request; current safe crouch required"},
    {"kt_native_reset","KuttenbergThief.nativeBridge.Reset()","KT: cancel native request immediately"}}) do
    call(System,"AddCCommand",false,cmd[1],cmd[2],cmd[3])
end
log("loaded protocol=2 epoch="..B.epoch.." dry_run=true movement_writer=false")
