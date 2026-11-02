-- First climb-onto milestone: bounded read-only geometry hints. Rays do not
-- prove capsule clearance, host ledge registration, or safe traversal.
local M=KuttenbergThief
local A=M.access
local get,call=A.member,A.invoke
if M.ledgeProbe and type(M.ledgeProbe.Stop)=="function" then M.ledgeProbe.Stop("reload") end
local L={schema=3}
M.ledgeProbe=L
local MAX_SCANS,MAX_RAYS,TTL=32,48,0.15
-- Inspect beyond the native action range without changing that range. Retail
-- VaultOver starts at .45; wide VaultOnto remains .65..1.5. Sampling dimensions
-- are diagnostic choices, NOT recovered capsule dimensions or admission rules.
local MIN_ONTO,MAX_ONTO,RADIUS,DEPTH,BODY_HEIGHT=0.65,1.5,0.32,0.38,1.8
local REACH,MASK=1.6,287
local FACE_HEIGHTS={0.2,0.45,0.7,1.0,1.35,1.7,2.1,2.6,3.1}
local TOP_INSETS={0.06,0.22,0.38,0.54,0.70}
local ALL="b"..string.rep("1",25)
local function finite(v)return type(v)=="number" and v==v and math.abs(v)<1e7 end
local function counter(v)return finite(v) and v>=0 and v<=8388607 and v==math.floor(v)end
local function vec(v)
    if type(v)~="table" then return nil end
    local x,y,z=get(v,"x"),get(v,"y"),get(v,"z")
    if finite(x) and finite(y) and finite(z) then return {x=x,y=y,z=z} end
end
local function add(a,b,s)return{x=a.x+b.x*s,y=a.y+b.y*s,z=a.z+b.z*s}end
local function length(v)return math.sqrt(v.x*v.x+v.y*v.y+v.z*v.z)end
local function num(v)return finite(v) and string.format("%.6f",v) or "unknown"end
local function count(v)return counter(v) and string.format("%.0f",v) or "unknown"end
local function vector(v)return v and "("..num(v.x)..","..num(v.y)..","..num(v.z)..")" or "unknown"end
local function boolean(v)return type(v)=="boolean" and tostring(v) or "unknown"end
local function emit(event,c,fields)
    local parts={"ledge v=3 event="..event,"diagnostic=true movement=false body_clearance=unknown host_ledge=unknown native_admission=not_requested"}
    if c then
        parts[#parts+1]="scan="..count(c.scan).." epoch="..count(c.epoch).." generation="..count(c.generation)
            .." identity="..count(c.identity).." reset="..count(c.reset).." frame="..count(c.sequence)
            .." observer="..count(c.observer).." async="..num(c.stamp).." dt="..num(c.dt)
    end
    if fields then parts[#parts+1]=fields end
    pcall(M.Log,table.concat(parts," "))
end
local function log(reason,result)
    local fields="result="..reason
    if result then fields=fields.." rays="..count(result.rays).." height="..num(result.height)
        .." reach="..num(result.reach).." face_hits="..count(result.faceHits)
        .." top_samples="..count(result.topSamples).." height_band="..(result.band or "unknown")
        .." host_onto_height="..boolean(result.hostOntoHeight).." transition_hits="..count(result.transitionHits)
        .." transition_height="..num(result.transitionHeight).." top_inset="..num(result.topInset)
        .." face_projection="..num(result.faceProjection)
        .." face_connection="..boolean(result.faceConnected)
        .." profile_transitions="..count(result.profileTransitions).." profile_connection="..boolean(result.profileConnected)
        .." profile_samples="..count(result.profileSamples).." top_transition_hits="..count(result.topTransitionHits)
        .." final_face_reach="..num(result.finalFaceReach)
        .." pitched_hits="..count(result.pitchedHits).." top_seed_hits="..count(result.seedHits) end
    emit(result and "summary" or "control",result,fields)
end
local function clear(reason,result)
    L.pending=nil;L.last=nil;L.reason=reason;L.rays=nil
    if result then
        -- Whitelisted scalar context only: no player, hit, target or pointer.
        L.last=result
    end
    log(reason,result)
    return reason=="candidate_hint"
end
function L.Stop(reason)
    L.pending=nil;L.last=nil;L.rays=nil;L.reason=reason or "stopped"
end
local function context(state)
    local b=M.nativeBridge
    if KuttenbergThief~=M or type(b)~="table" or state~=b.lastFrame then return nil,"canonical_frame" end
    if not counter(M.generation) or M.generation==0 or M.hadPlayer~=true or b.generation~=M.generation
        or not counter(b.epoch) or b.epoch==0 or not counter(b.identityEpoch) or b.identityEpoch==0
        or not counter(b.sequence) or not counter(b.resetSerial)
        or b.frameGeneration~=M.generation or b.frameIdentityEpoch~=b.identityEpoch then return nil,"context" end
    local p=A.currentPlayer()
    local id=get(p,"id")
    if not p or id==nil or b.owner~=p or b.frameOwner~=p or b.ownerId~=id
        or get(state,"originalId")~=id then return nil,"player" end
    if get(state,"known")~=ALL then return nil,"unknown_gate" end
    local safe=get(state,"safe")
    -- Crouch is known but may be false: this feature also starts standing.
    if type(safe)~="string" or #safe~=26 or string.sub(safe,1,10)~=string.sub(ALL,1,10)
        or not string.match(string.sub(safe,11,11),"^[01]$")
        or string.sub(safe,12)~=string.sub(ALL,12) then return nil,"unsafe_gate" end
    if b.dryRun~=true or b.armed~=nil then return nil,"movement_active" end
    local stamp,dt=get(state,"stamp"),get(state,"dt")
    local now,ns=call(System,"GetCurrAsyncTime",false)
    if not finite(stamp) or stamp<0 or b.lastStamp~=stamp or not finite(dt) or dt<=0 or dt>TTL then return nil,"clock" end
    if ns~="ok" or not finite(now) or now<stamp or now-stamp>TTL then return nil,"stale_frame" end
    return {bridge=b,player=p,id=id,stamp=stamp,now=now,epoch=b.epoch,identity=b.identityEpoch,
        generation=M.generation,reset=b.resetSerial,sequence=b.sequence,observer=get(state,"observerSequence"),dt=dt}
end
local function same(c,q)
    return c.bridge==q.bridge and c.player==q.player and c.id==q.id and c.epoch==q.epoch
        and c.identity==q.identity and c.generation==q.generation and c.reset==q.reset
end
local function fresh(c,q)
    return same(c,q) and c.sequence>=q.sequence and c.stamp>=q.stamp
        and c.now>=q.stamp and c.now-q.stamp<=TTL
end
function L.Request()
    L.last=nil;L.pending=nil
    local b=M.nativeBridge
    local ok,c,reason=pcall(context,get(b,"lastFrame"))
    if not ok or not c then return clear(reason or "callback_error") end
    if _KTLedgeScans==nil then _KTLedgeScans=0 end
    if not counter(_KTLedgeScans) or _KTLedgeScans>=MAX_SCANS then return clear("scan_limit") end
    L.pending=c;L.reason="pending"
    log("pending")
    return true
end
local function ray(c,label,origin,delta)
    if L.rays>=MAX_RAYS then return nil,"ray_budget" end
    L.rays=L.rays+1
    local hits,status=call(Physics,"RayWorldIntersection",false,origin,delta,1,ent_all,c.id)
    local function record(result,hit)
        emit("ray",c,"ray="..count(L.rays).." label="..label.." result="..result.." origin="..vector(origin)
            .." delta="..vector(delta).." mask="..count(ent_all).." distance="..num(hit and hit.dist)
            .." pos="..vector(hit and hit.pos).." normal="..vector(hit and hit.normal))
    end
    if status~="ok" or type(hits)~="table" then record("unknown");return nil,"ray_unknown" end
    local first=get(hits,1)
    if first==nil then
        if next(hits)==nil then record("miss");return false end
        record("shape");return nil,"ray_shape"
    end
    local p,n,d=vec(get(first,"pos")),vec(get(first,"normal")),get(first,"dist")
    local len=length(delta)
    if not p or not n or not finite(d) or d<0 or d>len+0.04 or math.abs(length(n)-1)>0.05 then record("shape");return nil,"ray_shape" end
    local expected=add(origin,delta,d/len)
    if length({x=p.x-expected.x,y=p.y-expected.y,z=p.z-expected.z})>0.04 then record("segment");return nil,"ray_segment" end
    local hit={pos=p,normal=n,dist=d};record("hit",hit);return hit
end
local function scan(c)
    local data={scan=c.scan,epoch=c.epoch,generation=c.generation,identity=c.identity,reset=c.reset,
        sequence=c.sequence,observer=c.observer,stamp=c.stamp,dt=c.dt,faceHits=0,topSamples=0,
        transitionHits=0,topTransitionHits=0,profileTransitions=0,profileSamples=0,pitchedHits=0,seedHits=0}
    local function finish(reason)
        data.rays=L.rays
        local current,why=context(c.bridge.lastFrame)
        if not current or not same(current,c) or current.sequence~=c.sequence then return clear(why or "context_changed") end
        return clear(reason,data)
    end
    if ent_all~=MASK then return finish("ray_mask") end
    local position,ps=call(c.player,"GetWorldPos",true)
    local direction,ds=call(System,"GetViewCameraDir",false)
    local camera=vec(call(System,"GetViewCameraPos",false))
    local angles=vec(call(c.player,"GetWorldAngles",true))
    local p,f=vec(position),vec(direction)
    emit("pose",c,"position="..vector(p).." camera="..vector(camera).." camera_direction="..vector(f)
        .." world_angles="..vector(angles).." reach_limit="..num(REACH).." inspection_height_limit=3.100000")
    if ps~="ok" or ds~="ok" or not p or not f then return finish("pose_unknown") end
    local horizontal=math.sqrt(f.x*f.x+f.y*f.y)
    if horizontal<0.2 then return finish("look_vertical") end
    local viewLength=length(f)
    local view={x=f.x/viewLength,y=f.y/viewLength,z=f.z/viewLength}
    f={x=f.x/horizontal,y=f.y/horizontal,z=0}
    local right={x=f.y,y=-f.x,z=0}
    emit("basis",c,"forward="..vector(f).." right="..vector(right))
    local wall,hits,surfaces,anyHit=nil,{},{},false
    for lane,offset in ipairs({0,-0.25,0.25})do
        surfaces[lane]={}
        for row,height in ipairs(FACE_HEIGHTS)do
            local origin=add({x=p.x,y=p.y,z=p.z+height},right,offset)
            local hit,why=ray(c,"face_"..lane.."_"..row,origin,add({x=0,y=0,z=0},f,REACH))
            if hit==nil then return finish(why) end
            surfaces[lane][row]=hit
            if hit then
                anyHit=true
                if math.abs(hit.normal.z)<=0.25 and -(hit.normal.x*f.x+hit.normal.y*f.y)>=0.75 then
                    hit.row=row;hit.lane=lane;hits[#hits+1]=hit;data.faceHits=data.faceHits+1
                    if not wall or hit.dist<wall.dist-0.02 then wall=hit end
                end
            end
        end
    end
    if not wall then
        -- A sparse horizontal grid can miss a thin rail or horizontal-only
        -- surface. These bounded discovery rays expose that coverage gap;
        -- they never manufacture a face or a climb candidate from a top hit.
        local cameraNear=camera and length({x=camera.x-p.x,y=camera.y-p.y,z=camera.z-p.z})<=2.5
        emit("discovery",c,"camera_rays="..boolean(cameraNear==true).." top_seed_rays=12 verified_face=false")
        if cameraNear then
            for lane,offset in ipairs({0,-0.25,0.25})do
                local origin=add(camera,right,offset)
                local hit,why=ray(c,"pitched_"..lane,origin,add({x=0,y=0,z=0},view,REACH))
                if hit==nil then return finish(why) end
                if hit then data.pitchedHits=data.pitchedHits+1 end
            end
        end
        for depth,distance in ipairs({0.4,0.8,1.2,1.6})do
            for lane,offset in ipairs({0,-0.25,0.25})do
                local point=add(add(p,f,distance),right,offset)
                local hit,why=ray(c,"top_seed_"..depth.."_"..lane,
                    {x=point.x,y=point.y,z=p.z+3.15},{x=0,y=0,z=-3.05})
                if hit==nil then return finish(why) end
                if hit then data.seedHits=data.seedHits+1 end
            end
        end
        if data.pitchedHits>0 or data.seedHits>0 then return finish("surface_without_verified_face") end
        return finish(anyHit and "front_surface_not_wall" or "no_wall")
    end
    data.reach=wall.dist
    if wall.dist<0.08 then return finish("wall_too_close") end
    local function planeRelation(hit,face)
        local plane=(hit.pos.x-face.pos.x)*face.normal.x+(hit.pos.y-face.pos.y)*face.normal.y
            +(hit.pos.z-face.pos.z)*face.normal.z
        local alignment=hit.normal.x*face.normal.x+hit.normal.y*face.normal.y+hit.normal.z*face.normal.z
        return plane,alignment
    end
    local function strictPlane(hit,face)
        local plane,alignment=planeRelation(hit,face)
        local magnitude=length(face.normal)
        return math.abs(plane/magnitude)<=0.02 and alignment/(magnitude*length(hit.normal))>0.995
    end
    local function bandEnd(face,start)
        -- Find one connected sampled band in this lane. Offset facades are
        -- never grouped, including after an observed bevel-to-face join.
        local rows={}
        for _,hit in ipairs(hits)do
            local plane,alignment=planeRelation(hit,face)
            if hit.lane==face.lane and math.abs(plane)<=0.08 and alignment>0.95 then rows[hit.row]=true end
        end
        local row=start
        while row<#FACE_HEIGHTS and not rows[row]do row=row+1 end
        while row<#FACE_HEIGHTS and rows[row+1]do row=row+1 end
        return row
    end
    local function faceBasis(face)
        local n=face.normal
        local horizontal=math.sqrt(n.x*n.x+n.y*n.y)
        return {x=-n.x/horizontal,y=-n.y/horizontal,z=0},n.z/-(n.x*f.x+n.y*f.y)
    end
    local function edgeAt(face,cap)
        -- Intersect the two measured 3D planes in their original ray lane.
        -- The face may lean; a non-rising/parallel cap never defines a join.
        local _,recede=faceBasis(face)
        local n=cap.normal
        local rate=n.z+(n.x*f.x+n.y*f.y)*recede
        if not finite(rate) or rate<=0.0001 then return nil end
        local height=face.pos.z+(n.x*(cap.pos.x-face.pos.x)+n.y*(cap.pos.y-face.pos.y)
            +n.z*(cap.pos.z-face.pos.z))/rate
        local point=add(face.pos,f,recede*(height-face.pos.z));point.z=height
        return point
    end
    local row=bandEnd(wall,1)
    if row==#FACE_HEIGHTS then return finish("face_above_inspection_range") end
    local inward,faceRecede=faceBasis(wall)
    local upper=row+1
    local capFirst,capEdge,capHits
    local profileChecks={}
    -- Retail wall samples include a rising chamfer with normal.z~.57. That
    -- hit is a transition, not an empty row and not a walkable top. A bounded
    -- compound profile may join one such cap to a recessed upper face. The
    -- plane intersections and later point confirmations are not body sweeps.
    while upper<=#FACE_HEIGHTS and surfaces[wall.lane][upper]do
        local hit=surfaces[wall.lane][upper]
        local n=hit.normal
        if capFirst and math.abs(n.z)<=0.25 and -(n.x*f.x+n.y*f.y)>=0.75 then
            -- At most one compound join: four later confirmation rays keep
            -- every path below the unchanged 48-ray hard limit.
            if data.profileTransitions~=0 then return finish("profile_transition_limit") end
            local join=edgeAt(hit,capFirst)
            local lastCap=capHits[#capHits]
            local anchor=surfaces[wall.lane][row].pos
            if not join or capEdge.z<anchor.z-0.005 or capEdge.z>capFirst.pos.z+0.005
                or join.z<lastCap.pos.z-0.005 or join.z>hit.pos.z+0.005
                or join.z<=capEdge.z+0.005
                or (join.x-capEdge.x)*inward.x+(join.y-capEdge.y)*inward.y<=0.005 then
                return finish("profile_join_unassociated")
            end
            for _,cap in ipairs(capHits)do
                if not strictPlane(cap,capFirst)then return finish("profile_join_unassociated") end
            end
            local lowerReach=(capEdge.x-p.x)*f.x+(capEdge.y-p.y)*f.y
            local upperReach=(join.x-p.x)*f.x+(join.y-p.y)*f.y
            if not finite(lowerReach) or not finite(upperReach) or lowerReach<0.08
                or upperReach>REACH+0.005 or upperReach<=lowerReach+0.005 then
                return finish("profile_join_outside_reach")
            end
            local capOffset=math.min(0.02,(join.z-capEdge.z)*0.25)
            profileChecks={
                {height=capEdge.z-math.min(0.02,math.max(0.005,(capEdge.z-anchor.z)*0.5)),face=wall},
                {height=capEdge.z+capOffset,face=capFirst},
                {height=join.z-capOffset,face=lastCap},
                {height=join.z+math.min(0.02,math.max(0.005,(hit.pos.z-join.z)*0.5)),face=hit},
            }
            data.profileTransitions=1
            emit("profile_join",c,"result=planes_associated lower_height="..num(capEdge.z-p.z)
                .." upper_height="..num(join.z-p.z).." lower_pos="..vector(capEdge).." upper_pos="..vector(join)
                .." point_connection=pending")
            -- Rebase to the observed upper face and its own contiguous band.
            -- Lower bevel evidence cannot authorize a cap above this face or
            -- exempt a bare final face from the two near-top confirmations.
            wall=hit;row=bandEnd(wall,upper)
            if row==#FACE_HEIGHTS then return finish("face_above_inspection_range") end
            inward,faceRecede=faceBasis(wall)
            data.finalFaceReach=wall.dist;data.topTransitionHits=0
            capFirst=nil;capEdge=nil;capHits=nil
            upper=row+1
        else
            local faceAtHeight=add(wall.pos,f,faceRecede*(hit.pos.z-wall.pos.z))
            local inset=(hit.pos.x-faceAtHeight.x)*inward.x+(hit.pos.y-faceAtHeight.y)*inward.y
            if n.z<=0.25 or n.z>=0.9 or -(n.x*f.x+n.y*f.y)<0.35 or inset< -0.04 or inset>REACH then
                return finish("face_transition_unassociated")
            end
            local edge=edgeAt(wall,hit)
            if not edge or edge.z-p.z<FACE_HEIGHTS[row]-0.08 or edge.z-p.z>FACE_HEIGHTS[row+1]+0.08 then
                return finish("face_transition_unassociated")
            end
            if not capFirst then capFirst=hit;capEdge=edge;capHits={} end
            capHits[#capHits+1]=hit
            data.transitionHits=data.transitionHits+1
            data.topTransitionHits=data.topTransitionHits+1
            upper=upper+1
        end
    end
    if upper>#FACE_HEIGHTS then return finish("face_above_inspection_range") end
    local topStart=p.z+FACE_HEIGHTS[upper]+0.05
    -- Start from the highest actual hit in this connected face band. A
    -- receding face's low hit is outside its top, even on the same 3D plane.
    -- Project forward only to this band's first observed clear-row height;
    -- the projected point must stay inside the inspected forward segment.
    -- For an overhanging face retain the measured high hit, rather than
    -- projecting back toward the player through unobserved geometry.
    local faceAnchor=surfaces[wall.lane][row].pos
    local projection=math.max(0,faceRecede*(p.z+FACE_HEIGHTS[upper]-faceAnchor.z))
    local topAnchor=add(faceAnchor,f,projection)
    local projectedReach=(topAnchor.x-p.x)*f.x+(topAnchor.y-p.y)*f.y
    if not finite(projection) or not finite(projectedReach) or projectedReach>REACH+0.005 then
        return finish("face_projection_outside_reach")
    end
    data.faceProjection=projection
    emit("face_band",c,"lane="..count(wall.lane).." last_vertical_row="..count(row)
        .." first_clear_row="..count(upper).." transition_hits="..count(data.transitionHits)
        .." top_transition_hits="..count(data.topTransitionHits).." profile_transitions="..count(data.profileTransitions)
        .." pos="..vector(wall.pos).." normal="..vector(wall.normal).." top_start="..num(topStart)
        .." anchor_pos="..vector(topAnchor).." face_projection="..num(projection))
    local lip,previous
    for i,inset in ipairs(TOP_INSETS)do
        local point=add(topAnchor,inward,inset)
        local hit,why=ray(c,"top_"..i,{x=point.x,y=point.y,z=topStart},{x=0,y=0,z=p.z+0.1-topStart})
        if hit==nil then return finish(why) end
        if not hit then return finish("no_lip_sample") end
        local height=hit.pos.z-p.z
        if height<FACE_HEIGHTS[row]-0.08 or height>FACE_HEIGHTS[upper]+0.005 then return finish("lip_inconsistent") end
        if previous then
            local rise=-(previous.normal.x*(hit.pos.x-previous.pos.x)
                +previous.normal.y*(hit.pos.y-previous.pos.y))/previous.normal.z
            if hit.pos.z<previous.pos.z-0.08 or hit.pos.z>previous.pos.z+rise+0.08 then
                return finish("top_transition_disconnected")
            end
        end
        if hit.normal.z>=0.9 then lip=hit;data.topInset=inset;break end
        -- Continue only along an observed rising cap. A slope hit is never
        -- reclassified as flat just to make the search succeed.
        local opposition=-(hit.normal.x*inward.x+hit.normal.y*inward.y)
        if data.topTransitionHits==0 or hit.normal.z<=0.25 or opposition<0.35 then
            -- A continuous rising sample followed by an opposing steep cap
            -- is a local ridge observation, not an observed flat landing.
            if previous and hit.normal.z>0.25 and opposition< -0.35 then return finish("cap_ridge_unwalkable") end
            return finish("top_surface_unwalkable")
        end
        data.transitionHeight=height
        previous=hit
    end
    if not lip then return finish("no_walkable_top") end
    data.height=lip.pos.z-p.z
    data.hostOntoHeight=data.height>=MIN_ONTO-0.005 and data.height<=MAX_ONTO+0.005
    data.band=data.height<0.45-0.005 and "below_native_vault" or data.height<MIN_ONTO-0.005 and "low_vault"
        or data.height>MAX_ONTO+0.005 and "above_native_onto" or "native_onto"
    emit("lip",c,"height="..num(data.height).." reach="..num(data.reach).." pos="..vector(lip.pos)
        .." normal="..vector(lip.normal).." height_band="..data.band.." host_onto_height="..boolean(data.hostOntoHeight))
    -- Base the footprint on the observed flat top, so its approach edge is
    -- checked beyond the chamfer rather than left on the rejected slope.
    local centre=add(lip.pos,inward,DEPTH)
    local lateral={x=inward.y,y=-inward.x,z=0}
    local tops={}
    for i,offset in ipairs({{0,0},{RADIUS,0},{-RADIUS,0},{0,RADIUS},{0,-RADIUS}})do
        local sample=add(add(centre,lateral,offset[1]),inward,offset[2])
        local hit,reason=ray(c,"landing_"..i,{x=sample.x,y=sample.y,z=lip.pos.z+0.12},{x=0,y=0,z=-0.24})
        if hit==nil then return finish(reason) end
        if not hit then return finish("landing_sample_missing") end
        data.topSamples=data.topSamples+1
        if hit.normal.z<0.9 then return finish("top_slope") end
        if math.abs(hit.pos.z-lip.pos.z)>0.08 then return finish("uneven_landing") end
        tops[#tops+1]=hit.pos
    end
    for i,top in ipairs(tops)do
        local hit,reason=ray(c,"headroom_"..i,{x=top.x,y=top.y,z=top.z+0.05},{x=0,y=0,z=BODY_HEIGHT})
        if hit==nil then return finish(reason) end
        if hit then return finish("headroom_obstructed") end
    end
    if #profileChecks>0 then
        data.profileConnected=false
        for i,sample in ipairs(profileChecks)do
            local origin=add(p,right,({0,-0.25,0.25})[wall.lane]);origin.z=sample.height
            local hit,why=ray(c,"profile_connection_"..i,origin,add({x=0,y=0,z=0},f,REACH))
            if hit==nil then return finish(why) end
            if not hit then return finish("profile_connection_missing") end
            if not strictPlane(hit,sample.face)then return finish("profile_connection_unassociated") end
            data.profileSamples=data.profileSamples+1
        end
        data.profileConnected=true
        emit("profile_connection",c,"result=samples_confirmed samples=4")
    end
    if data.topTransitionHits==0 and (wall.normal.z~=0 or data.profileTransitions>0)then
        -- Projection must not promote a disconnected shelf within the sparse
        -- face bracket. Confirm this measured face on its original lane at
        -- the midpoint and just below the flat top. This is point evidence,
        -- not a body sweep or proof that every smaller gap is filled.
        data.faceConnected=false
        local nearHeight=lip.pos.z-0.04
        for i,height in ipairs({(faceAnchor.z+nearHeight)*0.5,nearHeight})do
            local origin=add(p,right,({0,-0.25,0.25})[wall.lane])
            origin.z=height
            local hit,why=ray(c,"face_top_connection_"..i,origin,add({x=0,y=0,z=0},f,REACH))
            if hit==nil then return finish(why) end
            if not hit then return finish("face_top_connection_missing") end
            local plane=(hit.pos.x-wall.pos.x)*wall.normal.x+(hit.pos.y-wall.pos.y)*wall.normal.y
                +(hit.pos.z-wall.pos.z)*wall.normal.z
            local alignment=hit.normal.x*wall.normal.x+hit.normal.y*wall.normal.y+hit.normal.z*wall.normal.z
            -- Ray normals may differ slightly from unit length. Compare a
            -- distance and normalized angle without changing the raw samples
            -- or the existing coarse face/flat-normal admission thresholds.
            local wallMagnitude=length(wall.normal)
            plane=plane/wallMagnitude
            alignment=alignment/(wallMagnitude*length(hit.normal))
            -- This final association is deliberately tighter than coarse band
            -- grouping: a nearby shelf's vertical edge is not the measured lean.
            if math.abs(plane)>0.02 or alignment<=0.995 then return finish("face_top_connection_unassociated") end
        end
        data.faceConnected=true
        emit("face_connection",c,"result=samples_confirmed samples=2")
    end
    if not data.hostOntoHeight then return finish("height_outside_native_onto") end
    return finish("candidate_hint")
end
function L.Tick(state)
    if not L.pending then return false end
    local request=L.pending;L.pending=nil;L.last=nil;L.rays=0
    local ok,c,reason=pcall(context,state)
    if not ok or not c then return clear(reason or "callback_error") end
    if not fresh(c,request) or c.sequence<=request.sequence then return clear("request_expired") end
    if not counter(_KTLedgeScans) or _KTLedgeScans>=MAX_SCANS then return clear("scan_limit") end
    _KTLedgeScans=_KTLedgeScans+1
    c.scan=_KTLedgeScans
    local success,result=pcall(scan,c)
    if not success then return clear("callback_error") end
    return result
end
function L.Status()
    local b=M.nativeBridge
    local ok,c=pcall(context,get(b,"lastFrame"))
    local last=L.last
    if not ok or not c or (last and (c.epoch~=last.epoch or c.identity~=last.identity
        or c.generation~=last.generation or c.reset~=last.reset or c.sequence<last.sequence
        or c.now<last.stamp or c.now-last.stamp>TTL)) then L.last=nil;L.reason="stale" end
    log(L.reason or "idle",L.last)
    return L.reason
end
for _,cmd in ipairs({
    {"kt_ledge_probe","KuttenbergThief.ledgeProbe.Request()","KT: one read-only ledge geometry scan; never climbs"},
    {"kt_ledge_status","KuttenbergThief.ledgeProbe.Status()","KT: fresh ledge probe result; body clearance remains unknown"}})do
    call(System,"AddCCommand",false,cmd[1],cmd[2],cmd[3])
end
