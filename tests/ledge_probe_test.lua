-- Analytical box geometry exercises real ray placement, rather than supplying
-- an ordered list of canned answers. No runtime body-clearance claim follows.
local source=arg and arg[1] or 'src/Data/Scripts/KuttenbergThief/LedgeProbe.lua'
local M,B,L,p,state,t,logs,commands,boxes,reads,mutateRay
local all='b'..string.rep('1',25)
local function forbidden()error('gameplay writer called')end
local function intersection(origin,delta,box)
    local near,far,normal=0,1,nil
    for _,axis in ipairs({'x','y','z'})do
        local d,o=delta[axis],origin[axis]
        if math.abs(d)<1e-8 then
            if o<box.lo[axis] or o>box.hi[axis] then return nil end
        else
            local a,b=(box.lo[axis]-o)/d,(box.hi[axis]-o)/d
            local sign=-1
            if a>b then a,b=b,a;sign=1 end
            if a>near then near=a;normal={x=0,y=0,z=0};normal[axis]=sign end
            far=math.min(far,b)
            if near>far then return nil end
        end
    end
    if near<0 or near>1 or not normal then return nil end
    local len=math.sqrt(delta.x^2+delta.y^2+delta.z^2)
    return {dist=near*len,normal=normal,pos={x=origin.x+delta.x*near,y=origin.y+delta.y*near,z=origin.z+delta.z*near}}
end
-- Convex planar geometry supports actual chamfer intersections. The synthetic
-- caps use the observed ~.57045 normal.z, but their flat tops are authored test
-- geometry, not claimed reconstructions of the retained runtime walls.
local function planarIntersection(origin,delta,planes)
    local near,far,normal=0,1,nil
    for _,plane in ipairs(planes)do
        local n,a=plane.normal,plane.anchor
        local distance=n.x*(origin.x-a.x)+n.y*(origin.y-a.y)+n.z*(origin.z-a.z)
        local speed=n.x*delta.x+n.y*delta.y+n.z*delta.z
        if math.abs(speed)<1e-8 then
            if distance>0 then return nil end
        else
            local t=-distance/speed
            if speed<0 then
                if t>near then near=t;normal=n end
            else far=math.min(far,t)end
            if near>far then return nil end
        end
    end
    if not normal or near<0 or near>1 then return nil end
    return {dist=near*math.sqrt(delta.x^2+delta.y^2+delta.z^2),normal=normal,
        pos={x=origin.x+delta.x*near,y=origin.y+delta.y*near,z=origin.z+delta.z*near}}
end
local function setup()
    t=10;logs={};commands={};reads={rays=0,pose=0};mutateRay=nil;_KTLedgeScans=nil
    boxes={{lo={x=-2,y=.6,z=0},hi={x=2,y=3,z=1.1}}}
    System={AddCCommand=function(name,expression)commands[name]=expression end,
        GetCurrAsyncTime=function()return t end,GetViewCameraDir=function()return{x=0,y=1,z=0}end,
        GetViewCameraPos=function()return{x=0,y=0,z=1.6}end,
        SetCVar=forbidden,SetViewCameraFov=forbidden}
    Script={SetTimer=forbidden};ent_all=287
    Physics={RayWorldIntersection=function(origin,delta,maxHits,mask,skip)
        assert(maxHits==1 and mask==287 and skip==p.id)
        reads.rays=reads.rays+1
        if mutateRay then return mutateRay(origin,delta)end
        local best
        for _,box in ipairs(boxes)do
            local hit=intersection(origin,delta,box)
            if hit and (not best or hit.dist<best.dist)then best=hit end
        end
        return best and {best} or {}
    end}
    p={id=37,GetWorldPos=function()reads.pose=reads.pose+1;return{x=0,y=0,z=0}end,
        GetWorldAngles=function()return{x=0,y=0,z=0}end,
        SetWorldPos=forbidden,SetVelocity=forbidden,AddImpulse=forbidden,
        actor={SimulateOnAction=forbidden,SetMovementControlledByAnimation=forbidden},soul={AddBuff=forbidden}}
    local function member(o,k)
        if o==nil then return nil end
        local ok,v=pcall(function()return o[k]end);if ok then return v end
    end
    local function invoke(o,k,method,...)
        local fn=member(o,k);if type(fn)~='function'then return nil,'missing'end
        local ok,v
        if method then ok,v=pcall(fn,o,...)else ok,v=pcall(fn,...)end
        if not ok then return nil,'error'end
        return v,v==nil and 'nil' or 'ok'
    end
    B={epoch=7,identityEpoch=3,generation=2,sequence=1,requestSerial=0,resetSerial=0,
        enabled=true,dryRun=true,owner=p,ownerId=p.id,lastStamp=t,frameGeneration=2,
        frameIdentityEpoch=3,frameOwner=p}
    state={stamp=t,dt=1/60,known=all,safe=all,reason='ok',observerSequence=1,entity='37',originalId=p.id}
    B.lastFrame=state
    M={generation=2,hadPlayer=true,nativeBridge=B,Log=function(s)logs[#logs+1]=s end,
        access={member=member,invoke=invoke,currentPlayer=function()return p end}}
    KuttenbergThief=M;dofile(source);L=M.ledgeProbe
end
local function frame(dt)
    t=t+(dt or 1/60);state.stamp=t;B.sequence=B.sequence+1;B.lastStamp=t
    return L.Tick(state)
end
local function check(reason)
    assert(L.reason==reason,'expected '..reason..', got '..tostring(L.reason))
    assert(not L.pending)
    for _,line in ipairs(logs)do
        assert(not string.find(line,'[\r\n|]'))
        assert(string.find(line,'movement=false',1,true) and string.find(line,'body_clearance=unknown',1,true))
    end
end
local function scan()assert(L.Request());return frame()end

-- Exact retained v20 hit values and all 27 grid results. Unknown subsequent
-- geometry stays unknown: this replay supplies no authored top or clearance.
local function capturedWallReplay(old,unknownTop)
    setup()
    local base={x=2896.809814,y=766.515259,z=61.381580}
    local lanes={
        {origin={x=2896.809814,y=766.515259},hits={
            {.268754,2896.844238,766.781799},
            {.291526,2896.847168,766.804382},
            {.503169,2896.874268,767.014282},
        }},
        {origin={x=2896.561768,y=766.547241},hits={
            {.255339,2896.594482,766.800476},
            {.278112,2896.597412,766.823059},
            {.490056,2896.624512,767.033264},
        }},
        {origin={x=2897.057861,y=766.483276},hits={
            {.282230,2897.093994,766.763184},
            {.305002,2897.096924,766.785767},
            {.516189,2897.123779,766.995239},
        }},
    }
    local normals={
        {x=-.074174,y=-.993429,z=.087156},
        {x=-.055665,y=-.745543,z=.664129},
        {x=-.075940,y=-.997064,z=.009843},
    }
    local heights={61.581581,61.831581,62.081581,62.381580,62.731579,63.081581,63.481579,63.981579,64.481583}
    p.GetWorldPos=function()return base end
    p.GetWorldAngles=function()return{x=0,y=0,z=-.128284}end
    System.GetViewCameraPos=function()return{x=2896.813721,y=766.626526,z=62.931530}end
    System.GetViewCameraDir=function()return{x=.126420,y=.980053,z=-.153343}end
    mutateRay=function(origin,delta)
        local index=reads.rays
        if index>27 then
            assert(index==28 and math.abs(delta.x)<.001 and math.abs(delta.y)<.001 and delta.z<0)
            assert(math.abs(origin.z-62.431580)<.002,'top scan must use upper face clear-row bracket')
            assert(origin.y>767.07,'top scan must start from the measured upper face')
            if unknownTop then error('uncaptured top remains unknown')end
            return {}
        end
        local lane=lanes[math.floor((index-1)/9)+1]
        local row=(index-1)%9+1
        assert(math.abs(origin.x-lane.origin.x)<.001 and math.abs(origin.y-lane.origin.y)<.001)
        assert(math.abs(origin.z-heights[row])<.001)
        assert(math.abs(delta.x-.204692)<.001 and math.abs(delta.y-1.586853)<.001 and delta.z==0)
        local hit=lane.hits[row]
        return hit and {{dist=hit[1],pos={x=hit[2],y=hit[3],z=heights[row]},normal=normals[row]}} or {}
    end
    assert(not scan())
    if old then
        check('face_transition_unassociated');assert(reads.rays==27 and L.last.transitionHits==1)
    else
        check(unknownTop and 'ray_unknown' or 'no_lip_sample')
        assert(reads.rays==28 and L.last.profileTransitions==1 and L.last.topTransitionHits==0)
        assert(math.abs(L.last.finalFaceReach-.503169)<.001 and L.last.profileSamples==0)
    end
    assert(L.last.height==nil and L.last.topSamples==0 and L.last.faceConnected==nil)
end
if arg and arg[2]=='captured_only'then
    capturedWallReplay(arg[3]=='v20',false)
    for _,line in ipairs(logs)do print(line)end
    if arg[3]~='v20'then
        capturedWallReplay(false,true)
        print(logs[#logs])
    end
    print('PASS: retained 27-ray capture replay; uncaptured top remains missing/unknown')
    return
end
capturedWallReplay(false,false)
capturedWallReplay(false,true)
setup();assert(commands.kt_ledge_probe and commands.kt_ledge_status)
assert(not L.Tick(state) and reads.rays==0 and reads.pose==0)
assert(loadstring(commands.kt_ledge_probe))();assert(reads.rays==0 and reads.pose==0)
assert(frame());check('candidate_hint')
assert(reads.rays==38 and math.abs(L.last.height-1.1)<.002 and math.abs(L.last.reach-.6)<.002)
assert(L.Status()=='candidate_hint')
assert(L.last.player==nil and L.last.bridge==nil and L.last.pos==nil)
t=t+.2;assert(L.Status()=='stale' and L.last==nil,'status expires even when native stops ticking')
setup();assert(scan());local candidateStamp=t
frame(.14);assert(L.last and L.Status()=='candidate_hint')
t=candidateStamp+.28
assert(L.Status()=='stale' and not L.last,'frame age and candidate age must not add two TTLs')
setup();assert(L.Request());state.stamp=t+.14;B.sequence=B.sequence+1;B.lastStamp=state.stamp;t=t+.28
assert(not L.Tick(state));check('request_expired')
setup();state.safe=string.sub(all,1,10)..'0'..string.sub(all,12)
assert(scan());check('candidate_hint') -- known standing is permitted

-- Physical obstacle, depth, width and overhead variations in the same world.
for _,case in ipairs({
    {'no_wall',function()boxes={}end},
    {'landing_sample_missing',function()boxes[1].hi.y=.85 end},
    {'landing_sample_missing',function()boxes[1].lo.x=-.2;boxes[1].hi.x=.2 end},
    {'height_outside_native_onto',function()boxes[1].hi.z=.55 end},
    {'headroom_obstructed',function()boxes[2]={lo={x=-2,y=.6,z=2.0},hi={x=2,y=3,z=2.1}}end},
})do
    setup();case[2]();assert(not scan());check(case[1]);assert(L.last and reads.rays<=48)
end
for _,height in ipairs({.7,1.45})do setup();boxes[1].hi.z=height;assert(scan());check('candidate_hint')end
setup();boxes[2]={lo={x=-2,y=.6,z=1.36},hi={x=2,y=3,z=1.38}}
assert(not scan());check('lip_inconsistent') -- thin roof just above the clear face bracket
-- Regressions for v16's concrete coverage gaps. Extended inspection never
-- expands native admission or turns a low/thin/tall surface into a climb.
setup();boxes[1].lo.y=1.4;assert(scan());check('candidate_hint')
assert(math.abs(L.last.reach-1.4)<.002 and L.last.hostOntoHeight==true)
for _,height in ipairs({.3,.46,.55,2.3,3.0})do
    setup();boxes[1].hi.z=height
    assert(not scan());check('height_outside_native_onto')
    assert(math.abs(L.last.height-height)<.002 and L.last.hostOntoHeight==false)
    assert(L.last.band==(height<.45 and 'below_native_vault' or height<.65 and 'low_vault' or 'above_native_onto'))
end
setup();boxes[1].hi.z=3.6;assert(not scan());check('face_above_inspection_range')
assert(reads.rays==27 and L.last.height==nil)
setup();boxes[1].lo.x=.15;assert(not scan());check('landing_sample_missing')
assert(L.last.faceHits>0 and L.last.height>1,'lateral faces are recorded even when centre rays miss')
setup();ent_all=797;assert(not scan());check('ray_mask');assert(reads.rays==0)

-- A rising cap must lead to a sampled flat top, then pass the full landing and
-- headroom probes. Exercises real coordinates and the retail float32 runtime.
local function chamfer(edgeHeight,capWidth,overrides)
    local base={x=3160,y=539,z=46}
    local f={x=.8,y=.6,z=0}
    local nz=.57045;local nh=math.sqrt(1-nz*nz)
    local slope=nh/nz
    local face={x=base.x+f.x*.36,y=base.y+f.y*.36,z=base.z+edgeHeight}
    local cap={x=-f.x*nh,y=-f.y*nh,z=nz}
    local top=base.z+edgeHeight+capWidth*slope
    local planes={
        {anchor=face,normal={x=-f.x,y=-f.y,z=0}},
        {anchor=face,normal=cap},
        {anchor={x=face.x,y=face.y,z=top},normal={x=0,y=0,z=1}},
        {anchor=base,normal={x=0,y=0,z=-1}},
    }
    if overrides then overrides(planes,base,face,top)end
    p.GetWorldPos=function()return base end
    System.GetViewCameraDir=function()return f end
    System.GetViewCameraPos=function()return{x=base.x,y=base.y,z=base.z+1.6}end
    boxes={}
    mutateRay=function(origin,delta)
        local hit=planarIntersection(origin,delta,planes)
        return hit and {hit} or {}
    end
    return top-base.z
end
local function leaningPlatform(height,nz,change)
    local base={x=3160,y=539,z=46}
    local f={x=.8,y=.6,z=0}
    local nh=math.sqrt(1-nz*nz)
    local face={x=base.x+f.x*.6,y=base.y+f.y*.6,z=base.z}
    local planes={
        {anchor=face,normal={x=-f.x*nh,y=-f.y*nh,z=nz}},
        {anchor={x=base.x,y=base.y,z=base.z+height},normal={x=0,y=0,z=1}},
        {anchor={x=base.x+f.x*3,y=base.y+f.y*3,z=base.z},normal=f},
        {anchor=base,normal={x=0,y=0,z=-1}},
    }
    local extra=change and change(planes,base,f)
    p.GetWorldPos=function()return base end
    System.GetViewCameraDir=function()return f end
    System.GetViewCameraPos=function()return{x=base.x,y=base.y,z=base.z+1.6}end
    boxes={}
    mutateRay=function(origin,delta)
        local best=planarIntersection(origin,delta,planes)
        if extra then
            local hit=planarIntersection(origin,delta,extra)
            if hit and (not best or hit.dist<best.dist)then best=hit end
        end
        return best and {best} or {}
    end
end

-- A union of bounded convex pieces authors a connected lower face, bevel and
-- recessed upper face. Its broad top is synthetic, never supplied by replay.
local function compoundProfile(options)
    local o=options or {}
    local base={x=2896.809814,y=766.515259,z=61.381580}
    local f={x=.8,y=.6,z=0};local right={x=f.y,y=-f.x,z=0}
    local function normal(nz)return{x=-f.x*math.sqrt(1-nz*nz),y=-f.y*math.sqrt(1-nz*nz),z=nz}end
    local lowerNormal=normal(o.lowerNZ or .087156)
    local capNormal=normal(o.capNZ or .664129)
    local upperNormal=normal(o.upperNZ or .009843)
    local lowerHeight,upperHeight=o.lowerEdge or .40,o.upperEdge or .68
    local lowerAnchor={x=base.x+f.x*.27,y=base.y+f.y*.27,z=base.z+.2}
    local lowerRecede=lowerNormal.z/-(lowerNormal.x*f.x+lowerNormal.y*f.y)
    local lowerEdge={x=lowerAnchor.x+f.x*lowerRecede*(lowerHeight-.2),
        y=lowerAnchor.y+f.y*lowerRecede*(lowerHeight-.2),z=base.z+lowerHeight}
    local capRecede=capNormal.z/-(capNormal.x*f.x+capNormal.y*f.y)
    local upperEdge={x=lowerEdge.x+f.x*capRecede*(upperHeight-lowerHeight),
        y=lowerEdge.y+f.y*capRecede*(upperHeight-lowerHeight),z=base.z+upperHeight}
    local lowerFace={anchor=lowerAnchor,normal=lowerNormal}
    local upperFace={anchor={x=upperEdge.x+f.x*(o.shiftUpper or 0),
        y=upperEdge.y+f.y*(o.shiftUpper or 0),z=upperEdge.z},normal=upperNormal}
    if o.opposingCap then capNormal={x=-capNormal.x,y=-capNormal.y,z=capNormal.z}end
    local capFace={anchor=lowerEdge,normal=capNormal}
    local function piece(front,low,high)
        local width=o.width or 2
        return{
            front,
            {anchor={x=base.x,y=base.y,z=base.z+low},normal={x=0,y=0,z=-1}},
            {anchor={x=base.x,y=base.y,z=base.z+high},normal={x=0,y=0,z=1}},
            {anchor={x=base.x+f.x*(o.depth or 3),y=base.y+f.y*(o.depth or 3),z=base.z},normal=f},
            {anchor={x=base.x+right.x*width,y=base.y+right.y*width,z=base.z},normal=right},
            {anchor={x=base.x-right.x*width,y=base.y-right.y*width,z=base.z},normal={x=-right.x,y=-right.y,z=0}},
        }
    end
    local top=o.height or 1.10
    local pieces={piece(lowerFace,0,o.lowerStop or lowerHeight)}
    if not o.missingCap then pieces[#pieces+1]=piece(capFace,o.capStart or lowerHeight,o.capStop or upperHeight)end
    local upperPiece=piece(upperFace,o.upperStart or upperHeight,top)
    pieces[#pieces+1]=upperPiece
    if o.finalCap then
        local fc=o.finalCap;local n=normal(fc.nz or .57045)
        local recede=upperNormal.z/-(upperNormal.x*f.x+upperNormal.y*f.y)
        local edge={x=upperFace.anchor.x+f.x*recede*(fc.edge-upperHeight),
            y=upperFace.anchor.y+f.y*recede*(fc.edge-upperHeight),z=base.z+fc.edge}
        top=fc.edge+fc.width*math.sqrt(1-n.z*n.z)/n.z
        upperPiece[3]={anchor={x=edge.x,y=edge.y,z=base.z+top},normal={x=0,y=0,z=1}}
        upperPiece[#upperPiece+1]={anchor=edge,normal=n}
        if fc.ridge then
            local backZ=.589722;local backXY=math.sqrt(1-backZ*backZ)
            upperPiece[3]={anchor={x=edge.x+f.x*fc.width,y=edge.y+f.y*fc.width,z=base.z+top},
                normal={x=f.x*backXY,y=f.y*backXY,z=backZ}}
        end
    end
    p.GetWorldPos=function()return base end
    System.GetViewCameraDir=function()return f end
    System.GetViewCameraPos=function()return{x=base.x,y=base.y,z=base.z+1.6}end
    boxes={}
    mutateRay=function(origin,delta)
        local best
        for _,planes in ipairs(pieces)do
            local hit=planarIntersection(origin,delta,planes)
            if hit and (not best or hit.dist<best.dist)then best=hit end
        end
        return best and {best} or {}
    end
    return {base=base,f=f,pieces=pieces,piece=piece,upperFace=upperFace,lowerFace=lowerFace,
        capFace=capFace,lowerEdge=lowerEdge,upperEdge=upperEdge,top=top}
end

for _,upperNZ in ipairs({0,.009843,.2,-.2})do
    setup();compoundProfile({upperNZ=upperNZ})
    assert(scan());check('candidate_hint')
    assert(math.abs(L.last.height-1.10)<.003 and L.last.topSamples==5 and reads.rays==44)
    assert(L.last.transitionHits==1 and L.last.topTransitionHits==0 and L.last.profileTransitions==1)
    assert(L.last.profileConnected==true and L.last.profileSamples==4 and L.last.faceConnected==true)
    assert(L.last.finalFaceReach>L.last.reach+.1,'top search rebases to a separately measured upper face')
end
setup();compoundProfile({height=2.30,upperNZ=.2})
assert(not scan());check('height_outside_native_onto')
assert(math.abs(L.last.height-2.30)<.003 and L.last.band=='above_native_onto' and reads.rays==44)
assert(L.last.profileConnected==true and L.last.faceConnected==true and L.last.topSamples==5)
for _,capZ in ipairs({.57045,.8})do
    setup();compoundProfile({capNZ=capZ,lowerNZ=-.2,upperNZ=.2,height=1.30})
    assert(scan());check('candidate_hint');assert(reads.rays==44 and L.last.profileConnected==true)
end
setup();compoundProfile({finalCap={edge=1.10,width=.20}})
assert(scan());check('candidate_hint')
assert(L.last.profileTransitions==1 and L.last.transitionHits==2 and L.last.topTransitionHits==1)
assert(L.last.profileConnected==true and L.last.faceConnected==nil and reads.rays<=46)
assert(L.last.topSamples==5 and L.last.topInset>.06)
setup();compoundProfile({finalCap={edge=1.10,width=.70}})
assert(not scan());check('height_outside_native_onto')
assert(reads.rays==46 and L.last.topSamples==5 and L.last.profileSamples==4)
assert(L.last.topInset==.70 and L.last.topTransitionHits==3 and L.last.profileConnected==true,
    'the maximum path is 27 grid + 5 top + 5 landing + 5 headroom + 4 profile rays')

-- A lower bevel does not license an unobserved cap above the final face.
setup();compoundProfile({finalCap={edge=.80,width=.10}})
assert(not scan());check('top_surface_unwalkable')
assert(L.last.transitionHits==1 and L.last.topTransitionHits==0 and L.last.height==nil and reads.rays==28)
setup();compoundProfile({finalCap={edge=1.10,width=.20,ridge=true}})
assert(not scan());check('cap_ridge_unwalkable')
assert(L.last.profileTransitions==1 and L.last.topTransitionHits==1 and L.last.height==nil)

-- Separated/gapped pieces are counterexamples, never assumed continuous from
-- intersecting planes. Four finite near-join rays only confirm those points.
for _,options in ipairs({
    {lowerStop=.37,capStart=.43},
    {capStop=.65,upperStart=.695},
})do
    setup();compoundProfile(options)
    assert(not scan());check('profile_connection_missing')
    assert(L.last.profileTransitions==1 and L.last.profileConnected==false and reads.rays<=42)
end
setup();compoundProfile({shiftUpper=.20})
assert(not scan());check('profile_join_unassociated');assert(reads.rays==27 and L.last.height==nil)
setup();compoundProfile({shiftUpper=-.10})
assert(not scan());check('profile_connection_unassociated')
assert(L.last.profileConnected==false and L.last.height and L.last.topSamples==5)
for _,options in ipairs({{missingCap=true},{opposingCap=true}})do
    setup();compoundProfile(options)
    assert(not scan());assert(L.last and L.last.profileTransitions==0)
end

-- Mandatory final-face confirmations remain active for vertical as well as
-- leaning upper faces, even after a successfully sampled lower bevel.
for _,upperNZ in ipairs({0,.2})do
    setup();local geometry=compoundProfile({upperNZ=upperNZ})
    geometry.pieces[#geometry.pieces+1]=geometry.piece(geometry.upperFace,1.25,1.30)
    assert(not scan());check('face_top_connection_missing')
    assert(L.last.profileConnected==true and L.last.faceConnected==false and reads.rays==43)
end
setup();compoundProfile({width=.20})
assert(not scan());check('landing_sample_missing');assert(L.last.profileTransitions==1)
setup();local geometry=compoundProfile()
geometry.pieces[#geometry.pieces+1]=geometry.piece(geometry.upperFace,1.60,1.62)
assert(not scan());check('headroom_obstructed');assert(L.last.topSamples==5)

-- A second compound join must fail closed before a second four-ray group can
-- accumulate; the unchanged hard 48-ray cap is not a partial-success policy.
setup();local geometry=compoundProfile({finalCap={edge=1.10,width=.20}})
local originalRay=mutateRay
mutateRay=function(origin,delta)
    if delta.z==0 and math.abs(origin.z-(geometry.base.z+1.70))<.002 then
        local face={anchor={x=geometry.base.x+geometry.f.x*1.1,y=geometry.base.y+geometry.f.y*1.1,
            z=geometry.base.z+1.70},normal={x=-geometry.f.x,y=-geometry.f.y,z=0}}
        local planes=geometry.piece(face,1.50,1.80)
        local hit=planarIntersection(origin,delta,planes)
        return hit and {hit} or {}
    end
    return originalRay(origin,delta)
end
assert(not scan());check('profile_transition_limit');assert(reads.rays==27 and L.last.height==nil)
for _,which in ipairs({38,39,40,41,42,43,44})do
    setup();compoundProfile()
    local geometryRay=mutateRay
    mutateRay=function(origin,delta)
        if reads.rays==which then error('mandatory sample unavailable')end
        return geometryRay(origin,delta)
    end
    assert(not scan());check('ray_unknown');assert(reads.rays==which)
end
for _,after in ipairs({37,41,43})do
    setup();compoundProfile()
    local geometryRay=mutateRay
    mutateRay=function(origin,delta)
        local hits=geometryRay(origin,delta)
        if reads.rays==after then L.rays=48 end
        return hits
    end
    assert(not scan());check('ray_budget')
    assert(reads.rays==after and L.last.rays==48 and L.reason~='candidate_hint',
        'budget exhaustion cannot skip a mandatory profile/final-face sample')
end

-- The allowed near-vertical normal may have nonzero z. Group its hits on the
-- full 3D plane, and inspect above its measured high face rather than its low
-- front. Neither direction of lean changes the flat/footprint/height gates.
for _,nz in ipairs({0,.2,.25,-.2,-.25})do
    for _,height in ipairs({.70,1.10,1.30,1.49})do
        setup();leaningPlatform(height,nz)
        assert(scan());check('candidate_hint')
        assert(math.abs(L.last.height-height)<.003 and L.last.topSamples==5 and reads.rays==(nz==0 and 38 or 40))
        assert(L.last.faceProjection and L.last.faceProjection>=0)
        assert((nz>0 and L.last.faceProjection>0)or(nz<=0 and L.last.faceProjection==0))
        assert((nz==0 and L.last.faceConnected==nil)or(nz~=0 and L.last.faceConnected==true))
    end
    setup();leaningPlatform(2.30,nz,function(planes,base,f)
        planes[1].anchor={x=base.x+f.x*.85,y=base.y+f.y*.85,z=base.z}
    end)
    assert(not scan());check('height_outside_native_onto')
    assert(math.abs(L.last.height-2.30)<.003 and L.last.topSamples==5 and reads.rays==(nz==0 and 38 or 40))
end
-- The ray contract permits slightly non-unit normals. The new strict face
-- comparison measures an angle, rather than treating magnitude as curvature.
for _,scale in ipairs({.98,1.02})do
    setup();leaningPlatform(1.10,.2)
    local originalRay=mutateRay
    mutateRay=function(origin,delta)
        local hits=originalRay(origin,delta)
        for _,hit in ipairs(hits)do
            local n=hit.normal
            hit.normal={x=n.x*scale,y=n.y*scale,z=n.z*scale}
        end
        return hits
    end
    assert(scan());check('candidate_hint')
    assert(L.last.faceConnected==true and reads.rays==40)
end
setup();leaningPlatform(1.3,.2,function(planes,base,f)
    planes[1].anchor={x=base.x+f.x*1.43,y=base.y+f.y*1.43,z=base.z}
end)
assert(not scan());check('face_projection_outside_reach')
assert(reads.rays==27 and L.last.height==nil,'a ray-range miss does not license extrapolation beyond its segment')
setup();leaningPlatform(.60,.2,function(planes,base,f)
    local nz=.2;local nh=math.sqrt(1-nz*nz)
    return{
        {anchor={x=base.x+f.x*.75,y=base.y+f.y*.75,z=base.z},normal={x=-f.x*nh,y=-f.y*nh,z=nz}},
        {anchor={x=base.x,y=base.y,z=base.z+1.1},normal={x=0,y=0,z=1}},
        {anchor={x=base.x+f.x*3,y=base.y+f.y*3,z=base.z},normal=f},
        {anchor={x=base.x,y=base.y,z=base.z+.60},normal={x=0,y=0,z=-1}},
    }
end)
assert(not scan());check('face_transition_unassociated')
assert(reads.rays==27 and L.last.height==nil,'parallel shifted faces are not one continuous facade')
setup();leaningPlatform(1.10,.2,function(planes,base,f)
    return{
        {anchor={x=base.x+f.x*.6,y=base.y+f.y*.6,z=base.z},normal={x=-f.x,y=-f.y,z=0}},
        {anchor={x=base.x,y=base.y,z=base.z+1.6},normal={x=0,y=0,z=1}},
        {anchor={x=base.x+f.x*3,y=base.y+f.y*3,z=base.z},normal=f},
        {anchor={x=base.x,y=base.y,z=base.z+1.5},normal={x=0,y=0,z=-1}},
    }
end)
assert(not scan());check('headroom_obstructed')
assert(L.last.topSamples==5 and L.last.faceProjection>0,'projected top remains subject to the overhang check')
for _,shelf in ipairs({{.90,1.23,1.25,'face_top_connection_missing'},
    {.91,1.25,1.30,'face_top_connection_missing'},
    {.91,1.11,1.30,'face_top_connection_unassociated'}})do
    setup();leaningPlatform(1.10,.2,function(planes,base,f)
        return{
            {anchor={x=base.x+f.x*shelf[1],y=base.y+f.y*shelf[1],z=base.z},normal={x=-f.x,y=-f.y,z=0}},
            {anchor={x=base.x,y=base.y,z=base.z+shelf[3]},normal={x=0,y=0,z=1}},
            {anchor={x=base.x+f.x*3,y=base.y+f.y*3,z=base.z},normal=f},
            {anchor={x=base.x,y=base.y,z=base.z+shelf[2]},normal={x=0,y=0,z=-1}},
        }
    end)
    assert(not scan());check(shelf[4])
    assert(L.last.topSamples==5 and L.last.faceConnected==false and reads.rays==39,
        'a separate shelf within the coarse face-height bracket cannot complete a projected candidate')
end
setup();leaningPlatform(1.10,.2,function(planes,base,f)
    local nz=.2;local nh=math.sqrt(1-nz*nz)
    return{
        {anchor={x=base.x+f.x*.6,y=base.y+f.y*.6,z=base.z},normal={x=-f.x*nh,y=-f.y*nh,z=nz}},
        {anchor={x=base.x,y=base.y,z=base.z+1.30},normal={x=0,y=0,z=1}},
        {anchor={x=base.x+f.x*3,y=base.y+f.y*3,z=base.z},normal=f},
        {anchor={x=base.x,y=base.y,z=base.z+1.25},normal={x=0,y=0,z=-1}},
    }
end)
assert(not scan());check('face_top_connection_missing')
assert(L.last.topSamples==5 and L.last.faceConnected==false and reads.rays==39,
    'a coplanar disconnected shelf must fail the midpoint sample even when its near-lip face matches')
for _,nz in ipairs({.2,-.2})do
    setup()
    local edgeHeight,capWidth=1.10,.20
    local capZ=.57045;local capXY=math.sqrt(1-capZ*capZ)
    local top=edgeHeight+capWidth*capXY/capZ
    leaningPlatform(top,nz,function(planes,base,f)
        local recede=nz/math.sqrt(1-nz*nz)
        local edge={x=base.x+f.x*(.6+recede*edgeHeight),y=base.y+f.y*(.6+recede*edgeHeight),z=base.z+edgeHeight}
        planes[#planes+1]={anchor=edge,normal={x=-f.x*capXY,y=-f.y*capXY,z=capZ}}
    end)
    assert(scan());check('candidate_hint')
    assert(math.abs(L.last.height-top)<.003 and L.last.transitionHits>0 and L.last.topSamples==5)
    assert(L.last.faceConnected==nil and reads.rays<=42,'a real chamfer is not required to stay vertical up to its lip')
end

setup();local height=chamfer(.90,.20)
assert(scan());check('candidate_hint')
assert(L.schema==3 and L.last.transitionHits==1 and L.last.topInset>.06)
assert(math.abs(L.last.height-height)<.003 and L.last.transitionHeight<L.last.height)
assert(L.last.topSamples==5 and reads.rays==39 and reads.rays<=48)
for _,case in ipairs({{1.40,.42},{1.98,.35}})do
    setup();local high=chamfer(case[1],case[2])
    assert(not scan());check('height_outside_native_onto')
    assert(L.last.transitionHits>0 and L.last.topInset>.06)
    assert(math.abs(L.last.height-high)<.003 and L.last.band=='above_native_onto')
    assert(L.last.topSamples==5 and reads.rays<=42)
end
-- A roof-like cap turns downward between the second and third top samples.
-- Both steep planes are analytical test geometry inspired by observed v18
-- normals. Their intersection is not a flat top or an admission target.
setup();chamfer(1.43232,1.0,function(planes,base,face)
    local frontSlope=math.sqrt(1-.57045^2)/.57045
    local backZ=.589722;local backXY=math.sqrt(1-backZ^2)
    local ridgeInset=.37844
    planes[3]={anchor={x=face.x+.8*ridgeInset,y=face.y+.6*ridgeInset,
        z=base.z+1.43232+frontSlope*ridgeInset},normal={x=.8*backXY,y=.6*backXY,z=backZ}}
end)
assert(not scan());check('cap_ridge_unwalkable')
assert(reads.rays==30 and L.last.transitionHits==1 and L.last.transitionHeight>1.7)
assert(L.last.height==nil and L.last.topInset==nil and L.last.topSamples==0,
    'a sampled ridge is not a measured walkable ledge')
setup();chamfer(.90,1.0)
assert(not scan());check('no_walkable_top')
assert(L.last.height==nil and L.last.transitionHeight and L.last.topSamples==0 and reads.rays==32)
setup();chamfer(.90,.20)
local capRay=mutateRay
local roof={lo={x=3160.42,y=539.30,z=47.44},hi={x=3163,y=542,z=47.45}}
mutateRay=function(origin,delta)
    local top=intersection(origin,delta,roof)
    local hits=capRay(origin,delta)
    if top and (not hits[1] or top.dist<hits[1].dist)then return{top}end
    return hits
end
assert(not scan());check('headroom_obstructed')
assert(L.last.height and L.last.topSamples==5,'a flat top still requires clear headroom')
setup();chamfer(.90,1.0)
local longCapRay=mutateRay
local floatingRoof={lo={x=3160.42,y=539.30,z=47.39},hi={x=3163,y=542,z=47.40}}
mutateRay=function(origin,delta)
    local top=intersection(origin,delta,floatingRoof)
    local hits=longCapRay(origin,delta)
    if top and (not hits[1] or top.dist<hits[1].dist)then return{top}end
    return hits
end
assert(not scan());check('top_transition_disconnected')
assert(L.last.height==nil and L.last.topSamples==0,'a disconnected roof cannot complete a cap search')
setup()
local nz=.57045;local nh=math.sqrt(1-nz*nz)
local remoteCap={
    {anchor={x=0,y=1.2,z=1},normal={x=0,y=-1,z=0}},
    {anchor={x=0,y=1.2,z=1},normal={x=0,y=-nh,z=nz}},
    {anchor={x=0,y=1.2,z=2},normal={x=0,y=0,z=1}},
    {anchor={x=0,y=1.2,z=0},normal={x=0,y=0,z=-1}},
}
mutateRay=function(origin,delta)
    local front=intersection(origin,delta,boxes[1])
    local remote=planarIntersection(origin,delta,remoteCap)
    local best=front
    if remote and (not best or remote.dist<best.dist)then best=remote end
    return best and {best} or {}
end
assert(not scan());check('face_transition_unassociated')
assert(L.last.height==nil and reads.rays==27,'a farther slope cannot bridge a separate front wall')

-- The native baseline can cross a narrow object whose flat top cannot support
-- the requested climb-onto footprint. Finding its lip is useful evidence;
-- a missing landing centre must still refuse the candidate.
setup();boxes={{lo={x=3167,y=518.36,z=43.66},hi={x=3171,y=518.50,z=43.74}}}
p.GetWorldPos=function()return{x=3169,y=518,z=43}end
System.GetViewCameraPos=function()return{x=3169,y=518,z=44.6}end
assert(not scan());check('landing_sample_missing')
assert(reads.rays==29 and L.last.faceHits==3 and L.last.topSamples==0)
assert(math.abs(L.last.height-.740)<.003 and L.last.hostOntoHeight==true)

-- Thin horizontal geometry between face rows is discovered without promoting
-- an unverified vertical face into a candidate. Empty captures stay empty.
setup();boxes={};assert(not scan());check('no_wall');assert(reads.rays==42)
setup();boxes={{lo={x=-1,y=.35,z=.755},hi={x=1,y=.45,z=.78}}}
assert(not scan());check('surface_without_verified_face')
assert(L.last.faceHits==0 and L.last.seedHits>0 and L.last.height==nil and reads.rays==42)
setup();boxes={};System.GetViewCameraPos=function()return nil end
assert(not scan());check('no_wall');assert(reads.rays==39)
setup();boxes={};System.GetViewCameraPos=function()return{x=1000,y=1000,z=1000}end
assert(not scan());check('no_wall');assert(reads.rays==39,'far camera cannot produce unbounded discovery origins')
setup();boxes={};System.GetViewCameraDir=function()return{x=0,y=100,z=-50}end
local sawPitched=0
mutateRay=function(origin,delta)
    if delta.y~=0 and delta.z~=0 then
        sawPitched=sawPitched+1;assert(math.abs(math.sqrt(delta.x^2+delta.y^2+delta.z^2)-1.6)<.003)
    end
    return {}
end
assert(not scan());check('no_wall');assert(sawPitched==3 and reads.rays==42)
setup();boxes={};mutateRay=function()if reads.rays>27 then error('unknown discovery ray')end;return {}end
assert(not scan());check('ray_unknown');assert(L.last and L.last.height==nil and reads.rays==28)

setup()
mutateRay=function(origin,delta)
    -- A near sloped decoration must not mask the farther valid vertical face.
    if delta.y>0 and math.abs(origin.z-.2)<.01 and math.abs(origin.x)<.01 then
        return{{dist=.2,pos={x=0,y=.2,z=origin.z},normal={x=0,y=-.6,z=.8}}}
    end
    local hit=intersection(origin,delta,boxes[1]);return hit and {hit} or {}
end
assert(scan());check('candidate_hint');assert(math.abs(L.last.reach-.6)<.002)
local poseRows,rayRows,summaryRows=0,0,0
for _,line in ipairs(logs)do
    if string.find(line,'event=pose',1,true)then
        poseRows=poseRows+1;assert(string.find(line,'position=(0.000000,0.000000,0.000000)',1,true))
        assert(string.find(line,'camera=(0.000000,0.000000,1.600000)',1,true))
    elseif string.find(line,'event=ray',1,true)then
        rayRows=rayRows+1
        for _,key in ipairs({'scan=1','epoch=7','generation=2','identity=3','reset=0','frame=2','observer=1','async=','dt=','label=','origin=','delta=','mask=287','result='})do
            assert(string.find(line,key,1,true),key)
        end
    elseif string.find(line,'event=summary',1,true)then summaryRows=summaryRows+1 end
end
assert(poseRows==1 and rayRows==reads.rays and summaryRows==1 and #logs<=54)
setup();System.GetViewCameraDir=function()return{x=0,y=0,z=-1}end
assert(not scan());check('look_vertical');assert(reads.rays==0)
setup();ent_all=nil;assert(not scan());check('ray_mask')

-- Getter/ray failures remain unknown; no fallback invents a clear path.
for _,variant in ipairs({'missing','error','nil','number','bad_normal','outside','nonfinite','partial','sparse'})do
    setup()
    if variant=='missing'then Physics.RayWorldIntersection=nil
    elseif variant=='error'then mutateRay=function()error('PRIVATE_SENTINEL')end
    elseif variant=='nil'then mutateRay=function()return nil end
    elseif variant=='number'then mutateRay=function()return 0 end
    elseif variant=='sparse'then mutateRay=function()return{[2]={}}end
    else
        mutateRay=function(origin,delta)
            local hit=intersection(origin,delta,boxes[1])
            if not hit then return {}end
            if variant=='bad_normal'then hit.normal.z=3
            elseif variant=='outside'then hit.pos.x=hit.pos.x+1
            elseif variant=='nonfinite'then hit.dist=0/0
            elseif variant=='partial'then hit.pos.z=nil end
            return {hit}
        end
    end
    assert(not scan() and L.reason~='candidate_hint',variant)
    for _,line in ipairs(logs)do assert(not string.find(line,'PRIVATE_SENTINEL',1,true))end
end
for _,getter in ipairs({'GetWorldPos','GetViewCameraDir'})do
    setup()
    if getter=='GetWorldPos'then p.GetWorldPos=function()return{x=0/0,y=0,z=0}end
    else System.GetViewCameraDir=nil end
    assert(not scan());check('pose_unknown')
end

-- Every non-stance gate remains mandatory and typed under float32 too.
for i=1,25 do
    setup();state.known=string.sub(all,1,i)..'0'..string.sub(all,i+2)
    assert(not L.Request() and reads.rays==0)
    if i~=10 then
        setup();state.safe=string.sub(all,1,i)..'0'..string.sub(all,i+2)
        assert(not L.Request() and reads.rays==0)
    end
end
setup();B.dryRun=false;assert(not L.Request());check('movement_active')
setup();B.armed={};assert(not L.Request());check('movement_active')
setup();t=t+.3;assert(not L.Request());check('stale_frame')
setup();assert(L.Request());assert(not L.Tick(state));check('request_expired')
setup();assert(L.Request());assert(not frame(.2));check('request_expired')
for _,change in ipairs({
    function()M.generation=M.generation+1 end,
    function()B.identityEpoch=B.identityEpoch+1 end,
    function()B.epoch=B.epoch+1 end,
    function()B.resetSerial=B.resetSerial+1 end,
    function()KuttenbergThief={}end,
    function()M.nativeBridge={}end,
    function()p={}end,
    function()M.hadPlayer=false end,
})do
    setup();assert(L.Request());change();assert(not frame() and reads.rays==0 and not L.last)
end
setup();assert(L.Request());L.Stop('test');assert(not frame() and reads.rays==0)
setup();assert(L.Request());dofile(source);L=M.ledgeProbe;assert(not frame() and reads.rays==0)
setup()
for i=1,32 do assert(scan())end
assert(_KTLedgeScans==32 and not L.Request());check('scan_limit')
dofile(source);L=M.ledgeProbe;assert(not L.Request());check('scan_limit')
setup();assert(L.Request());_KTLedgeScans=0/0;assert(not frame());check('scan_limit')
setup();assert(L.Request());mutateRay=function()M.generation=3;return {}end
assert(not frame() and not L.last)
print('PASS: ledge analytical geometry, typed unknowns, standing/crouch, canonical queue, lifecycle, expiry, finite work and zero gameplay writes')
