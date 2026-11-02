-- Lua 5.1. Read-only M0 diagnostic: never writes player/physics/camera state.
local previous = KuttenbergThief
if previous and previous.Stop then previous.Stop("reload") end
if previous and UIAction and type(UIAction.UnregisterEventSystemListener) == "function" then
    pcall(UIAction.UnregisterEventSystemListener, previous, "")
end
KuttenbergThief = { version = "0.0.21", intervalMs = 100, maxSamples = 1200, actorStatesSequence = 0,
    remaining = 0, enabled = true, timer = nil, hadPlayer = false, generation = 0 }
local M = KuttenbergThief

local function member(object, name)
    if object == nil then return nil end
    local ok, value = pcall(function() return object[name] end)
    if ok then return value end
    return nil
end

local function invoke(object, name, method, ...)
    local fn = member(object, name)
    if type(fn) ~= "function" then return nil, "missing" end
    local ok, value
    if method then ok, value = pcall(fn, object, ...)
    else ok, value = pcall(fn, ...) end
    if not ok then return nil, "error" end
    if value == nil then return nil, "nil" end
    return value, "ok"
end

local function scalar(value)
    if value == nil then return "unknown" end
    local kind = type(value)
    if kind == "number" then return string.format("%.4f", value) end
    if kind == "boolean" then return tostring(value) end
    if kind == "string" then return string.sub(string.gsub(value, "[%c|]", "_"), 1, 100) end
    if kind == "table" then
        local x, y, z = member(value, "x"), member(value, "y"), member(value, "z")
        if type(x) == "number" and type(y) == "number" and type(z) == "number" then
            return string.format("(%.4f,%.4f,%.4f)", x, y, z)
        end
    end
    return "<" .. kind .. ">"
end

function M.Log(message)
    if not M.enabled then return end
    local fn = member(System, "LogAlways") or member(System, "Log")
    if type(fn) == "function" then pcall(fn, "[KT] " .. message) end
end

local function currentPlayer()
    -- Do not retain an entity across load transitions.
    local p = player or g_localActor
    if p and type(member(p, "GetWorldPos")) == "function" then return p end
    return nil
end

M.access = {member=member, invoke=invoke, scalar=scalar, currentPlayer=currentPlayer}

local function field(object, name, method, ...)
    local value, status = invoke(object, name, method, ...)
    if status ~= "ok" then return "unknown:" .. status end
    return scalar(value)
end

local observerKeys = {"crouch", "inAir", "cutscene", "interactiveScene", "dialogue",
    "horse", "minigame", "actorAnim", "onBed"}

function M.BeginActorObservation()
    M.pendingActorStates = {}
end

function M.ObserveActorState(name, value)
    if not M.pendingActorStates or type(value) ~= "boolean" then return end
    for _, key in ipairs(observerKeys) do
        if name == key then M.pendingActorStates[key] = value; return end
    end
end

function M.CommitActorObservation()
    local values = M.pendingActorStates
    M.pendingActorStates = nil
    if not values then return end
    local entries = {}
    for _, key in ipairs(observerKeys) do
        if type(values[key]) ~= "boolean" then
            M.actorStates = nil; M.Log("actor_observer rejected=incomplete"); return
        end
        entries[#entries+1] = key .. "=" .. tostring(values[key])
    end
    local fingerprint = table.concat(entries, " ")
    M.actorStates = values
    M.actorStatesTime = invoke(System, "GetCurrAsyncTime", false)
    M.actorStatesSequence = (M.actorStatesSequence or 0) + 1
    if fingerprint ~= M.lastActorStates and (M.observerLogs or 0) < 300 then
        M.observerLogs = (M.observerLogs or 0)+1
        M.lastActorStates = fingerprint
        M.Log("actor_observer " .. fingerprint .. " async=" .. scalar(M.actorStatesTime)
            .. " source=xgen verified=false")
    end
end

function M.Capabilities()
    local p = currentPlayer()
    local groups = {
        {"System", System, {"AddCCommand", "GetCurrTime", "GetCurrAsyncTime", "GetFrameTime", "GetFrameID", "GetViewCameraPos", "GetViewCameraDir", "GetViewCameraAngles", "SetViewCameraFov", "IsDevModeEnable"}},
        {"Script", Script, {"SetTimer", "KillTimer", "SetTimerForFunction"}},
        {"Physics", Physics, {"RayWorldIntersection", "RayTraceCheck", "SamplePhysEnvironment"}},
        {"Game", Game, {"IsLoadingEngineSaveGame", "IsGamePaused", "GetEntityState"}},
        {"CryAction", CryAction, {"IsGameStarted", "IsGamePaused"}},
        {"ActionMapManager", ActionMapManager, {"EnableActionMap", "EnableActionFilter"}},
        {"player", p, {"GetWorldPos", "GetWorldAngles", "GetVelocity", "GetSpeed", "GetStance", "GetPhysicalStats", "SetVelocity", "AddImpulse", "GetCurAnimation", "OnAction"}},
        {"actor", member(p, "actor"), {"GetCurrentAnimationState", "GetPhysicalizationProfile", "GetHeadPos", "GetStats", "GetParams", "IsDead", "IsUnconscious", "IsFlying", "IsCarryingCorpse", "SimulateOnAction", "SetMovementControlledByAnimation", "SetMovementRestriction", "SetViewShake"}},
        {"player_interface", member(p, "player"), {"IsSitting", "IsLaying", "GetFlyMode"}},
        {"human", member(p, "human"), {"IsMounted", "IsOnLadder", "IsInDialog", "IsPickpocketing", "IsWeaponDrawn", "SetAnimMotionParam"}},
        {"soul", member(p, "soul"), {"IsInCombatDanger", "IsInTenseCircumstance", "GetState", "GetStamina", "GetHealth"}},
    }
    M.Log("capabilities_begin version=" .. M.version .. " presence_only=true")
    for _, group in ipairs(groups) do
        local entries = {}
        for _, name in ipairs(group[3]) do
            entries[#entries + 1] = name .. ":" .. type(member(group[2], name))
        end
        M.Log("api=" .. group[1] .. " " .. table.concat(entries, " "))
    end
    M.Log("capabilities_end movement_enabled=false")
end

function M.Snapshot(reason)
    local p = currentPlayer()
    if not p then M.Log("sample reason=" .. reason .. " player=absent"); return false end
    local a, h, s = member(p, "actor"), member(p, "human"), member(p, "soul")
    local entries = {
        "sample reason=" .. reason, "generation=" .. M.generation,
        "time=" .. field(System, "GetCurrTime", false),
        "async=" .. field(System, "GetCurrAsyncTime", false),
        "dt=" .. field(System, "GetFrameTime", false),
        "pos=" .. field(p, "GetWorldPos", true), "angles=" .. field(p, "GetWorldAngles", true),
        "velocity=" .. field(p, "GetVelocity", true), "speed=" .. field(p, "GetSpeed", true),
        "stance_raw=" .. field(p, "GetStance", true),
        "xgen_crouch_raw=" .. scalar(member(M.actorStates, "crouch")),
        "xgen_last_async=" .. scalar(M.actorStatesTime),
        "ai_stance_raw=" .. field(AI, "GetStance", false, member(p, "id")),
        "anim=" .. field(a, "GetCurrentAnimationState", true),
        "profile=" .. field(a, "GetPhysicalizationProfile", true),
        "flying_raw=" .. field(a, "IsFlying", true),
        "sitting=" .. field(member(p, "player"), "IsSitting", true),
        "laying=" .. field(member(p, "player"), "IsLaying", true),
        "fly_mode=" .. field(member(p, "player"), "GetFlyMode", true),
        "mounted=" .. field(h, "IsMounted", true), "ladder=" .. field(h, "IsOnLadder", true),
        "dialog=" .. field(h, "IsInDialog", true), "weapon=" .. field(h, "IsWeaponDrawn", true),
        "dead=" .. field(a, "IsDead", true), "unconscious=" .. field(a, "IsUnconscious", true),
        "combat=" .. field(s, "IsInCombatDanger", true),
        "stamina=" .. field(s, "GetState", true, "stamina"),
        "health=" .. field(s, "GetState", true, "health"),
        "loading=" .. field(Game, "IsLoadingEngineSaveGame", false),
        "paused=" .. field(CryAction, "IsGamePaused", false),
        "grounded=unknown", "camera=" .. field(System, "GetViewCameraPos", false),
    }
    M.Log(table.concat(entries, " | "))
    return true
end

local function logTable(label, value, status)
    if type(value) ~= "table" then
        M.Log("detail=" .. label .. " status=" .. status .. " value=" .. scalar(value)); return
    end
    local keys = {}
    for k, v in pairs(value) do
        if type(k) == "string" or type(k) == "number" then keys[#keys+1] = k end
    end
    table.sort(keys, function(a,b) return tostring(a)<tostring(b) end)
    local entries = {}
    for i=1,math.min(#keys,24) do
        local key=keys[i]; entries[#entries+1]=tostring(key) .. "=" .. scalar(value[key])
    end
    M.Log("detail=" .. label .. " status=" .. status .. " " .. table.concat(entries, " | "))
end

function M.Detail()
    local p = currentPlayer()
    if not p then M.Log("detail skipped=no_player"); return end
    local stats, status = invoke(p, "GetPhysicalStats", true)
    logTable("physical_stats", stats, status)
    local params, ps = invoke(member(p, "actor"), "GetParams", true)
    logTable("actor_params", params, ps)
    local actorStats, ast = invoke(member(p, "actor"), "GetStats", true)
    logTable("actor_stats", actorStats, ast)
    -- These are historical script-table candidates, not trusted crouch sources.
    for _, key in ipairs({"stats", "actorStats"}) do
        local value = member(p, key)
        for _, state in ipairs({"stance", "inAir", "onGround", "bFlying", "bSneaking"}) do
            M.Log("table_state path=" .. key .. "." .. state .. " value=" .. scalar(member(value, state)))
        end
    end
    M.Log("detail=stance ai=" .. field(AI, "GetStance", false, member(p, "id"))
        .. " STANCE_CROUCH=" .. scalar(STANCE_CROUCH) .. " STANCE_STAND=" .. scalar(STANCE_STAND)
        .. " E_ACTORSTANCE_CROUCH=" .. scalar(E_ACTORSTANCE_CROUCH)
        .. " E_ACTORSTANCE_NORMAL=" .. scalar(E_ACTORSTANCE_NORMAL)
        .. " bSneaking=" .. scalar(member(member(p, "actor"), "bSneaking")))
    for _, name in ipairs({"pl_movement.speedScale", "pl_movement.crouch_SpeedScale",
        "pl_movement.sprint_SpeedScale", "wh_actor_MovementSpeedMultEnabled",
        "wh_actor_MovementSpeedMultDefault"}) do
        M.Log("cvar_read name=" .. name .. " value=" .. field(System, "GetCVar", false, name))
    end
    M.Log("detail=runtime devmode=" .. field(System, "IsDevModeEnable", false)
        .. " game_started=" .. field(CryAction, "IsGameStarted", false))
end

function M.ContactSample(p)
    local value, status = invoke(member(p, "actor"), "IsFlying", true)
    local key = status .. ":" .. scalar(value)
    -- 10 Hz observation, logging only changes. Never equate nil/error with grounded.
    if key ~= M.lastFlight then
        M.lastFlight = key
        M.Log("contact_change flying_raw=" .. scalar(value) .. " status=" .. status
            .. " time=" .. field(System, "GetCurrTime", false)
            .. " async=" .. field(System, "GetCurrAsyncTime", false)
            .. " pos=" .. field(p, "GetWorldPos", true)
            .. " anim=" .. field(member(p, "actor"), "GetCurrentAnimationState", true)
            .. " grounded=unverified")
    end
end

function M.Rays()
    local p = currentPlayer()
    local pos = invoke(System, "GetViewCameraPos", false)
    local dir = invoke(System, "GetViewCameraDir", false)
    if not p or type(pos) ~= "table" or type(dir) ~= "table" or type(ent_all) ~= "number" then
        M.Log("ray skipped=missing_player_camera_or_ent_all"); return
    end
    if type(dir.x) ~= "number" or type(dir.y) ~= "number" or type(dir.z) ~= "number" then
        M.Log("ray skipped=invalid_direction"); return
    end
    local hits, status = invoke(Physics, "RayWorldIntersection", false,
        pos, {x=dir.x*3, y=dir.y*3, z=dir.z*3}, 1, ent_all, p.id)
    local first = type(hits) == "table" and hits[1] or nil
    M.Log("ray status=" .. status .. " return_type=" .. type(hits)
        .. " count=" .. (type(hits) == "table" and #hits or -1)
        .. " first_dist=" .. scalar(member(first, "dist"))
        .. " first_pos=" .. scalar(member(first, "pos"))
        .. " first_normal=" .. scalar(member(first, "normal"))
        .. " note=ray_is_not_body_clearance")
end

function M.Stop(reason)
    local invalidateHud=member(member(M,"stealthHud"),"Invalidate")
    if type(invalidateHud)=="function" then pcall(invalidateHud,reason or "command") end
    local stopTerrain=member(member(M,"terrainProbe"),"Stop")
    if type(stopTerrain)=="function" then pcall(stopTerrain,reason or "command") end
    local stopLedge=member(member(M,"ledgeProbe"),"Stop")
    if type(stopLedge)=="function" then pcall(stopLedge,reason or "command") end
    if M.nativeBridge then M.nativeBridge.Invalidate(reason or "command") end
    if M.speedTrial then M.speedTrial.Cancel(reason or "command") end
    M.remaining = 0
    if M.timer then invoke(Script, "KillTimer", false, M.timer); M.timer = nil end
    M.Log("watch_stopped reason=" .. (reason or "command"))
end

function M.Tick()
    M.timer = nil
    local p = currentPlayer()
    if p and not M.hadPlayer then
        M.hadPlayer = true; M.generation = M.generation + 1
        M.lastFlight = nil
        M.Capabilities()
        M.Detail()
        M.Rays()
        M.remaining = M.maxSamples
        M.Log("player_available watch_ticks=" .. M.remaining)
    elseif not p and M.hadPlayer then
        M.hadPlayer = false; M.remaining = 0
        M.Log("player_lost reason=load_or_transition")
    end
    if p and M.remaining > 0 then
        M.ContactSample(p)
        if (M.maxSamples - M.remaining) % 10 == 0 then M.Snapshot("watch") end
        M.remaining = M.remaining - 1
        if M.remaining == 0 then M.Log("watch_complete") end
    end
    if M.remaining > 0 or not M.hadPlayer then M.Schedule() end
end

function M.Schedule()
    if M.timer then return end
    local token, status = invoke(Script, "SetTimer", false, M.intervalMs, function()
        if KuttenbergThief ~= M then return end
        local ok = pcall(M.Tick)
        if not ok then M.Log("tick_failed watch_disabled=true"); M.Stop("tick_error") end
    end)
    M.timer = token
    if status ~= "ok" then M.Log("timer_status=" .. status) end
end

function M.Watch()
    M.Stop("restart"); M.lastFlight = nil; M.remaining = M.maxSamples; M.Schedule()
    M.Log("watch_started max_ticks=" .. M.maxSamples .. " interval_ms=" .. M.intervalMs)
end

function M.OnGameplayStarted()
    if M.nativeBridge then M.nativeBridge.Invalidate("gameplay_started") end
    if M.speedTrial then M.speedTrial.Cancel("gameplay_started") end
    M.hadPlayer = false
    M.actorStates = nil; M.pendingActorStates = nil; M.actorStatesTime = nil; M.actorStatesSequence = 0
    M.lastActorStates = nil; M.observerLogs = 0
    M.Watch()
end

M.Log("boot version=" .. M.version .. " mode=automatic_read_only movement_enabled=false")
local commands = {
    {"kt_probe", "KuttenbergThief.Capabilities(); KuttenbergThief.Snapshot('manual')", "KT: inspect available API and player state"},
    {"kt_watch", "KuttenbergThief.Watch()", "KT: observe contact at 10 Hz for 120s; state summaries at 1 Hz"},
    {"kt_stop", "KuttenbergThief.Stop('command')", "KT: stop sample timer"},
    {"kt_mark", "KuttenbergThief.Snapshot('MARK')", "KT: insert a player-state marker"},
    {"kt_rays", "KuttenbergThief.Rays()", "KT: read-only 3m camera ray; no body-clearance claim"},
    {"kt_detail", "KuttenbergThief.Detail()", "KT: read-only physical stats, AI stance, state and movement-variable probes"},
}
for _, cmd in ipairs(commands) do
    local _, status = invoke(System, "AddCCommand", false, cmd[1], cmd[2], cmd[3])
    M.Log("command=" .. cmd[1] .. " registration=" .. status)
end
if UIAction and type(UIAction.RegisterEventSystemListener) == "function" then
    local ok = pcall(UIAction.RegisterEventSystemListener, M, "System", "OnGameplayStarted", "OnGameplayStarted")
    M.Log("gameplay_listener=" .. tostring(ok))
end
M.Capabilities()
M.Schedule()
