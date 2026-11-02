-- Passive host-stat feedback on the existing canonical script frame. The owned
-- Flash movie expires its display without Update heartbeats; no gameplay writes.
local M=KuttenbergThief
local A=M.access
local get,call=A.member,A.invoke
local previous=get(M,"stealthHud")
if type(get(previous,"Stop"))=="function" then pcall(previous.Stop,"reload") end
local H={enabled=true,visible=false,reason="waiting_for_frame",rendererFailed=false,
    rendererMode="waiting",rendererReason=nil,elementShown=false,elementLoaded=false}
M.stealthHud=H
local PERIOD,MAX_AGE,LOG_PERIOD,MAX_LOGS=0.1,0.15,0.5,120
local KEYS={"vib","evi","noi","fsm","con"}
local ELEMENT,INSTANCE,RECEIPT="KTStealth",0,"kt15"
-- Presentation ranges only: aggregate visibility is neither lux nor detection
-- probability. Noise stays in background diagnostics, never on the display.
local VIS_MIN,VIS_MAX=-2,3.5
-- Deliberately exclude crouch, contact, resources, weapon and combat gates.
-- Feedback supports ordinary standing/crouching, not just eligible swoops.
local GATES={1,2,3,4,5,6,7,8,9,12,15,16,17,21,23,24}
local function finite(v)return type(v)=="number" and v==v and math.abs(v)<1e12 end
local function counter(v)return finite(v) and v>0 and v<=8388607 and v==math.floor(v)end
local function numeric(v)return finite(v) and string.format("%.3f",v) or "--"end
local function token(v)
    return type(v)=="string" and string.match(v,"^[%w_]+$") and string.sub(v,1,48) or "unknown"
end
local function log(message,explicit)
    if not explicit then
        if _KTStealthHudLogs==nil then _KTStealthHudLogs=0 end
        if not finite(_KTStealthHudLogs) or _KTStealthHudLogs<0 or _KTStealthHudLogs>=MAX_LOGS then return end
        _KTStealthHudLogs=_KTStealthHudLogs+1
    end
    pcall(M.Log,"stealth "..message)
end
local function invoke(object,name,method,...)
    local ok,value,status=pcall(call,object,name,method,...)
    if not ok then return nil,"error" end
    if status~="ok" then
        return nil,(status=="missing" or status=="nil" or status=="error") and status or "unknown"
    end
    return value,"ok"
end
local function hideOwned(unload)
    if H.elementShown then invoke(UIAction,"HideElement",false,ELEMENT,INSTANCE) end
    H.elementShown=false
    if unload and H.elementLoaded then
        invoke(UIAction,"UnloadElement",false,ELEMENT,INSTANCE)
        H.elementLoaded=false
    end
end
local function clear(reason)
    hideOwned(false)
    H.visible=false;H.reason=reason;H.values=nil;H.sampleStamp=nil;H.nextRead=nil
    H.lastFrameStamp=nil;H.context=nil;H.lastLogged=nil;H.lastLogStamp=nil
    H.frame=nil;H.observer=nil
end
local function mask(v)
    return type(v)=="string" and #v==26 and string.match(v,"^b[01]+$")
end
local function context(b)
    return {epoch=b.epoch,generation=b.generation,identity=b.identityEpoch,reset=b.resetSerial}
end
local function same(a,b)
    return a and b and a.epoch==b.epoch and a.generation==b.generation
        and a.identity==b.identity and a.reset==b.reset
end
local function frame(state)
    if KuttenbergThief~=M or get(M,"stealthHud")~=H then return nil,"module_changed" end
    local b=get(M,"nativeBridge")
    if type(b)~="table" or b.nativeFailed or b.counterFailed or b.available~=true
        or type(_KTNativePublish)~="function" then return nil,"native_unavailable" end
    if type(state)~="table" or b.lastFrame~=state then return nil,"noncanonical_frame" end
    local p=A.currentPlayer()
    local id=get(p,"id")
    if not p or id==nil or b.owner~=p or b.ownerId~=id or get(state,"originalId")~=id
        or b.frameOwner~=p then return nil,"owner_changed" end
    if M.hadPlayer~=true or not counter(M.generation) or b.generation~=M.generation
        or b.frameGeneration~=M.generation or b.frameIdentityEpoch~=b.identityEpoch
        or not counter(b.epoch) or not counter(b.identityEpoch) or not counter(b.sequence)
        or not finite(b.resetSerial) or b.resetSerial<0 or b.resetSerial>8388607
        or b.resetSerial~=math.floor(b.resetSerial) then return nil,"context_unknown" end
    local known,safe=get(state,"known"),get(state,"safe")
    if not mask(known) or not mask(safe) then return nil,"gates_unknown" end
    for _,i in ipairs(GATES)do
        if string.sub(known,i+1,i+1)~="1" or string.sub(safe,i+1,i+1)~="1" then
            return nil,"context_unsafe"
        end
    end
    local stamp,dt=get(state,"stamp"),get(state,"dt")
    local now,status=invoke(System,"GetCurrAsyncTime",false)
    if not finite(stamp) or stamp<0 or not finite(dt) or dt<=0 or dt>MAX_AGE
        or status~="ok" or not finite(now) or now<stamp or now-stamp>MAX_AGE then
        return nil,"frame_stale"
    end
    return {bridge=b,player=p,stamp=stamp,context=context(b)},nil
end
local function readValues(f)
    local values,known={},0
    for _,key in ipairs(KEYS)do
        -- Never store the player, soul or native handles in the cache.
        if A.currentPlayer()~=f.player or f.bridge.owner~=f.player then return nil,"owner_changed" end
        local value,status=invoke(get(f.player,"soul"),"GetDerivedStat",true,key)
        if status=="ok" and not finite(value) then value=nil;status="nonfinite" end
        values[key]={value=value,status=status}
        if finite(value) then known=known+1 end
    end
    return values,known
end
local function value(key)return H.values and H.values[key] and H.values[key].value end
local function text()
    return "VIS "..numeric(value("vib")),
        "HUD text: "..token(H.rendererReason)
end
local function fingerprint()
    local parts={}
    for _,key in ipairs(KEYS)do
        local entry=H.values and H.values[key]
        parts[#parts+1]=key.."="..numeric(value(key))
        parts[#parts+1]=key.."_status="..token(entry and entry.status)
    end
    return table.concat(parts," ")
end
local function position()
    local viewport,status=invoke(System,"GetViewport",false)
    local width,height=get(viewport,"width"),get(viewport,"height")
    if status=="ok" and finite(width) and finite(height) and width>=320 and height>=200
        and width<=32768 and height<=32768 then return math.max(16,width*0.025),height*0.2 end
    return 24,120 -- The shipped caller also supports fixed pixel coordinates.
end
local function failHud(reason)
    clear(reason);H.enabled=false;H.rendererFailed=true
    log("hud_disabled reason="..reason.." movement_unchanged=true")
    return false
end
local function graphicFailure(reason)
    hideOwned(true)
    H.rendererFailed=true;H.rendererReason=reason;H.rendererMode="text"
    log("renderer_fallback reason="..token(reason).." movement_unchanged=true")
    return false
end
local function amount(raw,minimum,maximum)
    if not finite(raw) then return nil end
    return math.max(0,math.min(1,(raw-minimum)/(maximum-minimum)))
end
local function displayAmount()
    -- The owned movie smooths these targets; avoid filtering twice in Lua.
    return amount(value("vib"),VIS_MIN,VIS_MAX)
end
local function renderGraphic(visibility)
    if H.rendererFailed then return false end
    for _,name in ipairs({"ShowElement","HideElement","UnloadElement","CallFunction"})do
        if type(get(UIAction,name))~="function" then return graphicFailure("ui_api_missing") end
    end
    if not H.elementShown then
        -- A failed Show may still have partially initialized the owned movie.
        H.elementLoaded=true
        local shown,status=invoke(UIAction,"ShowElement",false,ELEMENT,INSTANCE)
        if status~="ok" or shown~=true then
            return graphicFailure("ui_show_"..(status=="ok" and "refused" or token(status)))
        end
        H.elementShown=true
    end
    local receipt,status=invoke(UIAction,"CallFunction",false,ELEMENT,INSTANCE,"Update",
        visibility or 0,visibility~=nil)
    -- Retail CallFunction converts the Flash result to a string. Requiring our
    -- receipt distinguishes successful delivery from pcall/native-call success.
    if status~="ok" or receipt~=RECEIPT then
        return graphicFailure("ui_update_"..(status=="ok" and "receipt" or token(status)))
    end
    H.rendererMode="graphic";H.rendererReason=nil
    return true
end
local function renderText()
    local renderer=get(System,"DrawText")
    if type(renderer)~="function" then return failHud("text_renderer_missing") end
    local x,y=position()
    local a,b=text()
    for i,line in ipairs({a,b})do
        if not pcall(renderer,x,y+(i-1)*24,line) then return failHud("text_renderer_error") end
    end
    H.rendererMode="text"
    return true
end
local function tick(state)
    H.visible=false
    if not H.enabled then return false end
    local f,why=frame(state)
    if not f then clear(why);return false end
    if H.context and not same(H.context,f.context) then clear("context_changed") end
    if H.lastFrameStamp and (f.stamp<H.lastFrameStamp or f.stamp-H.lastFrameStamp>MAX_AGE) then
        clear("clock_discontinuity");H.lastFrameStamp=f.stamp;return false
    end
    H.lastFrameStamp=f.stamp
    if not H.nextRead or f.stamp>=H.nextRead then
        local values,known=readValues(f)
        if not values then clear(known);return false end
        local current=frame(state)
        if not current or not same(current.context,f.context) then clear("sample_context_changed");return false end
        H.values=values;H.context=f.context;H.sampleStamp=f.stamp;H.nextRead=f.stamp+PERIOD
        H.frame=f.bridge.sequence;H.observer=get(state,"observerSequence")
        local key=fingerprint()
        if key~=H.lastLogged and (not H.lastLogStamp or f.stamp-H.lastLogStamp>=LOG_PERIOD) then
            log("sample "..key.." async="..string.format("%.6f",f.stamp)
                .." epoch="..string.format("%.0f",f.context.epoch)
                .." generation="..string.format("%.0f",f.context.generation)
                .." identity="..string.format("%.0f",f.context.identity)
                .." reset="..string.format("%.0f",f.context.reset)
                .." frame="..string.format("%.0f",H.frame))
            H.lastLogged=key;H.lastLogStamp=f.stamp
        end
        if known==0 then
            hideOwned(false)
            H.reason="all_unknown"
            return false
        end
    end
    if not H.sampleStamp or f.stamp<H.sampleStamp or f.stamp-H.sampleStamp>MAX_AGE then
        clear("sample_stale");return false
    end
    if not finite(value("vib")) then
        hideOwned(false);H.reason="display_unknown";return false
    end
    local visibility=displayAmount()
    if not renderGraphic(visibility) and not renderText() then return false end
    H.visible=true;H.reason="ok"
    return true
end
function H.Tick(state)
    local ok,result=pcall(tick,state)
    if not ok then return failHud("callback_error") end
    return result
end
function H.Snapshot()
    if H.values then
        local b=get(M,"nativeBridge")
        local ok,current=pcall(frame,get(b,"lastFrame"))
        if not ok or not current or not same(current.context,H.context)
            or not H.sampleStamp or current.stamp<H.sampleStamp
            or current.stamp-H.sampleStamp>MAX_AGE then clear("snapshot_stale") end
    end
    local out={enabled=H.enabled,visible=H.visible,reason=H.reason,rendererFailed=H.rendererFailed,
        rendererMode=H.rendererMode,rendererReason=H.rendererReason,
        visibilityMinimum=VIS_MIN,visibilityMaximum=VIS_MAX,
        stamp=H.sampleStamp,frame=H.frame,observer=H.observer,values={}}
    if H.context then
        out.epoch=H.context.epoch;out.generation=H.context.generation
        out.identity=H.context.identity;out.reset=H.context.reset
    end
    for _,key in ipairs(KEYS)do
        local v=H.values and H.values[key]
        out.values[key]={value=v and v.value or nil,status=v and v.status or "unknown"}
    end
    return out
end
function H.On()
    hideOwned(true)
    clear("waiting_for_frame");H.enabled=true;H.rendererFailed=false
    H.rendererMode="waiting";H.rendererReason=nil
    log("enabled=true",true);return true
end
function H.Stop(reason)
    H.enabled=false;clear(token(reason or "command"));hideOwned(true);return true
end
function H.Invalidate(reason)
    clear(token(reason or "context_changed"));return true
end
function H.Off()
    H.Stop("command");log("enabled=false",true);return true
end
function H.Status()
    local result=H.Snapshot()
    log("status enabled="..tostring(H.enabled).." visible="..tostring(H.visible)
        .." reason="..token(H.reason).." renderer="..token(H.rendererMode)
        .." renderer_reason="..token(H.rendererReason).." "..fingerprint(),true)
    return result
end
for _,cmd in ipairs({
    {"kt_stealth_on","KuttenbergThief.stealthHud.On()","KT: show passive host visibility feedback"},
    {"kt_stealth_off","KuttenbergThief.stealthHud.Off()","KT: hide passive stealth feedback"},
    {"kt_stealth_status","KuttenbergThief.stealthHud.Status()","KT: record current raw host stealth readings"}})do
    invoke(System,"AddCCommand",false,cmd[1],cmd[2],cmd[3])
end
