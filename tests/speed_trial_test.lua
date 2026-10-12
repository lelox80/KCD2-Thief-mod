-- Behavioral tests with a fake clock/engine. No actual game or movement claims.
local t,sim,timers,nextId,logs,buff,p,adds,removes,failAdd,failRemove
local function contains(s)
    for _,v in ipairs(logs) do if string.find(v,s,1,true) then return true end end
    return false
end
local function setup()
    KuttenbergThief=nil
    t=10;sim=0;timers={};nextId=0;logs={};buff=false;adds=0;removes=0;failAdd=false;failRemove=false
    System={LogAlways=function(s) logs[#logs+1]=s end,AddCCommand=function()end,
        GetCurrAsyncTime=function()return t end,GetCurrTime=function()return sim end,
        GetViewCameraPos=function()return {x=0,y=0,z=1.2}end}
    Script={SetTimer=function(ms,fn)nextId=nextId+1;timers[nextId]=fn;return nextId end,
        KillTimer=function(id)timers[id]=nil end}
    UIAction=nil;AI=nil;Physics=nil;Game={IsLoadingEngineSaveGame=function()return false end}
    local no=function()return false end
    p={id=1,GetWorldPos=function()return {x=0,y=0,z=0}end,GetVelocity=function()return {x=0,y=0,z=0}end,
        actor={IsFlying=no,IsDead=no,IsUnconscious=no,IsCarryingCorpse=no,
            GetPhysicalizationProfile=function()return 'alive'end,
            GetCurrentAnimationState=function()return 'MotionMovement'end},
        human={IsMounted=no,IsInDialog=no,IsPickpocketing=no,IsWeaponDrawn=no,IsOnLadder=function()return 0 end},
        player={IsSitting=no,IsLaying=no,GetFlyMode=function()return 0 end},soul={IsInCombatDanger=no}}
    p.soul.HasBuffDebug=function()return buff end
    p.soul.GetDerivedStat=function(self,key)assert(key=='rms');return buff and 1.25 or 1 end
    p.soul.AddBuff=function(self,id)
        assert(id=='eacd8708-9dde-43a6-bddb-76d56b9338d1');adds=adds+1;buff=true
        if failAdd then error('after application')end
        return 'private-instance'
    end
    p.soul.RemoveAllBuffsByGuid=function(self,id)
        assert(id=='eacd8708-9dde-43a6-bddb-76d56b9338d1');removes=removes+1
        if failRemove then error('removal unavailable')end
        buff=false
    end
    p.SetVelocity=function()error('forbidden movement writer')end
    p.SetWorldPos=p.SetVelocity;p.actor.SimulateOnAction=p.SetVelocity
    player=p;g_localActor=nil
    dofile('src/Data/Scripts/KuttenbergThief/Probe.lua');KuttenbergThief.Stop('fixture')
    KuttenbergThief.generation=1
    KuttenbergThief.actorStates={crouch=true,inAir=false,cutscene=false,interactiveScene=false,
        dialogue=false,horse=false,minigame=false,actorAnim=false,onBed=false}
    KuttenbergThief.actorStatesTime=t
    dofile('src/Data/Scripts/KuttenbergThief/SpeedTrial.lua')
    return KuttenbergThief.speedTrial
end
local function step(dt,fresh)
    t=t+dt;sim=sim+dt
    if fresh~=false then KuttenbergThief.actorStatesTime=t end
    local id,fn=next(timers);assert(fn,'missing timer');timers[id]=nil;fn()
end
local function untilBoost(T)
    T.Begin()
    for i=1,110 do if T.state=='boost' then return end;step(0.05)end
    error('boost not reached')
end
local T=setup();assert(adds==0);T.Begin();T.Begin();assert(contains('refused=already_active'))
for i=1,180 do if not T.state then break end;step(0.05)end
assert(adds==1 and not buff and removes==1 and contains('end reason=complete'))
T.Begin();assert(contains('refused=cooldown'))

for _,case in ipairs({'crouch','stale','missing','camera','air','carry','unknown_buff','unknown_rms'})do
    T=setup()
    if case=='crouch' then KuttenbergThief.actorStates.crouch=false
    elseif case=='stale'then KuttenbergThief.actorStatesTime=0
    elseif case=='missing'then p.human.IsPickpocketing=nil
    elseif case=='camera'then System.GetViewCameraPos=function()return{x=0,y=0,z=26}end
    elseif case=='air'then p.actor.IsFlying=function()return true end
    elseif case=='carry'then p.actor.IsCarryingCorpse=function()return true end
    elseif case=='unknown_buff'then p.soul.HasBuffDebug=function()return nil end
    elseif case=='unknown_rms'then p.soul.GetDerivedStat=function()return 0/0 end end
    T.Begin();assert(not T.state and adds==0,case)
end
T=setup();untilBoost(T);step(3);assert(not buff and contains('reason=pause_or_time_gap'))
T=setup();untilBoost(T);KuttenbergThief.actorStates.dialogue=true;step(.05)
assert(not buff and contains('reason=actor_state_dialogue'))
T=setup();untilBoost(T);step(.31,false);assert(not buff and contains('reason=observer_stale'))
T=setup();untilBoost(T);KuttenbergThief.OnGameplayStarted();assert(not buff and not T.state)
KuttenbergThief.Stop('done')
T=setup();untilBoost(T);local originalSoul=p.soul;player={id=1,GetWorldPos=p.GetWorldPos,soul={}}
step(.05);assert(removes==0 and T.locked and contains('cleanup=owner_lost'))
T=setup();failAdd=true
T.Begin();for i=1,110 do if not T.state then break end;step(.05)end
assert(not buff and contains('reason=add_not_confirmed'))
T=setup();T.Begin();for i=1,105 do step(.05)end;assert(buff)
failRemove=true;T.Cancel('forced');assert(T.locked and buff)
failRemove=false;T.Reset();assert(not buff and not T.locked)
T=setup();untilBoost(T);buff=false;step(.05)
assert(T.state=='recovery' and contains('reason=engine_expired_or_unknown'))
T=setup();untilBoost(T);Script.SetTimer=function()error('scheduler lost')end;step(.05)
assert(not buff and not T.state and contains('reason=timer_unavailable'))
T=setup();T.Begin();local stale=timers[T.timer];T.Cancel('reload')
KuttenbergThief={};stale();assert(adds==0)
print('PASS: speed trial opt-in, refusal gates, bounded lifetime, cleanup, pause/load, stale observer, reused id, errors and reload')
