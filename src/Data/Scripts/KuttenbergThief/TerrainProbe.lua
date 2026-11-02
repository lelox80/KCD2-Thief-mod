-- Passive diagnostics only. No terrain forecast, safety evidence, gameplay
-- writes, timers or native calls. Root invokes Tick after a native publish.
local M=KuttenbergThief
local A=M.access
local get,call=A.member,A.invoke
if M.terrainProbe and type(M.terrainProbe.Stop)=="function" then
    pcall(M.terrainProbe.Stop,"terrain_reload")
end
local T={active=false,maxSamples=900,maxSeconds=90,maxSessions=32,interval=0.1}
M.terrainProbe=T
local LIMIT,MAX_SAMPLES,MAX_SECONDS,MAX_SESSIONS,MAX_EVENTS=8388607,900,90,32,2048
local function finite(v)return type(v)=="number" and v==v and math.abs(v)<1e12 end
local function counter(v)return finite(v) and v>=0 and v<=LIMIT and v==math.floor(v)end
local function number(v)return finite(v) and string.format("%.6f",v) or "unknown"end
local function count(v)return counter(v) and string.format("%.0f",v) or "unknown"end
local function boolean(v)return type(v)=="boolean" and tostring(v) or "unknown"end
local function token(v)
    if type(v)=="string" and #v>0 and #v<=64 and string.match(v,"^[%w_]+$") then return v end
    return "unknown"
end
local function mask(v)
    if type(v)=="string" and #v==26 and string.match(v,"^b[01]+$") then return v end
    return "unknown"
end
local function entity(v)
    if type(v)=="string" and #v>0 and #v<=10 and string.match(v,"^%d+$") then return v end
    return "unknown"
end
local function log(s)pcall(M.Log,"terrain "..s)end
local function refuse(reason)
    if (T.refusals or 0)<16 then
        T.refusals=(T.refusals or 0)+1
        log("v=1 event=refused reason="..token(reason).." diagnostic=true")
    end
    return false
end
local function vector(p,name)
    local ok,v,status=pcall(call,p,name,true)
    if not ok then return "unknown","error",false end
    if status~="ok" then
        local statuses={missing=true,error=true,["nil"]=true}
        return "unknown",statuses[status] and status or "unknown",false
    end
    if type(v)~="table" then return "unknown","shape",false end
    local x,y,z=get(v,"x"),get(v,"y"),get(v,"z")
    if not finite(x) or not finite(y) or not finite(z) then return "unknown","nonfinite",false end
    return "("..number(x)..","..number(y)..","..number(z)..")","ok",true
end
local function record(state,p)
    local b=T.bridge
    local pos,ps,pv=vector(p,"GetWorldPos")
    local velocity,vs,vv=vector(p,"GetVelocity")
    return {generation=count(M.generation),epoch=count(b.epoch),identity=count(b.identityEpoch),
        request=count(b.requestSerial),reset=count(b.resetSerial),frame=count(b.sequence),
        observer=count(get(state,"observerSequence")),entity=entity(get(state,"entity")),
        async=number(get(state,"stamp")),dt=number(get(state,"dt")),
        known=mask(get(state,"known")),safe=mask(get(state,"safe")),gate=token(get(state,"reason")),
        pos=pos,pos_status=ps,pos_valid=boolean(pv),velocity=velocity,velocity_status=vs,velocity_valid=boolean(vv),
        native_enabled=boolean(b.enabled),dry_run=boolean(b.dryRun)}
end
local fields={"generation","epoch","identity","request","reset","frame","observer","entity",
    "async","dt","known","safe","gate","pos","pos_status","pos_valid",
    "velocity","velocity_status","velocity_valid","native_enabled","dry_run"}
local function emit(event,r,extra)
    T.events=counter(T.events) and T.events<LIMIT and T.events+1 or LIMIT
    local parts={"v=1","event="..event,"phase="..T.phase,"session="..count(T.session),
        "seq="..count(T.events),"samples="..count(T.samples),"diagnostic=true"}
    for _,key in ipairs(fields)do parts[#parts+1]=key.."="..(r and r[key] or "unknown")end
    if extra then parts[#parts+1]=extra end
    log(table.concat(parts," "))
end
function T.Stop(reason)
    if not T.active then return false end
    T.active=false
    local why=token(reason or "command")
    local truncated=why=="sample_limit" or why=="duration_limit" or why=="event_limit" or why=="counter_invalid"
    -- Position remains cached, but end the native-audit window at the latest
    -- canonical frame when it still belongs to this exact bound context.
    local boundaryFrame=T.last and T.last.frame or "unknown"
    local boundaryAsync=T.last and T.last.async or "unknown"
    local boundaryValid=false
    local b=M.nativeBridge
    if KuttenbergThief==M and b==T.bridge and b.epoch==T.epoch
        and b.identityEpoch==T.identity and M.generation==T.generation
        and b.generation==T.generation and b.owner==T.owner and b.ownerId==T.ownerId
        and counter(b.sequence) and finite(b.lastStamp) then
        boundaryFrame=count(b.sequence);boundaryAsync=number(b.lastStamp)
        boundaryValid=true
    end
    emit("stop",T.last,"reason="..why.." elapsed="..number(T.lastTime and T.started and T.lastTime-T.started)
        .." truncated="..tostring(truncated).." cached=true boundary_frame="..boundaryFrame.." boundary_async="..boundaryAsync
        .." boundary_valid="..tostring(boundaryValid))
    T.owner=nil;T.ownerId=nil;T.bridge=nil
    return true
end
local function start(phase)
    if phase~="slope" and phase~="stairs" and phase~="ledge" then return refuse("phase")end
    if T.active then return refuse("already_active")end
    if KuttenbergThief~=M then return refuse("module_changed")end
    local b=M.nativeBridge
    if type(b)~="table" or type(b.lastFrame)~="table" then return refuse("native_frame_unavailable")end
    local state=b.lastFrame
    local p=A.currentPlayer()
    local id=get(p,"id")
    if not p or id==nil or b.owner~=p or b.ownerId~=id or get(state,"originalId")~=id then return refuse("owner_unknown")end
    if not counter(M.generation) or M.generation==0 or b.generation~=M.generation
        or not counter(b.epoch) or b.epoch==0 or not counter(b.identityEpoch) or b.identityEpoch==0
        or not counter(b.sequence) or not counter(b.requestSerial) or not counter(b.resetSerial)
        or not finite(state.stamp) or state.stamp<0 then return refuse("context_unknown")end
    if _KTTerrainSessions==nil then _KTTerrainSessions=0 end
    if not counter(_KTTerrainSessions) or _KTTerrainSessions>=MAX_SESSIONS then return refuse("session_limit")end
    _KTTerrainSessions=_KTTerrainSessions+1
    T.active=true;T.session=_KTTerrainSessions;T.phase=phase;T.samples=0;T.events=0
    T.last=nil
    T.bridge=b;T.epoch=b.epoch;T.generation=M.generation;T.identity=b.identityEpoch;T.reset=b.resetSerial
    T.owner=p;T.ownerId=id;T.started=state.stamp;T.lastTime=state.stamp;T.nextSample=state.stamp
    T.last=record(state,p)
    emit("start",T.last,"elapsed=0.000000 truncated=false cached=false")
    return true
end
function T.Start(phase)
    local ok,result=pcall(start,phase)
    if not ok then T.Stop("callback_error");return refuse("callback_error")end
    return result
end
local function tick(state)
    if not T.active then return false end
    if KuttenbergThief~=M then return T.Stop("module_changed")end
    local b=M.nativeBridge
    if b~=T.bridge then return T.Stop("bridge_changed")end
    if b.epoch~=T.epoch then return T.Stop("epoch_changed")end
    if M.generation~=T.generation or b.generation~=T.generation then return T.Stop("generation_changed")end
    if b.identityEpoch~=T.identity then return T.Stop("identity_changed")end
    if not counter(b.sequence) or not counter(b.requestSerial) or not counter(b.resetSerial)
        or not counter(b.identityEpoch) or not counter(b.epoch) or not counter(T.samples)
        or not counter(T.events) or not counter(T.session) then return T.Stop("counter_invalid")end
    -- The existing typed bridge loading gate is only used to end this trace;
    -- telemetry never writes or promotes any gate back into movement evidence.
    local known,safe=mask(get(state,"known")),mask(get(state,"safe"))
    if known~="unknown" and safe~="unknown" and string.sub(known,5,5)=="1"
        and string.sub(safe,5,5)=="0" then return T.Stop("loading")end
    if M.hadPlayer==false then return T.Stop("gameplay_inactive")end
    local p=A.currentPlayer()
    local id=get(p,"id")
    if p~=T.owner or id~=T.ownerId or b.owner~=p or b.ownerId~=id
        or get(state,"originalId")~=id then return T.Stop("player_changed")end
    local stamp=get(state,"stamp")
    if not finite(stamp) or stamp<0 then return T.Stop("clock_unknown")end
    if stamp<T.lastTime then return T.Stop("clock_rollback")end
    local delta=stamp-T.lastTime
    T.lastTime=stamp
    if stamp-T.started>=MAX_SECONDS then return T.Stop("duration_limit")end
    -- Reserve room for a possible context+gap pair and the final stop record.
    if T.events>=MAX_EVENTS-2 then return T.Stop("event_limit")end
    local changed=b.resetSerial~=T.reset
    local gap=delta>0.15
    if changed or gap or stamp>=T.nextSample then T.last=record(state,p)end
    if changed then
        T.reset=b.resetSerial
        emit("context",T.last,"reason=reset_changed elapsed="..number(stamp-T.started).." truncated=false cached=false")
    end
    if gap then
        emit("gap",T.last,"reason=clock_gap delta="..number(delta).." elapsed="..number(stamp-T.started)
            .." truncated=false cached=false")
        T.nextSample=stamp+0.1 -- no catch-up samples after pause/focus stalls
    elseif stamp>=T.nextSample then
        if T.samples>=MAX_SAMPLES then return T.Stop("sample_limit")end
        T.samples=T.samples+1
        emit("sample",T.last,"elapsed="..number(stamp-T.started).." truncated=false cached=false")
        T.nextSample=stamp+0.1
        if T.samples>=MAX_SAMPLES then return T.Stop("sample_limit")end
    end
    return true
end
function T.Tick(state)
    if not T.active then return false end
    local ok,result=pcall(tick,state)
    if not ok then T.Stop("callback_error");return false end
    return result
end
for _,cmd in ipairs({
    {"kt_terrain_slope","KuttenbergThief.terrainProbe.Start('slope')","KT: bounded passive slope telemetry"},
    {"kt_terrain_stairs","KuttenbergThief.terrainProbe.Start('stairs')","KT: bounded passive stairs telemetry"},
    {"kt_terrain_ledge","KuttenbergThief.terrainProbe.Start('ledge')","KT: bounded passive ledge telemetry"},
    {"kt_terrain_stop","KuttenbergThief.terrainProbe.Stop('command')","KT: stop passive terrain telemetry"}})do
    call(System,"AddCCommand",false,cmd[1],cmd[2],cmd[3])
end
