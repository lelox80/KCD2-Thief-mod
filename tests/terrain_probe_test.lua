-- Passive telemetry fixtures. No retail movement/terrain acceptance claims.
local M,B,T,p,state,t,logs,commands,reads,failInvoke
local source='src/Data/Scripts/KuttenbergThief/TerrainProbe.lua'
local all='b'..string.rep('1',25)
local function setup()
    t=10;logs={};commands={};reads={pos=0,velocity=0};failInvoke=false
    _KTTerrainSessions=nil
    local function forbidden()error('forbidden gameplay writer')end
    System={AddCCommand=function(name,expression)commands[name]=expression end,
        SetCVar=forbidden,SetViewCameraFov=forbidden,GetCurrAsyncTime=forbidden}
    Script={SetTimer=forbidden};Physics={RayWorldIntersection=forbidden}
    p={id=37,GetWorldPos=function()reads.pos=reads.pos+1;return{x=1,y=2,z=3}end,
        GetVelocity=function()reads.velocity=reads.velocity+1;return{x=0,y=1,z=0}end,
        SetWorldPos=forbidden,SetVelocity=forbidden,AddImpulse=forbidden,
        actor={SimulateOnAction=forbidden},soul={AddBuff=forbidden}}
    local function member(o,k)
        if o==nil then return nil end
        local ok,v=pcall(function()return o[k]end);if ok then return v end
    end
    local function invoke(o,k,method,...)
        if failInvoke then error('private invocation failure')end
        local fn=member(o,k);if type(fn)~='function'then return nil,'missing'end
        local ok,v
        if method then ok,v=pcall(fn,o,...)else ok,v=pcall(fn,...)end
        if not ok then return nil,'error'end
        return v,v==nil and 'nil' or 'ok'
    end
    B={epoch=7,identityEpoch=3,generation=2,sequence=1,requestSerial=0,resetSerial=0,
        enabled=true,dryRun=true,owner=p,ownerId=p.id,lastStamp=t}
    state={stamp=t,dt=1/60,known=all,safe=all,reason='ok',observerSequence=1,entity='37',originalId=p.id}
    B.lastFrame=state
    M={generation=2,nativeBridge=B,Log=function(s)logs[#logs+1]='[KT] '..s end,
        access={member=member,invoke=invoke,currentPlayer=function()return p end}}
    KuttenbergThief=M;dofile(source);T=M.terrainProbe
    return T
end
local function frame(dt)
    t=t+(dt or .11);state.stamp=t;state.observerSequence=state.observerSequence+1
    B.sequence=B.sequence+1;B.lastFrame=state
    B.lastStamp=t
    return T.Tick(state)
end
local function rows(event)
    local result={}
    for _,line in ipairs(logs)do
        assert(not string.find(line,'[\r\n|]'),'single line, no delimiter injection')
        local row={raw=line}
        for key,value in string.gmatch(line,'([%w_]+)=([^ ]+)')do row[key]=value end
        if row.event==event then result[#result+1]=row end
    end
    return result
end
local function last(event)local list=rows(event);return list[#list]end
local function stopped(reason)
    assert(not T.active,'trace must stop')
    assert(last('stop').reason==reason,last('stop').raw)
end

setup();assert(not T.active and not T.Tick(state) and #logs==0 and reads.pos==0 and reads.velocity==0)
assert(commands.kt_terrain_slope and commands.kt_terrain_stairs and commands.kt_terrain_ledge and commands.kt_terrain_stop)
assert(loadstring(commands.kt_terrain_slope))();assert(T.active and #rows('start')==1)
assert(not T.Start('stairs') and _KTTerrainSessions==1)
frame(0);local r=last('sample')
for _,key in ipairs({'phase','session','seq','generation','epoch','identity','request','reset','frame','observer','entity',
    'async','dt','known','safe','gate','pos','velocity','pos_status','velocity_status','native_enabled','dry_run'})do
    assert(r[key]~=nil,key)
end
assert(r.pos=='(1.000000,2.000000,3.000000)' and r.velocity=='(0.000000,1.000000,0.000000)')
assert(r.pos_status=='ok' and r.pos_valid=='true' and r.velocity_valid=='true' and r.diagnostic=='true')
assert(r.frame=='2' and r.observer=='2' and r.known==all and r.safe==all and r.entity=='37')
assert(loadstring(commands.kt_terrain_stop))();stopped('command')
local previous=reads.pos;frame();assert(reads.pos==previous and #rows('sample')==1)
setup();assert(T.Start('slope'));frame(0)
local cachedFrame=last('sample').frame
B.sequence=B.sequence+1;B.lastStamp=t+.04
assert(T.Stop('command'))
assert(last('stop').frame==cachedFrame and tonumber(last('stop').boundary_frame)==B.sequence)
assert(last('stop').cached=='true' and last('stop').boundary_valid=='true')

-- Getter failures/shape/nonfinite components stay explicitly unknown, never a
-- fabricated zero vector and never raw engine error text.
for _,name in ipairs({'GetWorldPos','GetVelocity'})do
    for _,mode in ipairs({'missing','nil','error','shape','nan','infinite','partial','string'})do
        setup()
        if mode=='missing'then p[name]=nil
        elseif mode=='nil'then p[name]=function()return nil end
        elseif mode=='error'then p[name]=function()error('PRIVATE_SENTINEL')end
        elseif mode=='shape'then p[name]=function()return 4 end
        elseif mode=='nan'then p[name]=function()return{x=0/0,y=1,z=2}end
        elseif mode=='infinite'then p[name]=function()return{x=0,y=math.huge,z=2}end
        elseif mode=='partial'then p[name]=function()return{x=0,y=1}end
        else p[name]=function()return{x='0',y=1,z=2}end end
        assert(T.Start('stairs'));frame(0);r=last('sample')
        local key=name=='GetWorldPos' and 'pos' or 'velocity'
        assert(r[key]=='unknown' and r[key..'_valid']=='false' and r[key..'_status']~='ok',mode)
        assert(not string.find(r.raw,'PRIVATE_SENTINEL',1,true) and T.active)
    end
end
setup();assert(T.Start('ledge'));failInvoke=true;frame(0)
assert(last('sample').pos_status=='error' and T.active)
setup();state.entity='userdata: 0xDEADBEEF';state.reason='bad\nprivate message|x'
assert(T.Start('ledge'));frame(0);r=last('sample')
assert(r.entity=='unknown' and r.gate=='unknown' and not string.find(r.raw,'DEADBEEF',1,true))
if newproxy then
    setup();p.id=newproxy();B.ownerId=p.id;state.originalId=p.id;state.entity='0'
    assert(T.Start('slope'));frame(0);assert(last('sample').entity=='0')
    assert(not string.find(last('sample').raw,'userdata',1,true))
end

-- Roughly 10 Hz, never catch up after gaps. Reset records context and continues
-- so the same trace can include cancellation and subsequent recovery.
setup();assert(T.Start('slope'));frame(0);frame(.03);frame(.03);frame(.03)
assert(#rows('sample')==1);frame(.02);assert(#rows('sample')==2)
frame(.6);assert(T.active and #rows('gap')==1 and #rows('sample')==2)
assert(tonumber(last('gap').delta)>.59)
frame(.05);assert(#rows('sample')==2);frame(.06);assert(#rows('sample')==3)
B.resetSerial=B.resetSerial+1;frame(.01)
assert(T.active and #rows('context')==1 and last('context').reason=='reset_changed' and last('context').reset=='1')
assert(#rows('stop')==0)

-- Ownership/lifecycle stop before querying a stored old player handle.
local transitions={module_changed=function()KuttenbergThief={}end,
    bridge_changed=function()M.nativeBridge={}end,epoch_changed=function()B.epoch=B.epoch+1 end,
    generation_changed=function()M.generation=M.generation+1 end,
    identity_changed=function()B.identityEpoch=B.identityEpoch+1 end,
    player_changed=function()local other={};for k,v in pairs(p)do other[k]=v end;p=other end}
for reason,change in pairs(transitions)do
    setup();assert(T.Start('stairs'));local old=p;change()
    old.GetWorldPos=function()error('old handle dereferenced')end
    old.GetVelocity=old.GetWorldPos
    frame();stopped(reason)
    assert(last('stop').cached=='true')
end
setup();assert(T.Start('ledge'));state.originalId=38;frame();stopped('player_changed')
setup();assert(T.Start('ledge'));B.owner={};frame();stopped('player_changed')
setup();assert(T.Start('ledge'));state.safe='b1110'..string.rep('1',21);frame();stopped('loading')
setup();assert(T.Start('ledge'));M.hadPlayer=false;frame();stopped('gameplay_inactive')
setup();assert(T.Start('ledge'));state.stamp=t-.01;T.Tick(state);stopped('clock_rollback')
setup();assert(T.Start('ledge'));state.stamp=0/0;T.Tick(state);stopped('clock_unknown')
setup();assert(T.Start('ledge'));B.sequence=8388608;frame();stopped('counter_invalid')
setup();assert(T.Start('ledge'));T.events=nil;frame();stopped('counter_invalid')
setup();assert(T.Start('ledge'));M.access.currentPlayer=function()error('PRIVATE_CALLBACK')end
assert(not T.Tick(state));stopped('callback_error')
assert(not string.find(last('stop').raw,'PRIVATE_CALLBACK',1,true))
setup();assert(T.Start('ledge'));M.Log=function()error('logger unavailable')end
assert(frame(0) and T.active);assert(T.Stop('command'))

-- Hard limits cannot be enlarged through the public diagnostic constants.
setup();assert(T.Start('slope'));T.maxSeconds=1e9;frame(90);stopped('duration_limit')
assert(last('stop').truncated=='true')
setup();assert(T.Start('slope'));T.maxSamples=1e9
for i=1,900 do T.nextSample=t;frame(.001)end
stopped('sample_limit');assert(#rows('sample')==900 and last('stop').samples=='900')
setup();assert(T.Start('slope'))
for i=1,2100 do if not T.active then break end;B.resetSerial=B.resetSerial+1;frame(0)end
stopped('event_limit');assert(#logs<=2048)
setup()
for i=1,32 do assert(T.Start('stairs'));assert(T.Stop('command'))end
assert(not T.Start('stairs') and _KTTerrainSessions==32)
dofile(source);T=M.terrainProbe;assert(not T.Start('stairs') and _KTTerrainSessions==32)
setup();assert(not T.Start('invalid') and not T.active)
setup();_KTTerrainSessions=8388608;assert(not T.Start('stairs') and not T.active)
setup();assert(T.Start('stairs'));dofile(source);stopped('terrain_reload')
assert(not M.terrainProbe.active and _KTTerrainSessions==1)
if arg and arg[1] then
    setup();assert(T.Start('slope'));frame(0)
    p.GetVelocity=nil;frame(.11);assert(T.Stop('command'))
    local out=assert(io.open(arg[1],'wb'))
    for _,line in ipairs(logs)do out:write(line,'\n')end
    assert(out:close())
end
print('PASS: passive terrain telemetry, strict getter status, exact joins, rate/time/sample/session bounds, context/gap and ownership/error isolation; no gameplay writes')
