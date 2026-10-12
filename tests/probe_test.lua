local logs, timers, nextId = {}, {}, 0
local function contains(fragment)
    for _, line in ipairs(logs) do if string.find(line, fragment, 1, true) then return true end end
    return false
end
local function count(fragment)
    local n = 0
    for _, line in ipairs(logs) do if string.find(line, fragment, 1, true) then n=n+1 end end
    return n
end
System = {LogAlways=function(s) logs[#logs+1]=s end,
    AddCCommand=function() end, GetCurrTime=function() return 1 end,
    GetViewCameraPos=function() return {x=1,y=2,z=3} end,
    GetViewCameraDir=function() return {x=0,y=1,z=0} end}
Script = {SetTimer=function(ms, fn) nextId=nextId+1; timers[nextId]=fn; return nextId end,
    KillTimer=function(id) timers[id]=nil end}
UIAction = {RegisterEventSystemListener=function() end}
local function step()
    local id, fn = next(timers); assert(id, 'expected timer'); timers[id]=nil; fn()
end
dofile('src/Data/Scripts/KuttenbergThief/Probe.lua')
assert(contains('boot version=0.0.5'))
step(); assert(count('sample reason=watch')==0)
local writes = 0
player = {id=1, GetWorldPos=function() return {x=1,y=2,z=3} end,
    GetVelocity=function() return {x=0,y=0,z=0} end,
    SetVelocity=function() writes=writes+1 end,
    actor={GetCurrentAnimationState=function() error('unavailable') end}}
AI = {GetStance=function(id) assert(id==1 or id==2); return 1 end}
player.GetPhysicalStats=function() return {mass=80,gravity={x=0,y=0,z=-13}} end
player.soul={GetState=function(self,name) assert(name=='health' or name=='stamina'); return 100 end}
step(); assert(contains('player_available')); assert(contains('anim=unknown:error'))
assert(contains('ai_stance_raw=1.0000')); assert(contains('stamina=100.0000'))
assert(contains('detail=physical_stats status=ok gravity=(0.0000,0.0000,-13.0000) | mass=80.0000'))
assert(contains('stance_raw=unknown:missing'))
assert(contains('grounded=unknown')); assert(writes==0)
local flight = false
player.actor.IsFlying=function() return flight end
step(); assert(contains('contact_change flying_raw=false status=ok'))
local contactCount=count('contact_change')
step(); assert(count('contact_change')==contactCount)
flight=true; step(); assert(contains('contact_change flying_raw=true status=ok'))
player.actor.IsFlying=function() error('transition') end
step(); assert(contains('contact_change flying_raw=unknown status=error'))
player=nil; step(); assert(contains('player_lost'))
player={id=2,GetWorldPos=function() return {x=9,y=8,z=7} end}
step(); assert(contains('generation=2')); assert(contains('pos=(9.0000,8.0000,7.0000)'))
KuttenbergThief.Stop('test'); assert(next(timers)==nil)
KuttenbergThief.maxSamples=3; KuttenbergThief.Watch()
step(); step(); step(); assert(next(timers)==nil); assert(contains('watch_complete'))
KuttenbergThief.Rays(); assert(contains('ray skipped='))
ent_all=123
Physics={RayWorldIntersection=function(pos, dir, max, mask, skip)
    assert(dir.y==3 and max==1 and mask==ent_all and skip==2)
    return {{dist=1,pos={x=1,y=3,z=3},normal={x=0,y=-1,z=0}}}
end}
KuttenbergThief.Rays(); assert(contains('ray status=ok')); assert(writes==0)
KuttenbergThief.Watch(); local stale = timers[KuttenbergThief.timer]
dofile('src/Data/Scripts/KuttenbergThief/Probe.lua')
local before=count('sample reason=watch'); stale(); assert(count('sample reason=watch')==before)
KuttenbergThief.Stop('done')
KuttenbergThief.BeginActorObservation()
KuttenbergThief.ObserveActorState('crouch',true)
KuttenbergThief.CommitActorObservation()
assert(KuttenbergThief.actorStates==nil)
KuttenbergThief.BeginActorObservation()
for _,key in ipairs({'crouch','inAir','cutscene','interactiveScene','dialogue','horse','minigame','actorAnim','onBed'}) do
    KuttenbergThief.ObserveActorState(key,false)
end
KuttenbergThief.CommitActorObservation()
assert(KuttenbergThief.actorStates.crouch==false)
assert(contains('actor_observer crouch=false'))
KuttenbergThief.OnGameplayStarted()
assert(KuttenbergThief.actorStates==nil)
KuttenbergThief.Stop('done')
print('PASS: absent player, missing APIs, getter errors, entity replacement, bounded logging, stop/reload, ray contract; no movement writes')
