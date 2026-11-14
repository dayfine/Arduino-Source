-- Unit tests for bridge.lua against stub mGBA globals (no mGBA, no ROM).
-- Run: lua lab/mgba/bridge_test.lua   (Lua 5.4; prints "ok N tests" and exits 0)

local here = arg[0]:match("^(.*)/[^/]*$") or "."

local function new_world(config, busy_ports)
    local w = { busy_ports = busy_ports, frame = 100, keys = 0, sent = {}, inbound = {}, cbs = {}, states = {},
                shots = 0, resets = 0, t = 0, polls = 0 }

    local client = { cbs = {} }
    function client:add(ev, fn) self.cbs[ev] = fn; return 1 end
    function client:send(data, from)
        from = from or 1
        w.sent[#w.sent + 1] = data:sub(from)
        return #data
    end
    function client:receive(_)
        if #w.inbound == 0 then
            return nil, "AGAIN"
        end
        return table.remove(w.inbound, 1)
    end
    function client:poll()
        w.polls = w.polls + 1
        if w.on_poll then
            w.on_poll(w)
        end
        if #w.inbound > 0 then
            self.cbs.received()
        end
    end
    function client:close() w.closed = true end
    client._s = { select = function(_, ms) w.selects = (w.selects or 0) + 1; w.select_ms = ms; return 0 end }
    w.client = client

    local server = {}
    function server:listen(_) return 0 end
    function server:add(ev, fn) w.accept = fn; return 1 end
    function server:accept()
        if w.next_accept ~= nil then
            local s = w.next_accept
            w.next_accept = nil
            return s
        end
        return client
    end
    function server:poll()
        w.server_polls = (w.server_polls or 0) + 1
        if w.on_server_poll then
            w.on_server_poll(w)
        end
    end
    function server:close() end

    socket = {
        ERRORS = { AGAIN = "AGAIN", ADDRESS_IN_USE = "ADDRESS_IN_USE" },
        bind = function(host, p)
            if w.busy_ports and w.busy_ports[p] then
                return nil, "ADDRESS_IN_USE"
            end
            w.bound = host .. ":" .. p
            return server
        end,
    }
    callbacks = { add = function(_, ev, fn) w.cbs[ev] = fn; return 1 end }
    console = {
        log = function(_, m) w.last_log = m end,
        warn = function(_, m) w.last_warn = m end,
        error = function(_, m) w.last_error = m end,
    }
    emu = {}
    function emu:currentFrame() return w.frame end
    function emu:getKeys() return w.keys end
    function emu:setKeys(k) w.keys = k end
    function emu:getGameCode() return "AGB-BPRE" end
    function emu:screenshot(_) w.shots = w.shots + 1 end
    function emu:saveStateFile(path, flags)
        w.save_flags = flags
        w.states[path] = w.frame
        return true
    end
    function emu:loadStateFile(path, flags)
        w.load_flags = flags
        if w.states[path] == nil then
            return false
        end
        w.frame = w.states[path]
        return true
    end
    function emu:reset() w.resets = w.resets + 1; w.frame = 0 end

    config = config or {}
    config.clock = config.clock or function() return w.t end
    config.frame_path = config.frame_path or "/tmp/test-frame.png"
    MGBA_BRIDGE_CONFIG = config
    dofile(here .. "/bridge.lua")
    w.accept()
    return w
end

-- Sends one line and returns the bridge's reply (strips the newline).
local function cmd(w, line)
    w.sent = {}
    w.inbound[#w.inbound + 1] = line .. "\n"
    w.client.cbs.received()
    assert(#w.sent == 1, "expected one reply to " .. line .. ", got " .. #w.sent)
    return (w.sent[1]:gsub("\n$", ""))
end

-- One emulated frame: keysRead during the frame, then the frame callback after it.
local function step(w)
    w.cbs.keysRead()
    w.frame = w.frame + 1
    w.cbs.frame()
end

local tests = {}

function tests.hello_reports_game_code_and_mode()
    local w = new_world()
    assert(cmd(w, "HELLO 1") == "OK 1 BPRE bridge-0.1 paced", cmd(w, "HELLO 1"))
    assert(cmd(w, "HELLO 2") == "ERR version 1")
    assert(w.bound == "127.0.0.1:47801")
end

function tests.next_port_when_busy()
    local w = new_world({ port = 5000 }, { [5000] = true, [5001] = true })
    assert(w.bound == "127.0.0.1:5002", w.bound)
    assert(cmd(w, "FRAME") == "FRAME 100 240 160 /tmp/test-frame.png")
end

function tests.framing_partial_and_crlf()
    local w = new_world()
    w.sent = {}
    w.inbound = { "HEL", "LO 1\r\nSTA", "TUS\n" }
    w.client.cbs.received()
    assert(#w.sent == 2, "replies: " .. #w.sent)
    assert(w.sent[1]:match("^OK 1 BPRE"))
    assert(w.sent[2]:match("^OK frame=100 "))
end

function tests.unknown_and_bad_args()
    local w = new_world()
    assert(cmd(w, "FOO") == "ERR unknown FOO")
    assert(cmd(w, "KEYS 5"):match("^ERR usage KEYS"))
    assert(cmd(w, "KEYS 5 1024"):match("^ERR usage KEYS"))
    assert(cmd(w, "GRANT x"):match("^ERR usage GRANT"))
    assert(cmd(w, "SAVE"):match("^ERR usage SAVE"))
end

function tests.keys_apply_from_their_frame()
    local w = new_world()
    assert(cmd(w, "KEYS 103 2") == "OK")
    step(w) -- frame 100
    step(w) -- 101
    step(w) -- 102
    assert(w.keys == 0, "held too early: " .. w.keys)
    step(w) -- 103: applied at keysRead
    assert(w.keys == 2)
    local s = cmd(w, "STATUS")
    assert(s:match(" applied_frame=103 applied_mask=2 "), s)
    assert(s:match(" read_frame=103 read_keys=2 "), s)
    -- the bridge re-asserts the mask every keysRead, over mGBA's own input
    w.keys = 0
    step(w)
    assert(w.keys == 2)
end

function tests.late_keys_apply_at_once()
    local w = new_world()
    w.frame = 200
    assert(cmd(w, "KEYS 197 1") == "LATE 3")
    assert(w.keys == 1)
    assert(cmd(w, "STATUS"):match(" applied_frame=200 applied_mask=1 "))
end

function tests.keys_out_of_order_are_sorted()
    local w = new_world()
    cmd(w, "KEYS 102 4")
    cmd(w, "KEYS 101 8")
    step(w) -- 100
    step(w) -- 101
    assert(w.keys == 8)
    step(w) -- 102
    assert(w.keys == 4)
end

function tests.grant_stalls_until_next_grant()
    local w = new_world()
    assert(cmd(w, "GRANT 102") == "OK 100")
    step(w) -- -> 101, no stall
    assert(w.polls == 0)
    -- the frame that reaches 102 stalls; the stub delivers the next GRANT after 3 polls
    w.on_poll = function(ww)
        if ww.polls == 3 then
            ww.inbound[#ww.inbound + 1] = "GRANT 104\n"
        end
    end
    step(w) -- -> 102, stalls
    assert(w.polls == 3, "polls " .. w.polls)
    assert(w.frame == 102)
end

function tests.stall_limit_releases_grant()
    local w = new_world({ stall_limit_s = 2 })
    cmd(w, "GRANT 101")
    w.on_poll = function(ww) ww.t = ww.t + 1 end
    step(w) -- -> 101, stalls until the clock passes 2 s
    assert(w.polls == 3, "polls " .. w.polls)
    assert(cmd(w, "STATUS"):match(" grant=%- "))
    assert(w.last_warn:match("no GRANT"))
end

function tests.free_mode_ignores_grant()
    local w = new_world({ mode = "free" })
    assert(cmd(w, "HELLO 1"):match(" free$"))
    cmd(w, "GRANT 100")
    step(w)
    assert(w.polls == 0)
end

function tests.save_load_round_trip_with_all_flags()
    local w = new_world()
    assert(cmd(w, "SAVE /tmp/x y.ss") == "OK 100")
    assert(w.save_flags == 31)
    step(w); step(w)
    cmd(w, "GRANT 500")
    cmd(w, "KEYS 400 1")
    assert(cmd(w, "LOAD /tmp/x y.ss") == "OK 100")
    assert(w.load_flags == 31)
    local s = cmd(w, "STATUS")
    assert(s:match(" grant=100 "), s) -- holds at the loaded frame
    cmd(w, "GRANT 200")
    step(w); step(w)
    assert(w.keys == 0, "pending KEYS survived LOAD")
    assert(cmd(w, "LOAD /nope") == "ERR load failed")
end

function tests.frame_reports_number_size_and_path()
    local w = new_world()
    assert(cmd(w, "FRAME") == "FRAME 100 240 160 /tmp/test-frame.png")
    assert(w.shots == 1)
end

function tests.disconnect_releases_keys_and_grant()
    local w = new_world()
    cmd(w, "KEYS 99 3")
    cmd(w, "GRANT 100")
    assert(w.keys == 3)
    w.client.receive = function() return nil, "DISCONNECTED" end
    w.client.cbs.received()
    assert(w.keys == 0 and w.closed)
    step(w)
    assert(w.polls == 0)
end

function tests.eof_reported_as_again_drops_client()
    -- 0.10.5 returns nil plus a stale AGAIN when a readable socket hits end of stream
    local w = new_world()
    cmd(w, "KEYS 99 3")
    assert(w.keys == 3)
    w.client.cbs.received() -- readable, nothing to read
    assert(w.closed and w.keys == 0)
end

function tests.again_after_data_keeps_client()
    local w = new_world()
    cmd(w, "STATUS") -- data, then AGAIN in the same callback
    assert(not w.closed)
end

function tests.bye_replies_then_closes()
    local w = new_world()
    cmd(w, "GRANT 100")
    assert(cmd(w, "BYE") == "OK BYE")
    assert(w.closed)
    step(w)
    assert(w.polls == 0)
end

function tests.stall_waits_on_native_select()
    local w = new_world()
    cmd(w, "GRANT 101")
    w.on_poll = function(ww) ww.inbound[#ww.inbound + 1] = "GRANT 105\n" end
    step(w)
    assert(w.selects == 1 and w.select_ms == 1)
end

function tests.no_game_loaded()
    local w = new_world()
    emu = nil
    assert(cmd(w, "HELLO 1") == "ERR no-game")
end

function tests.reset_clears_pending_keys_and_holds_grant()
    local w = new_world()
    assert(cmd(w, "KEYS 400 1") == "OK")
    assert(cmd(w, "GRANT 500") == "OK 100")
    assert(cmd(w, "RESET") == "OK")
    assert(w.resets == 1 and w.frame == 0)
    local s = cmd(w, "STATUS")
    assert(s:match(" grant=0 "), s)
    assert(cmd(w, "GRANT 450") == "OK 0")
    w.frame = 400
    step(w)
    assert(w.keys == 0, "pending KEYS survived RESET")
end

local function new_rejected_client()
    local s = { sent = {}, closed = false }
    function s:send(data) s.sent[#s.sent + 1] = data; return #data end
    function s:close() s.closed = true end
    return s
end

function tests.second_client_gets_err_busy()
    local w = new_world()
    local second = new_rejected_client()
    w.next_accept = second
    w.accept()
    assert(#second.sent == 1 and second.sent[1] == "ERR busy\n")
    assert(second.closed)
    assert(w.last_warn:match("refused"))
    assert(cmd(w, "HELLO 1"):match("^OK 1 "))
end

function tests.second_client_during_stall_gets_err_busy()
    local w = new_world()
    local second = new_rejected_client()
    cmd(w, "GRANT 101")
    w.on_server_poll = function(ww)
        if ww.server_polls == 1 then
            ww.next_accept = second
            ww.accept()
        end
    end
    w.on_poll = function(ww)
        if ww.polls == 2 then
            ww.inbound[#ww.inbound + 1] = "GRANT 105\n"
        end
    end
    step(w)
    assert(w.server_polls >= 1)
    assert(#second.sent == 1 and second.sent[1] == "ERR busy\n")
    assert(second.closed)
    assert(cmd(w, "STATUS"):match(" grant=105 "))
end

function tests.handler_error_returns_err_internal()
    local w = new_world()
    emu.screenshot = function() error("boom\nline2") end
    local reply = cmd(w, "FRAME")
    assert(reply:match("^ERR internal "), reply)
    assert(not reply:find("[\r\n]"), reply)
    assert(reply:find("boom line2", 1, true), reply)
end

local names = {}
for name in pairs(tests) do
    names[#names + 1] = name
end
table.sort(names)
local failed = 0
for _, name in ipairs(names) do
    local ok, err = pcall(tests[name])
    if not ok then
        failed = failed + 1
        print("FAIL " .. name .. ": " .. tostring(err))
    end
end
if failed > 0 then
    print(failed .. " of " .. #names .. " failed")
    os.exit(1)
end
print("ok " .. #names .. " tests")
