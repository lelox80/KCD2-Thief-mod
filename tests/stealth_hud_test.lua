-- Real collector -> owned UI contract, with forbidden gameplay writers.
local t,p,M,B,H,draws,logs,reads,packets,stats,frameDt,paused,loading,started,ui,uiShown
local function member(o,k)
    if o==nil then return nil end
    local ok,v=pcall(function()return o[k]end);if ok then return v end
end
local function invoke(o,k,method,...)
    local fn=member(o,k);if type(fn)~='function'then return nil,'missing'end
    local ok,v
    if method then ok,v=pcall(fn,o,...)else ok,v=pcall(fn,...)end
    if not ok then return nil,'error'end
    return v,v==nil and'nil'or'ok'
end
local forbiddenWrites=0
local function forbid()forbiddenWrites=forbiddenWrites+1;error('forbidden gameplay write')end
local function setup()
    t=10;frameDt=1/60;paused=false;loading=false;started=true
    draws={};logs={};reads=0;packets=0;ui={};uiShown=false
    stats={vib=-.57,evi=.2,noi=0,fsm=.3,con=.1}
    _KTStealthHudLogs=nil;_KTNativeBridgeEpoch=0;_KTLegacySpeedTrialSeen=nil
    _KTMovementSession={protocol=2,nativeOnly=true,clean=true}
    local no=function()return false end
    System={AddCCommand=function()end,GetCurrAsyncTime=function()return t end,
        GetFrameTime=function()return frameDt end,
        GetViewCameraPos=function()return{x=0,y=0,z=1.2}end,
        GetViewport=function()return{width=1920,height=1080}end,
        DrawText=function(x,y,text)draws[#draws+1]={x=x,y=y,text=text}end,
        SetCVar=forbid}
    Game={IsLoadingEngineSaveGame=function()return loading end}
    CryAction={IsGameStarted=function()return started end,IsGamePaused=function()return paused end}
    local function event(kind,element,instance,...)
        assert(element=='KTStealth'and instance==0,'only the owned UI instance is mutable')
        local e={kind=kind,args={...}};ui[#ui+1]=e;return e
    end
    UIAction={
        ShowElement=function(e,i)event('show',e,i);uiShown=true;return true end,
        HideElement=function(e,i)event('hide',e,i);uiShown=false;return true end,
        UnloadElement=function(e,i)event('unload',e,i);uiShown=false;return true end,
        CallFunction=function(e,i,fn,...)
            assert(fn=='Update'and uiShown,'show must precede the heartbeat')
            assert(select('#',...)==2,'noise must stay outside the graphical contract')
            event('update',e,i,...);return'kt15'
        end}
    p={id=37,GetWorldPos=function()return{x=0,y=0,z=0}end,
        actor={IsFlying=no,IsDead=no,IsUnconscious=no,IsCarryingCorpse=no,
            GetPhysicalizationProfile=function()return'alive'end,
            GetCurrentAnimationState=function()return'MotionMovement'end,
            SimulateOnAction=forbid},
        human={IsMounted=no,IsOnLadder=function()return 0 end,IsInDialog=no,
            IsPickpocketing=no,IsWeaponDrawn=no},
        player={IsSitting=no,IsLaying=no,GetFlyMode=function()return 0 end},
        soul={IsInCombatDanger=no,GetState=function()return 100 end,
            GetDerivedStat=function(_,key)
                assert(key=='vib'or key=='evi'or key=='noi'or key=='fsm'or key=='con')
                reads=reads+1;return stats[key]
            end,AddBuff=forbid,RemoveAllBuffsByGuid=forbid},
        SetVelocity=forbid,SetWorldPos=forbid,AddImpulse=forbid}
    M={generation=1,hadPlayer=true,movementSession=_KTMovementSession,
        actorStatesTime=t,actorStatesSequence=1,
        actorStates={crouch=true,inAir=false,cutscene=false,interactiveScene=false,
            dialogue=false,horse=false,minigame=false,actorAnim=false,onBed=false},
        Log=function(s)logs[#logs+1]=s end,
        access={member=member,invoke=invoke,currentPlayer=function()return p end}}
    KuttenbergThief=M
    _KTNativePublish=function(packet,id)
        assert(type(packet)=='string'and id==p.id);packets=packets+1
    end
    dofile('src/Data/Scripts/KuttenbergThief/NativeBridge.lua');B=M.nativeBridge
    dofile('src/Data/Scripts/KuttenbergThief/StealthHud.lua');H=M.stealthHud
end
local function tick(dt,fresh)
    t=t+(dt or 1/60)
    if fresh~=false then M.actorStatesTime=t;M.actorStatesSequence=M.actorStatesSequence+1 end
    assert(B.Tick(),'HUD must not fail publication')
end
local function contains(list,fragment)
    for _,v in ipairs(list)do
        if string.find(type(v)=='table'and v.text or v,fragment,1,true)then return true end
    end
    return false
end
local function count(kind)
    local n=0;for _,e in ipairs(ui)do if e.kind==kind then n=n+1 end end;return n
end
local function latest(kind)
    for i=#ui,1,-1 do if ui[i].kind==kind then return ui[i]end end
end
local function near(a,b)return math.abs(a-b)<.00001 end

setup();tick()
assert(H.visible and H.rendererMode=='graphic'and reads==5 and #draws==0)
assert(count('show')==1 and count('update')==1)
local update=latest('update')
assert(near(update.args[1],(stats.vib+2)/5.5)and update.args[2]==true)
setup();stats.vib=0;stats.noi=0/0;tick();update=latest('update')
assert(H.visible and update.args[2]==true,'zero visibility is known; unknown background noise cannot hide the gem')
assert(H.Snapshot().values.noi.status=='nonfinite','retain typed background noise evidence')
setup();tick()
assert(contains(logs,'vib_status=ok')and contains(logs,'generation=1'))
for i=1,4 do tick()end
assert(reads==5 and count('update')==5 and count('show')==1,'heartbeat each frame; cache getters')
stats.vib=1.2;tick(.11);assert(reads==10)
update=latest('update')
assert(near(update.args[1],(1.2+2)/5.5),'pass the mapped target; the owned movie smooths once')
local copy=H.Snapshot();copy.values.vib.value=99
assert(H.Snapshot().values.vib.value~=99,'snapshot must not expose owned cache')
assert(H.Snapshot().visibilityMinimum==-2 and H.Snapshot().visibilityMaximum==3.5)
local beforeReads=reads;H.Status();assert(reads==beforeReads,'status must not read host stats')
setup();stats.vib=-99;stats.noi=99;tick();update=latest('update')
assert(update.args[1]==0 and update.args[2]==true,'own bounded presentation range')
assert(H.Snapshot().values.vib.value==-99 and H.Snapshot().values.noi.value==99,'never clamp host evidence')

-- Feedback includes ordinary standing and movement-ineligible states.
setup();M.actorStates.crouch=false
p.actor.IsFlying=function()return true end
p.human.IsWeaponDrawn=function()return true end
p.soul.GetState=function()return nil end
p.soul.IsInCombatDanger=function()return true end
tick();assert(H.visible and B.last.safe~=B.requiredMask)

-- Actual producer gates suppress unsupported/lifecycle contexts.
local contexts={
    function()loading=true end,function()started=false end,function()paused=true end,
    function()frameDt=0 end,function()M.actorStates.dialogue=true end,
    function()M.actorStates.cutscene=true end,function()M.actorStates.interactiveScene=true end,
    function()M.actorStates.minigame=true end,function()M.actorStates.onBed=true end,
    function()M.actorStates.horse=true end,function()p.actor.IsDead=function()return true end end,
    function()p.human.IsOnLadder=function()return 1 end end,
    function()System.GetViewCameraPos=function()return{x=0,y=0,z=10}end end,
}
for _,change in ipairs(contexts)do
    setup();tick();local before=count('update');change();tick()
    assert(not H.visible and not uiShown and count('update')==before and H.Snapshot().stamp==nil)
    assert(count('hide')==1,'invalidate an already-visible persistent movie immediately')
end
setup();tick();local before=count('update');tick(.3,false)
assert(not H.visible and not uiShown and count('update')==before,'stale observer/gap must hide')
tick();assert(H.visible and reads==10,'resume reacquires current readings')
before=count('update');t=t-1;assert(B.Tick());assert(not H.visible and not uiShown and count('update')==before)

-- Missing/error/nonfinite inputs remain typed unknown; zero remains a value.
setup();stats.vib=0/0;stats.fsm=math.huge;stats.noi=0
tick();local snapshot=H.Snapshot();update=latest('update')
assert(not H.visible and count('show')==0,'noise alone cannot display or fabricate a visibility gem')
assert(snapshot.values.vib.status=='nonfinite'and snapshot.values.fsm.status=='nonfinite')
setup();p.soul.GetDerivedStat=nil;tick();snapshot=H.Snapshot()
assert(not H.visible and count('show')==0 and snapshot.values.vib.status=='missing')
assert(contains(logs,'vib_status=missing')and H.enabled)
setup();p.soul.GetDerivedStat=function()error('unavailable')end;tick()
assert(not H.visible and H.Snapshot().values.noi.status=='error'and H.enabled)
setup();stats={evi=.2,fsm=.3,con=.1};tick()
assert(not H.visible and count('show')==0,'diagnostic-only values must not draw empty indicators')
setup();tick();stats={evi=.2,fsm=.3,con=.1};tick(.11)
assert(not H.visible and not uiShown and count('hide')==1,'losing visibility hides the old gem')
setup();tick();stats.vib=nil;tick(.11)
assert(not H.visible and not uiShown,'a lost visibility channel cannot show a stale value')
stats.vib=3.5;tick(.11);assert(latest('update').args[1]==1,'a returning channel starts from its actual value')

-- A replacement during getters discards the complete partial sample.
setup();p.soul.GetDerivedStat=function(_,key)
    if key=='vib'then p={id=44};return .5 end
    return 0
end
tick();assert(not H.visible and count('show')==0 and H.Snapshot().stamp==nil)
setup();tick();local old=p;local another={}
for k,v in pairs(old)do another[k]=v end
p=another;p.id=44;M.generation=2;stats.vib=2
tick();assert(H.visible and H.Snapshot().generation==2 and reads==10)
assert(H.Snapshot().values.vib.value==2,'new identity must never reuse old readings')

-- Graphic failures explicitly fall back to text; no repeated failed UI loads.
setup();UIAction=nil;tick()
assert(H.enabled and H.visible and H.rendererFailed and H.rendererMode=='text')
assert(contains(draws,'HUD text: ui_api_missing')and contains(draws,'VIS -0.570'))
assert(not contains(draws,'NOISE'),'no noise readout in the fallback either')
assert(B.available and not B.nativeFailed and packets==1)
local failures={
    {'ShowElement',function()return false end,'ui_show_refused'},
    {'ShowElement',function()error('load failure')end,'ui_show_error'},
    {'CallFunction',function()return false end,'ui_update_receipt'},
    {'CallFunction',function()return true end,'ui_update_receipt'},
    {'CallFunction',function()return nil end,'ui_update_nil'},
    {'CallFunction',function()return'wrongmovie'end,'ui_update_receipt'},
    {'CallFunction',function()error('invoke failure')end,'ui_update_error'},
}
for _,case in ipairs(failures)do
    setup();UIAction[case[1]]=case[2];tick()
    assert(H.enabled and H.visible and H.rendererMode=='text'and not uiShown)
    assert(H.rendererReason==case[3]and count('unload')==1)
    local n=#ui;tick();assert(#ui==n,'sticky failures avoid repeated failed loads')
    assert(B.available and packets==2 and B.requestSerial==0)
end
setup();local original=UIAction.CallFunction
UIAction.CallFunction=function()return false end;tick();local n=#ui
UIAction.CallFunction=original;tick();assert(#ui==n and H.rendererMode=='text')
H.On();tick();assert(H.visible and H.rendererMode=='graphic'and not H.rendererFailed)
setup();UIAction=nil;System.DrawText=nil;tick()
assert(not H.enabled and H.rendererFailed and B.available and packets==1)
setup();UIAction=nil;System.DrawText=function()error('renderer unavailable')end;tick()
assert(not H.enabled and B.available and packets==1)
setup();UIAction=nil;System.GetViewport=nil;tick()
assert(draws[#draws].x==24,'fixed shipped coordinate fallback')
setup();tick();M.access.currentPlayer=function()error('access failure')end
assert(H.Tick(B.lastFrame)==false and not H.enabled and not uiShown,'callback errors hide only the owned HUD')

-- Status freshness does not require another game callback or host stat read.
setup();tick();t=t+.3;snapshot=H.Status()
assert(not snapshot.visible and snapshot.stamp==nil and snapshot.frame==nil and not uiShown)
setup();tick();_KTNativePublish=nil;snapshot=H.Snapshot()
assert(not snapshot.visible and snapshot.values.vib.value==nil)
setup();tick();B.NativeFault()
assert(not H.enabled and not H.visible and not uiShown and H.Snapshot().stamp==nil)
setup();tick();H.Invalidate('watch_restart')
assert(H.enabled and H.Snapshot().stamp==nil);tick();assert(H.visible)
H.Off();assert(not H.enabled and H.Snapshot().frame==nil)
assert(not uiShown and count('unload')==1)
local total=#ui;tick();assert(#ui==total)
H.On();tick();assert(H.visible)
setup();tick();local oldHud=H
dofile('src/Data/Scripts/KuttenbergThief/StealthHud.lua');H=M.stealthHud
assert(not oldHud.enabled and not uiShown and count('unload')==1,'reload removes the old owned instance')
tick();assert(H.visible and H.rendererMode=='graphic')

-- XML exposes the exact owned movie/Update contract and no input ownership.
local file=assert(io.open('src/Data/Libs/UI/UIElements/KTStealth.xml','r'))
local xml=file:read('*a');file:close()
assert(string.find(xml,'name="KTStealth"',1,true)and string.find(xml,'file="KTStealth.swf"',1,true))
assert(string.find(xml,'name="Update" funcname="kt_update"',1,true))
for _,key in ipairs({'visibilityAmount','visibilityKnown'})do
    assert(string.find(xml,'name="'..key..'"',1,true))
end
assert(not string.find(xml,'noiseAmount',1,true)and not string.find(xml,'noiseKnown',1,true))
for _,key in ipairs({'mouseevents','keyevents','cursor','controller_input','events_exclusive'})do
    assert(string.find(xml,key..'="0"',1,true))
end

setup()
for i=1,700 do stats.vib=i;tick(.11)end
assert(_KTStealthHudLogs==120 and H.visible and H.Snapshot().values.vib.value==700)
assert(B.requestSerial==0 and B.dryRun and B.enabled,'HUD cannot arm or enable movement')
assert(forbiddenWrites==0,'errors must never hide an attempted gameplay write')
print('PASS: real bridge -> owned graphical HUD, heartbeat/cache/lifecycle/unknown/fallback isolation and bounded logs')
