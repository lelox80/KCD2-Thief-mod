-- Main-thread collector fixture; no retail-process or collision claims.
local t,p,M,B,packets,logs
local source='src/Data/Scripts/KuttenbergThief/NativeBridge.lua'
local observerKeys={'crouch','inAir','cutscene','interactiveScene','dialogue','horse','minigame','actorAnim','onBed'}
local function setup()
    t=10;packets={};logs={}
    local no=function()return false end
    local zero=function()return 0 end
    System={GetCurrAsyncTime=function()return t end,GetFrameTime=function()return 1/60 end,
        GetViewCameraPos=function()return{x=0,y=0,z=1.2}end,AddCCommand=function()end}
    Game={IsLoadingEngineSaveGame=no}
    CryAction={IsGameStarted=function()return true end,IsGamePaused=no}
    p={id=37,GetWorldPos=function()return{x=0,y=0,z=0}end,
        actor={IsFlying=no,IsDead=no,IsUnconscious=no,IsCarryingCorpse=no,
            GetPhysicalizationProfile=function()return'alive'end,
            GetCurrentAnimationState=function()return'MotionMovement'end},
        human={IsMounted=no,IsOnLadder=zero,IsInDialog=no,IsPickpocketing=no,IsWeaponDrawn=no},
        player={IsSitting=no,IsLaying=no,GetFlyMode=zero},
        soul={IsInCombatDanger=no,GetState=function(_,key)
            assert(key=='health' or key=='stamina');return 100 end}}
    -- Any attempted mutation immediately fails the test.
    local write=function()error('forbidden gameplay writer')end
    p.SetVelocity=write;p.SetWorldPos=write;p.actor.SimulateOnAction=write
    System.SetCVar=write;p.soul.AddBuff=write;p.soul.RemoveAllBuffsByGuid=write
    local function member(o,k)
        if o==nil then return nil end
        local ok,v=pcall(function()return o[k]end);if ok then return v end
    end
    local function invoke(o,k,method,...)
        local fn=member(o,k);if type(fn)~='function'then return nil,'missing'end
        local ok,v
        if method then ok,v=pcall(fn,o,...)else ok,v=pcall(fn,...)end
        if not ok then return nil,'error'end
        return v,v==nil and 'nil'or'ok'
    end
    _KTLegacySpeedTrialSeen=nil
    _KTMovementSession={protocol=2,nativeOnly=true,clean=true}
    M={generation=4,hadPlayer=true,actorStatesTime=t,actorStatesSequence=1,
        actorStates={crouch=true,inAir=false,cutscene=false,interactiveScene=false,
            dialogue=false,horse=false,minigame=false,actorAnim=false,onBed=false},
        movementSession=_KTMovementSession,
        Log=function(s)logs[#logs+1]=s end,
        access={member=member,invoke=invoke,currentPlayer=function()return p end}}
    KuttenbergThief=M
    _KTNativePublish=function(packet,id)
        assert(type(packet)=='string' and #packet<=512 and not string.find(packet,'[^ -~]'))
        assert(id==p.id,'publisher must pass original handle')
        local fields={}
        for field in string.gmatch(packet..'|','([^|]*)|')do fields[#fields+1]=field end
        assert(#fields==19,'fixed protocol field count')
        assert(fields[1]=='v2')
        local n={}
        for i=2,18 do
            if i==11 or i==12 then
                assert(#fields[i]==26 and string.match(fields[i],'^b[01]+$'),'ASCII mask '..i)
            else n[i]=assert(tonumber(fields[i]),'numeric field '..i)end
        end
        packets[#packets+1]={raw=packet,fields=fields,epoch=n[2],generation=n[3],identity=n[4],
            entity=n[5],sequence=n[6],stamp=n[7],dt=n[8],age=n[9],observer=n[10],
            known=fields[11],safe=fields[12],request=n[13],reset=n[14],enabled=n[15],dry=n[16],
            health=n[17],stamina=n[18],reason=fields[19]}
    end
    dofile(source);B=M.nativeBridge
    return B
end
local function tick(dt,fresh)
    t=t+(dt or .05)
    if fresh~=false then M.actorStatesTime=t;M.actorStatesSequence=M.actorStatesSequence+1 end
    assert(B.Tick());return packets[#packets]
end
local function bit(mask,index)
    assert(type(mask)=='string' and #mask==26 and string.match(mask,'^b[01]+$'))
    return tonumber(string.sub(mask,index+1,index+1))
end
local function unsafe(index,unknown)
    local state=B.Capture(t)
    assert(bit(state.safe,index)==0,'safe gate '..index)
    if unknown then assert(bit(state.known,index)==0,'unknown gate '..index)end
    assert(state.safe~=B.requiredMask)
end

setup();local v=tick();assert(v.known==B.requiredMask and v.safe==B.requiredMask and v.reason=='ok')
assert(B.protocol==2 and B.requiredMask=='b'..string.rep('1',25))
assert(v.entity==37 and v.generation==4 and v.dry==1 and v.enabled==1 and v.request==0)
assert(v.health==100 and v.stamina==100 and v.observer==2)
assert(v.reset==1 and v.identity==1)
assert(type(v.known)=='string' and type(v.safe)=='string' and tonumber(v.known)==nil)
-- Each gate occupies exactly one character, including the 25th gate. This
-- cannot pass by constructing a 25-bit numeric word and rounding in float32.
local singleFailures={
    function()KuttenbergThief={}end,
    function()M.hadPlayer=false end,
    function()p.id=0 end,
    function()Game.IsLoadingEngineSaveGame=function()return true end end,
    function()CryAction.IsGameStarted=function()return false end end,
    function()CryAction.IsGamePaused=function()return true end end,
    function()System.GetFrameTime=function()return .151 end end,
    function()B.lastStamp=t-.2 end,
    function()M.actorStatesTime=t-.151 end,
    function()M.actorStates.crouch=false end,
    function()p.actor.IsFlying=function()return true end end,
    function()p.actor.IsDead=function()return true end end,
    function()p.soul.GetState=function()return 0 end end,
    function()p.actor.IsCarryingCorpse=function()return true end end,
    function()p.human.IsMounted=function()return true end end,
    function()p.human.IsOnLadder=function()return 1 end end,
    function()p.human.IsInDialog=function()return true end end,
    function()p.human.IsPickpocketing=function()return true end end,
    function()p.human.IsWeaponDrawn=function()return true end end,
    function()p.soul.IsInCombatDanger=function()return true end end,
    function()p.player.IsSitting=function()return true end end,
    function()p.actor.GetCurrentAnimationState=function()return'Attack'end end,
    function()M.actorStates.cutscene=true end,
    function()System.GetViewCameraPos=function()return{x=0,y=0,z=26}end end,
    function()_KTLegacySpeedTrialSeen=true end}
assert(#singleFailures==25)
for i,change in ipairs(singleFailures)do
    setup();change();local state=B.Capture(t)
    local expected='b'..string.rep('1',i-1)..'0'..string.rep('1',25-i)
    assert(state.safe==expected,'independent ASCII gate '..i)
end

-- Every getter disappearance and thrown error is unknown, never a safe default.
local getters={
    {'game','IsLoadingEngineSaveGame',4},{'cry','IsGameStarted',5},{'cry','IsGamePaused',6},
    {'system','GetFrameTime',7},{'system','GetCurrAsyncTime',8},
    {'actor','IsFlying',11},{'actor','IsDead',12},{'actor','IsUnconscious',12},
    {'actor','GetPhysicalizationProfile',12},{'soul','GetState',13},
    {'actor','IsCarryingCorpse',14},{'human','IsMounted',15},{'human','IsOnLadder',16},
    {'human','IsInDialog',17},{'human','IsPickpocketing',18},{'human','IsWeaponDrawn',19},
    {'soul','IsInCombatDanger',20},{'player','IsSitting',21},{'player','IsLaying',21},
    {'player','GetFlyMode',21},{'actor','GetCurrentAnimationState',22},
    {'body','GetWorldPos',24},{'system','GetViewCameraPos',24}}
for _,g in ipairs(getters)do
    for mode=1,3 do
        setup()
        local groups={game=Game,cry=CryAction,system=System,body=p,actor=p.actor,human=p.human,player=p.player,soul=p.soul}
        if mode==1 then groups[g[1]][g[2]]=nil
        else groups[g[1]][g[2]]=function()
            if mode==2 then error('getter failed')end;return nil end end
        local state=B.Capture(M.access.invoke(System,'GetCurrAsyncTime',false))
        if g[3]==6 and mode==1 then
            assert(bit(state.known,6)==1 and bit(state.safe,6)==1 and B.pauseSource=='retail_game_timer_0')
        else assert(bit(state.known,g[3])==0 and bit(state.safe,g[3])==0,g[2]..' mode '..mode)end
    end
end
-- Original-retail CTimer proof supports a derived missing-only pause gate.
-- It shares one GetFrameTime read with the frame-dt gate. Zero/negative is
-- unsafe; missing, NaN, nil and errors are unknown. Existing pause getters
-- never fall back merely because they fail or return an unexpected type.
setup();CryAction.IsGamePaused=nil;local dtReads=0
System.GetFrameTime=function()dtReads=dtReads+1;return 1/60 end
assert(B.Capture(t).safe==B.requiredMask and dtReads==1 and B.pauseSource=='retail_game_timer_0')
for _,dt in ipairs({0,-.01})do
    setup();CryAction.IsGamePaused=nil;System.GetFrameTime=function()return dt end
    unsafe(6)
end
setup();CryAction.IsGamePaused=nil;System.GetFrameTime=nil;unsafe(6,true)
setup();CryAction.IsGamePaused=nil;System.GetFrameTime=function()return nil end;unsafe(6,true)
setup();CryAction.IsGamePaused=nil;System.GetFrameTime=function()return 0/0 end;unsafe(6,true)
setup();CryAction.IsGamePaused=nil;System.GetFrameTime=function()error('timer failed')end;unsafe(6,true)
setup();CryAction.IsGamePaused=function()return 0 end;unsafe(6,true)
setup();CryAction.IsGamePaused=function()return nil end;unsafe(6,true)
setup();CryAction.IsGamePaused=function()error('pause getter failed')end;unsafe(6,true)
for _,key in ipairs(observerKeys)do setup();M.actorStates[key]=nil;unsafe(9,true)end
setup();M.actorStates=nil;unsafe(9,true)
setup();M.actorStatesTime=nil;unsafe(9,true)
setup();M.actorStatesSequence=nil;unsafe(9,true)
setup();M.actorStatesSequence=0;unsafe(9,true)
setup();M.actorStatesTime=t-.151;unsafe(9)
setup();M.actorStatesTime=t+.001;unsafe(9)
setup();M.actorStates.crouch=false;unsafe(10)
setup();M.actorStates.inAir=true;unsafe(11)
setup();p.actor.IsFlying=function()return true end;unsafe(11)
setup();p.actor.IsFlying=function()return 0 end;unsafe(11,true)
setup();p.actor.IsDead=function()return true end;unsafe(12)
setup();p.actor.IsUnconscious=function()return true end;unsafe(12)
setup();p.actor.GetPhysicalizationProfile=function()return'ragdoll'end;unsafe(12)
setup();p.soul.GetState=function()return 0 end;unsafe(13)
setup();p.soul.GetState=function()return 0/0 end;unsafe(13,true)
setup();p.actor.IsCarryingCorpse=function()return true end;unsafe(14)
setup();p.human.IsMounted=function()return true end;unsafe(15)
setup();M.actorStates.horse=true;unsafe(15)
setup();p.human.IsOnLadder=function()return 1 end;unsafe(16)
setup();p.human.IsInDialog=function()return true end;unsafe(17)
setup();M.actorStates.dialogue=true;unsafe(17)
setup();p.human.IsPickpocketing=function()return true end;unsafe(18)
setup();p.human.IsWeaponDrawn=function()return true end;unsafe(19)
setup();p.soul.IsInCombatDanger=function()return true end;unsafe(20)
setup();p.player.IsSitting=function()return true end;unsafe(21)
setup();p.player.IsLaying=function()return true end;unsafe(21)
setup();p.player.GetFlyMode=function()return 1 end;unsafe(21)
setup();p.actor.GetCurrentAnimationState=function()return'Attack'end;unsafe(22)
for _,key in ipairs({'cutscene','interactiveScene','minigame','actorAnim','onBed'})do
    setup();M.actorStates[key]=true;unsafe(23)
end
setup();System.GetViewCameraPos=function()return{x=0,y=0,z=26}end;unsafe(24)
-- Retail HasBuffDebug is absent. Native-only session provenance replaces that
-- unsupported getter; neither nil buff results nor cleanup are inferred here.
setup();assert(p.soul.HasBuffDebug==nil and B.Capture(t).safe==B.requiredMask)
setup();p.soul.HasBuffDebug=function()error('unsupported API must not be called')end
assert(B.Capture(t).safe==B.requiredMask)
setup();M.speedTrial={};unsafe(25)
setup();M.speedTrial=false;unsafe(25)
for _,marker in ipairs({true,false,'legacy'})do setup();_KTLegacySpeedTrialSeen=marker;unsafe(25)end
for _,field in ipairs({'protocol','nativeOnly','clean'})do
    setup();M.movementSession[field]=nil;unsafe(25,true)
end
setup();M.movementSession.protocol='2';unsafe(25,true)
setup();M.movementSession.nativeOnly=1;unsafe(25,true)
setup();M.movementSession.clean='true';unsafe(25,true)
setup();M.movementSession.protocol=1;unsafe(25)
setup();M.movementSession.nativeOnly=false;unsafe(25)
setup();M.movementSession.clean=false;unsafe(25)
setup();_KTMovementSession=nil;unsafe(25,true)
setup();M.movementSession=nil;unsafe(25,true)
setup();_KTMovementSession={protocol=2,nativeOnly=true,clean=true};unsafe(25)
setup();M.movementSession={protocol=2,nativeOnly=true,clean=true};unsafe(25)
setup();_KTMovementSession={protocol=2,nativeOnly=true,clean=true};M.movementSession=_KTMovementSession;unsafe(25)
-- A missing capture-time marker stays unknown even if a marker appears later.
setup();M.movementSession=nil;_KTMovementSession=nil;dofile(source);B=M.nativeBridge
_KTMovementSession={protocol=2,nativeOnly=true,clean=true};M.movementSession=_KTMovementSession
unsafe(25,true)
-- Reload preserves both a clean sticky marker and a poisoned sticky marker.
setup();local session=M.movementSession;dofile(source);B=M.nativeBridge
assert(M.movementSession==session and B.Capture(t).safe==B.requiredMask)
session.clean=false;_KTLegacySpeedTrialSeen=true;dofile(source);B=M.nativeBridge
assert(M.movementSession==session);unsafe(25)
_KTLegacySpeedTrialSeen=nil;dofile(source);B=M.nativeBridge;unsafe(25)
setup();Game.IsLoadingEngineSaveGame=function()return true end;unsafe(4)
setup();CryAction.IsGameStarted=function()return false end;unsafe(5)
setup();CryAction.IsGamePaused=function()return true end;unsafe(6)
setup();System.GetFrameTime=function()return 0 end;unsafe(7)
setup();System.GetFrameTime=function()return .151 end;unsafe(7)
setup();M.hadPlayer=false;unsafe(2)
setup();M.generation=0;unsafe(2)
setup();M.generation=nil;unsafe(2,true)
setup();p=nil;unsafe(3,true)
setup();p.id={};unsafe(3,true)
setup();p.id='userdata: 000001234567';unsafe(3,true)
setup();p.id='0x7FFFFF';assert(tick().entity==8388607)
setup();p.id='0xFFFFFFFF';unsafe(3,true)
setup();p.id='8388608';unsafe(3,true)
setup();p.id='4294967296';unsafe(3,true)
setup();p.id=0;unsafe(3,true)
if newproxy then setup();p.id=newproxy();assert(tick().entity==0)end

-- Explicit requests are delayed, single use, and never reissued when a failed
-- eligibility gate becomes safe later. Native consumes the published serial.
setup();assert(B.armSeconds==5 and B.Begin());for i=1,99 do tick()end;assert(B.requestSerial==0)
tick(.051);assert(B.requestSerial==1 and not B.armed)
for i=1,10 do tick()end;assert(B.requestSerial==1)
setup();M.actorStates.crouch=false;assert(B.Begin())
for i=1,101 do tick()end;assert(B.requestSerial==1 and packets[#packets].safe~=B.requiredMask)
M.actorStates.crouch=true;assert(tick().safe==B.requiredMask and B.requestSerial==1)
setup();assert(B.Begin());assert(not B.Begin());B.Reset()
assert(not B.armed and B.requestSerial==0)
for i=1,101 do tick()end;assert(B.requestSerial==0)

-- Command-context time can lead the following canonical script-frame time.
-- Identity checks still run in Begin, but this sample never checkpoints the
-- frame clock, toggles its broken state, or publishes an eligible snapshot.
setup();tick();local canonical=B.lastStamp;local priorReset=B.resetSerial
t=canonical+.08;local command=t;local count=#packets
assert(B.Begin() and #packets==count and B.lastStamp==canonical and B.clockBroken==false)
t=canonical+.04;M.actorStatesTime=t;M.actorStatesSequence=M.actorStatesSequence+1
assert(t<command and B.Tick() and B.armed and B.resetSerial==priorReset)
assert(B.lastStamp==t and B.clockBroken==false and packets[#packets].safe==B.requiredMask)
for i=1,105 do tick()end;assert(B.requestSerial==1 and not B.armed)
-- Direct diagnostic captures also leave the canonical clock untouched.
setup();tick();canonical=B.lastStamp
B.Capture(canonical+1);assert(B.lastStamp==canonical and B.clockBroken==false)
-- Command reset/mode packets reuse the canonical stamp and force the clock
-- known/safe bits to zero. A later native frame can observe normally again.
setup();tick();canonical=B.lastStamp;t=canonical+.08;B.Mode('enabled')
v=packets[#packets]
assert(v.reason=='command_cancel' and bit(v.known,8)==0 and bit(v.safe,8)==0)
assert(math.abs(v.stamp-canonical)<.00001 and B.lastStamp==canonical and B.clockBroken==false)
assert(B.Begin());B.Reset();v=packets[#packets]
assert(not B.armed and v.reason=='command_cancel' and bit(v.known,8)==0 and bit(v.safe,8)==0)
assert(math.abs(v.stamp-canonical)<.00001 and B.lastStamp==canonical)
t=canonical+.04;M.actorStatesTime=t;M.actorStatesSequence=M.actorStatesSequence+1
assert(B.Tick() and packets[#packets].safe==B.requiredMask and B.requestSerial==0)
-- Genuine native-frame rollback/gaps keep the original strict refusal and
-- clear the pending arm; command sampling cannot hide either discontinuity.
setup();tick();assert(B.Begin());canonical=B.lastStamp;t=canonical-.01
M.actorStatesTime=t;M.actorStatesSequence=M.actorStatesSequence+1
assert(B.Tick() and not B.armed and B.clockBroken and B.lastInvalidation=='clock_rollback')
assert(bit(packets[#packets].safe,8)==0)
setup();tick();assert(B.Begin());canonical=B.lastStamp
B.Capture(canonical+.08);v=tick(.151)
assert(not B.armed and B.clockBroken and B.lastInvalidation=='clock_gap' and bit(v.safe,8)==0)

-- Replacement with a reused ID, generation changes and time gaps invalidate
-- pending intent immediately. An obsolete module cannot publish a safe packet.
setup();tick();assert(B.Begin());local identity=B.identityEpoch;local reset=B.resetSerial
local replacement={};for k,x in pairs(p)do replacement[k]=x end;p=replacement
v=tick();assert(v.identity==identity+1 and v.reset==reset+1 and not B.armed and v.request==0)
setup();tick();assert(B.Begin());M.generation=M.generation+1
tick();assert(not B.armed and B.requestSerial==0)
setup();tick();assert(B.Begin());v=tick(.151)
assert(bit(v.safe,8)==0 and not B.armed and B.requestSerial==0)
assert(tick().safe==B.requiredMask,'one fresh frame after gap can observe again')
setup();tick();assert(B.Begin());t=t-.1;v=tick(0)
assert(bit(v.safe,8)==0 and not B.armed)
setup();tick();local oldEpoch=B.epoch;dofile(source);B=M.nativeBridge
assert(B.epoch==oldEpoch+1 and tick().request==0)
setup();tick();local old=B;KuttenbergThief={};assert(old.Capture(t).safe~=old.requiredMask)

-- Diagnostics are the default. Native availability is required to arm; changing
-- modes or resetting cancels an outstanding arm and publishes the reset.
setup();assert(B.dryRun and B.enabled);B.Mode('enabled')
assert(not B.dryRun and packets[#packets].dry==0);assert(B.Begin())
B.Mode('disabled');assert(not B.enabled and not B.armed and not B.Begin())
B.Mode('observe');assert(B.enabled and B.dryRun and packets[#packets].dry==1)
setup();_KTNativePublish=nil;assert(not B.Tick() and not B.Begin())
setup();assert(B.Begin());_KTNativePublish=function()error('native unavailable')end
assert(not B.Tick() and not B.armed)

-- kt_swoop stages one immediate intent for the next canonical frame. It never
-- bypasses fresh-state gates, publishes from the command, or waits for later
-- safe state/input. Physical key binding/edge behavior belongs to the host.
setup();assert(not B.Begin(true) and not B.armed and B.requestSerial==0)
setup();tick();canonical=B.lastStamp;count=#packets
assert(B.Begin(true) and B.armed.immediate and not B.Begin(true))
assert(#packets==count and B.lastStamp==canonical and B.clockBroken==false)
assert(tick().request==1 and not B.armed)
for i=1,10 do tick()end;assert(B.requestSerial==1)
-- The following native frame may precede command time; immediate means that
-- first frame, not a queued request that waits for its timestamp to catch up.
setup();tick();canonical=B.lastStamp;t=canonical+.08;command=t
assert(B.Begin(true));t=canonical+.04
M.actorStatesTime=t;M.actorStatesSequence=M.actorStatesSequence+1
assert(t<command and B.Tick() and B.requestSerial==1 and not B.armed)
assert(packets[#packets].safe==B.requiredMask and B.lastStamp==t)
-- Unsafe command-time state is refused. Returning to safety does not create a
-- request; another explicit activation is necessary.
setup();tick();M.actorStates.crouch=false
assert(not B.Begin(true) and not B.armed and B.requestSerial==0)
M.actorStates.crouch=true;tick();assert(B.requestSerial==0)
assert(B.Begin(true) and tick().request==1)
setup();tick();p.actor.IsFlying=nil
assert(not B.Begin(true) and not B.armed);p.actor.IsFlying=function()return false end
tick();assert(B.requestSerial==0)
-- Current safe reads cannot replace a missing/unsafe prior canonical frame.
setup();M.actorStates.crouch=false;tick();M.actorStates.crouch=true
assert(not B.Begin(true));tick();assert(B.Begin(true))
-- A gate change after the command is published once as unsafe, and its serial
-- stays consumed when safety returns. Native also requires nonzero live input.
setup();tick();assert(B.Begin(true));M.actorStates.crouch=false
v=tick();assert(v.request==1 and v.safe~=B.requiredMask and not B.armed)
M.actorStates.crouch=true;v=tick();assert(v.safe==B.requiredMask and v.request==1)
setup();tick();assert(B.Begin(true));v=tick(.151)
assert(v.request==0 and not B.armed and bit(v.safe,8)==0)
tick();assert(B.requestSerial==0)
setup();tick();assert(B.Begin(true));M.generation=M.generation+1
tick();assert(not B.armed and B.requestSerial==0)
setup();tick();assert(B.Begin(true));B.Reset();tick()
assert(not B.armed and B.requestSerial==0)
-- Publisher loss before processing discards intent without advancing a serial
-- that could activate after delivery returns. An error after processing bumps
-- the reset serial, so the returned publisher must baseline the consumed ID.
setup();tick();assert(B.Begin(true));local publisher=_KTNativePublish
_KTNativePublish=nil;assert(not B.Tick() and not B.armed and B.requestSerial==0)
_KTNativePublish=publisher;tick();assert(B.requestSerial==0)
setup();tick();assert(B.Begin(true));publisher=_KTNativePublish;priorReset=B.resetSerial
_KTNativePublish=function()error('delivery failed')end
assert(not B.Tick() and not B.armed and B.requestSerial==1 and B.resetSerial>priorReset)
_KTNativePublish=publisher;tick();assert(B.requestSerial==1)

-- Native writer failure is visible and sticky; mode changes cannot reactivate
-- Passive terrain callbacks execute only after packet delivery, and neither
-- callback errors nor missing diagnostics can change the published state.
setup();local terrainCalls,hudCalls,ledgeCalls=0,0,0
M.terrainProbe={Tick=function(state)
    terrainCalls=terrainCalls+1
    assert(state==B.lastFrame and state==B.last)
    assert(packets[#packets].sequence==B.sequence)
    error('isolated diagnostic failure')
end}
M.stealthHud={Tick=function(state)
    hudCalls=hudCalls+1
    assert(terrainCalls==hudCalls,'HUD follows even a failed terrain callback')
    assert(state==B.lastFrame and packets[#packets].sequence==B.sequence)
    error('isolated renderer/getter failure')
end}
M.ledgeProbe={Tick=function(state)
    ledgeCalls=ledgeCalls+1
    assert(hudCalls==ledgeCalls and state==B.lastFrame and state==B.last)
    error('isolated geometry failure')
end}
local delivered=B.Tick();assert(delivered and terrainCalls==1 and hudCalls==1 and ledgeCalls==1 and not B.counterFailed)
local sender=_KTNativePublish;_KTNativePublish=function()error('delivery failed')end
assert(not B.Tick() and terrainCalls==1 and hudCalls==1 and ledgeCalls==1);_KTNativePublish=sender
local terrainStopped,ledgeStopped=false,false
M.terrainProbe.Stop=function(reason)terrainStopped=reason=='native_fault';error('isolated stop failure')end
M.ledgeProbe.Stop=function(reason)ledgeStopped=reason=='native_fault' end
B.NativeFault();assert(terrainStopped and ledgeStopped and B.nativeFailed and not B.enabled)
-- Native writer failure is visible and sticky; mode changes cannot reactivate
-- it, and no staged intent survives even if an old closure remains present.
setup();tick();assert(B.Begin(true));B.NativeFault()
assert(B.nativeFailed and not B.enabled and B.dryRun and not B.armed and B.lastFrame==nil)
assert(not B.Begin(true));B.Mode('enabled');assert(not B.enabled and B.dryRun)
assert(not B.Begin());assert(not B.armed)
dofile(source);B=M.nativeBridge
assert(B.nativeFailed and not B.enabled and not B.Begin(true))
-- Counter boundaries stay exact in retail float32; exhaustion is latched and
-- publishes an invalid disabled snapshot instead of wrapping or rearming.
local zeroMask='b'..string.rep('0',25)
local function exhausted()
    assert(B.counterFailed and not B.enabled and not B.armed and not B.Begin())
    local state=B.Capture(t);assert(state.known==zeroMask and state.safe==zeroMask)
    B.Mode('enabled');assert(not B.enabled and packets[#packets].enabled==0)
    assert(packets[#packets].known==zeroMask and packets[#packets].safe==zeroMask)
end
setup();tick();B.sequence=B.counterLimit-1
assert(tick().sequence==8388607 and not B.counterFailed)
tick();assert(B.sequence==8388607);exhausted()
setup();tick();assert(B.Begin());B.requestSerial=B.counterLimit
for i=1,101 do tick()end
assert(B.requestSerial==8388607);exhausted()
setup();tick();assert(B.Begin());B.resetSerial=B.counterLimit;B.Reset();exhausted()
setup();tick();assert(B.Begin());B.identityEpoch=B.counterLimit
local another={};for k,x in pairs(p)do another[k]=x end;p=another;tick();exhausted()
setup();M.generation=B.counterLimit+1;tick();exhausted()
setup();M.actorStatesSequence=B.counterLimit+1;B.Tick();exhausted()
setup();local savedEpoch=_KTNativeBridgeEpoch
_KTNativeBridgeEpoch=B.counterLimit-1;dofile(source);B=M.nativeBridge
assert(B.epoch==8388607 and not B.counterFailed)
dofile(source);B=M.nativeBridge
assert(B.epoch==8388607 and _KTNativeBridgeEpoch==8388607);B.Tick();exhausted()
_KTNativeBridgeEpoch=savedEpoch
-- Optional cross-runtime wire fixture: these are the collector's actual bytes,
-- rather than a second protocol implementation inside the native parser test.
if arg and arg[1] then
    local out=assert(io.open(arg[1],'wb'))
    setup();out:write(tick().raw,'\n')
    p.actor.IsFlying=function()return true end;out:write(tick().raw,'\n')
    p.actor.IsFlying=nil;out:write(tick().raw,'\n')
    setup();KuttenbergThief={};out:write(tick().raw,'\n')
    assert(out:close())
end
print('PASS: protocol2 exact ASCII25 gates, bounded float32 counters, fresh XGen, identity/reload, one-shot arming and native-only session; no gameplay writes')
