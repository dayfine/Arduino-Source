-- MgbaLink v1 bridge for mGBA 0.10.5.
--
-- Load it by hand once per mGBA launch: Tools > Scripting, then File > Load script.
-- It listens on 127.0.0.1 (first free port from CONFIG.port) and speaks the line protocol in
-- lab/mgba/PROTOCOL.md to one client. Design: docs/design/Lab-mGBA-Console.md.
--
-- Only the mGBA 0.10 scripting API is used: socket.bind/listen/accept/receive/send/poll,
-- callbacks:add("frame"/"keysRead"), emu:currentFrame/getKeys/setKeys/getGameCode/screenshot/
-- saveStateFile/loadStateFile/reset, console:log/warn/error.

local CONFIG = {
    host = "127.0.0.1",
    port = 47801,       -- first port tried; the next CONFIG.port_tries - 1 ports are tried after it
    port_tries = 10,
    mode = "paced",     -- "paced": stall in the frame callback at the GRANT frame; "free": ignore GRANT
    frame_path = nil,   -- FRAME screenshot file; nil = <TMPDIR>/mgba-bridge-<port>.png
    stall_limit_s = 30, -- a stall longer than this drops the grant and runs free (0 = no limit)
    clock = nil,        -- seconds clock for the stall limit; nil = os.time
}
-- A caller (the Lua tests) may preset MGBA_BRIDGE_CONFIG before loading this file.
if type(MGBA_BRIDGE_CONFIG) == "table" then
    for k, v in pairs(MGBA_BRIDGE_CONFIG) do
        CONFIG[k] = v
    end
end

local PROTOCOL_VERSION = "1"
local BRIDGE_VERSION = "bridge-0.1"
local STATE_FLAGS = 31          -- C.SAVESTATE.ALL; includes SAVEDATA (the load default 29 does not)
local KEY_MASK_MAX = 0x3FF      -- 10 GBA keys
local MAX_LINE = 4096
local SCREEN_W, SCREEN_H = 240, 160
local SEND_MAX_ATTEMPTS = 100   -- cap retries on AGAIN/partial sends so a wedged peer cannot hang the emulator

local server = nil
local port = nil
local client = nil
local inbuf = ""
local grant = nil       -- paced mode: stall while currentFrame() >= grant; nil = run free
local pending = {}      -- queued KEYS, sorted by frame: { frame = F, mask = M }
local held = nil        -- mask the bridge holds; nil = leave the keys to mGBA's own input
local applied_frame, applied_mask = nil, nil -- last time the held mask changed
local read_frame, read_keys = nil, nil       -- frame and getKeys() at the last keysRead

local function log(msg)
    console:log("[mgba-link] " .. msg)
end

local function warn(msg)
    console:warn("[mgba-link] " .. msg)
end

local function now_s()
    if CONFIG.clock then
        return CONFIG.clock()
    end
    if os and os.time then
        return os.time()
    end
    return nil
end

local function frame_path()
    if CONFIG.frame_path then
        return CONFIG.frame_path
    end
    local dir = (os and os.getenv and os.getenv("TMPDIR")) or "/tmp"
    dir = dir:gsub("/+$", "")
    return dir .. "/mgba-bridge-" .. tostring(port) .. ".png"
end

local function close_socket(sock)
    pcall(function() sock:close() end)
end

local function opt(v)
    if v == nil then
        return "-"
    end
    return tostring(v)
end

local function release_keys()
    pending = {}
    if held ~= nil and emu then
        emu:setKeys(0)
    end
    held = nil
end

local function drop_client(reason)
    if client == nil then
        return
    end
    log("client disconnected (" .. reason .. "); keys released, running free")
    close_socket(client)
    client = nil
    inbuf = ""
    grant = nil
    release_keys()
end

local function send_line(line)
    if client == nil then
        return
    end
    local data = line .. "\n"
    local from = 1
    for _ = 1, SEND_MAX_ATTEMPTS do
        local last, err = client:send(data, from)
        if last == nil then
            if err ~= socket.ERRORS.AGAIN then
                drop_client("send error " .. tostring(err))
                return
            end
        else
            if last >= #data then
                return
            end
            from = last + 1
        end
    end
    drop_client("send stalled")
end

-- Applies every queued KEYS whose frame has come, in frame order; the last one wins.
local function apply_due(cur)
    while pending[1] ~= nil and pending[1].frame <= cur do
        local e = table.remove(pending, 1)
        if e.mask ~= held then
            applied_frame, applied_mask = cur, e.mask
        end
        held = e.mask
    end
end

local function queue_keys(frame, mask)
    local i = #pending + 1
    while i > 1 and pending[i - 1].frame > frame do
        i = i - 1
    end
    table.insert(pending, i, { frame = frame, mask = mask })
end

local function game_code()
    local code = emu:getGameCode() or ""
    code = code:gsub("^AGB%-", "")
    if code == "" then
        return "????"
    end
    return code
end

local function to_int(s)
    if s == nil or not s:match("^%d+$") then
        return nil
    end
    return math.tointeger(tonumber(s))
end

local handlers = {}

function handlers.HELLO(args)
    if args[1] ~= PROTOCOL_VERSION then
        return "ERR version " .. PROTOCOL_VERSION
    end
    return "OK " .. PROTOCOL_VERSION .. " " .. game_code() .. " " .. BRIDGE_VERSION .. " " .. CONFIG.mode
end

function handlers.GRANT(args)
    local f = to_int(args[1])
    if f == nil then
        return "ERR usage GRANT <frame>"
    end
    if CONFIG.mode == "paced" then
        grant = f
    end
    return "OK " .. emu:currentFrame()
end

function handlers.KEYS(args)
    local f, mask = to_int(args[1]), to_int(args[2])
    if f == nil or mask == nil or mask > KEY_MASK_MAX then
        return "ERR usage KEYS <frame> <mask 0..1023>"
    end
    local cur = emu:currentFrame()
    queue_keys(f, mask)
    if f < cur then
        apply_due(cur)
        emu:setKeys(held)
        return "LATE " .. (cur - f)
    end
    return "OK"
end

function handlers.FRAME(_)
    local path = frame_path()
    emu:screenshot(path)
    return "FRAME " .. emu:currentFrame() .. " " .. SCREEN_W .. " " .. SCREEN_H .. " " .. path
end

local function state_file(line, cmd)
    local file = line:match("^%u+%s+(.-)%s*$")
    if file == nil or file == "" then
        return nil, "ERR usage " .. cmd .. " <file>"
    end
    return file, nil
end

function handlers.SAVE(_, line)
    local file, err = state_file(line, "SAVE")
    if file == nil then
        return err
    end
    if not emu:saveStateFile(file, STATE_FLAGS) then
        return "ERR save failed"
    end
    return "OK " .. emu:currentFrame()
end

function handlers.LOAD(_, line)
    local file, err = state_file(line, "LOAD")
    if file == nil then
        return err
    end
    if not emu:loadStateFile(file, STATE_FLAGS) then
        return "ERR load failed"
    end
    pending = {}
    local cur = emu:currentFrame()
    if grant ~= nil then
        grant = cur -- hold at the loaded frame until the client grants again
    end
    return "OK " .. cur
end

function handlers.RESET(_)
    pending = {}
    emu:reset()
    if grant ~= nil then
        grant = emu:currentFrame()
    end
    return "OK"
end

-- Clean goodbye: the bridge replies, then releases keys and grant and closes the connection.
function handlers.BYE(_)
    return "OK BYE"
end

-- Diagnostic for the smoke client and tests; the app does not depend on it.
function handlers.STATUS(_)
    return "OK frame=" .. emu:currentFrame()
        .. " keys=" .. emu:getKeys()
        .. " read_frame=" .. opt(read_frame)
        .. " read_keys=" .. opt(read_keys)
        .. " applied_frame=" .. opt(applied_frame)
        .. " applied_mask=" .. opt(applied_mask)
        .. " grant=" .. opt(grant)
        .. " mode=" .. CONFIG.mode
end

local function handle_line(line)
    local args = {}
    for word in line:gmatch("%S+") do
        args[#args + 1] = word
    end
    if #args == 0 then
        return nil
    end
    local cmd = table.remove(args, 1)
    local handler = handlers[cmd]
    if handler == nil then
        return "ERR unknown " .. cmd
    end
    if not emu then
        return "ERR no-game"
    end
    local ok, reply = pcall(handler, args, line)
    if not ok then
        return "ERR internal " .. tostring(reply):gsub("[\r\n]", " ")
    end
    return reply
end

-- Runs only when the socket is readable (mGBA's poll checks select(0) first). A readable socket
-- that yields no bytes on the first read has been closed by the peer: 0.10.5's receive() then
-- returns nil plus a stale error (often AGAIN) rather than "disconnected", so the first empty read
-- counts as end of stream.
local function on_client_received()
    local first = true
    while client ~= nil do
        local data, err = client:receive(MAX_LINE)
        if data == nil or data == "" then
            if first or (err ~= nil and err ~= socket.ERRORS.AGAIN) then
                drop_client("end of stream (" .. tostring(err) .. ")")
            end
            break
        end
        first = false
        inbuf = inbuf .. data
        while client ~= nil do
            local nl = inbuf:find("\n", 1, true)
            if nl == nil then
                break
            end
            local line = inbuf:sub(1, nl - 1):gsub("\r$", "")
            inbuf = inbuf:sub(nl + 1)
            local reply = handle_line(line)
            if reply ~= nil then
                send_line(reply)
            end
            if reply == "OK BYE" then
                drop_client("BYE")
            end
        end
        if #inbuf > MAX_LINE then
            drop_client("line too long")
        end
    end
end

local function on_accept()
    local sock, err = server:accept()
    if sock == nil then
        warn("accept failed: " .. tostring(err))
        return
    end
    if client ~= nil then
        sock:send("ERR busy\n")
        close_socket(sock)
        warn("refused a second client")
        return
    end
    client = sock
    inbuf = ""
    client:add("received", on_client_received)
    client:add("error", function() drop_client("socket error") end)
    log("client connected")
end

local function on_keys_read()
    if client == nil or not emu then
        return
    end
    local cur = emu:currentFrame()
    apply_due(cur)
    if held ~= nil then
        emu:setKeys(held)
    end
    read_frame, read_keys = cur, emu:getKeys()
end

-- Blocks up to 1 ms for data on the client, through the native socket's select() that 0.10.5's
-- own hasdata() uses (with timeout 0). Without it the stall loop below busy-polls.
local function wait_readable(sock)
    local ok, native = pcall(rawget, sock, "_s")
    if ok and native ~= nil then
        pcall(function() native:select(1) end)
    end
end

-- Paced mode: hold the emulator in its frame callback until the client grants a later frame.
-- The loop waits on the client socket 1 ms at a time; a GRANT past the current frame, the stall
-- limit or a disconnect ends it.
local function on_frame()
    if CONFIG.mode ~= "paced" or grant == nil or client == nil or not emu then
        return
    end
    if emu:currentFrame() < grant then
        return
    end
    local start = now_s()
    while client ~= nil and grant ~= nil and emu:currentFrame() >= grant do
        wait_readable(client)
        if server ~= nil then
            server:poll()
        end
        if client ~= nil then
            client:poll()
        end
        if CONFIG.stall_limit_s > 0 and start ~= nil and now_s() - start > CONFIG.stall_limit_s then
            warn("no GRANT for " .. CONFIG.stall_limit_s .. " s; dropping the grant and running free")
            grant = nil
        end
    end
end

local function start()
    for p = CONFIG.port, CONFIG.port + CONFIG.port_tries - 1 do
        local sock, err = socket.bind(CONFIG.host, p)
        if sock ~= nil then
            local _, lerr = sock:listen(1)
            if lerr == nil then
                server, port = sock, p
                break
            end
            close_socket(sock)
            err = lerr
        end
        if err ~= socket.ERRORS.ADDRESS_IN_USE then
            console:error("[mgba-link] cannot listen on " .. CONFIG.host .. ":" .. p .. ": " .. tostring(err))
            return
        end
    end
    if server == nil then
        console:error("[mgba-link] no free port in " .. CONFIG.port .. ".." .. (CONFIG.port + CONFIG.port_tries - 1))
        return
    end
    server:add("received", on_accept)
    callbacks:add("keysRead", on_keys_read)
    callbacks:add("frame", on_frame)
    log("MgbaLink v" .. PROTOCOL_VERSION .. " " .. BRIDGE_VERSION .. " listening on "
        .. CONFIG.host .. ":" .. port .. " (mode " .. CONFIG.mode .. ")")
end

start()
