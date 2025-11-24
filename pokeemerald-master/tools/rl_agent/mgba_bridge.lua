-- Template mGBA Lua bridge (may require LuaSocket or emulator-specific APIs)
-- This is a starting point: adapt to your emulator's Lua environment.

-- Protocol: JSON lines over TCP. Responds with JSON containing `obs` (base64 raw bytes), `reward`, `done`, `info`.

local json = require('json')
local socket = require('socket') -- may not be available; adapt per emulator

local HOST = "127.0.0.1"
local PORT = 9999

local server = assert(socket.bind(HOST, PORT))
server:settimeout(0.01)
print(string.format("RL bridge listening on %s:%d", HOST, PORT))

-- Utility: read screen and RAM
local function get_screen_png_base64()
    -- mGBA has `screenshot()` in some builds or you can read VRAM / decode; placeholder returns empty
    return ""
end

local function read_ram_addresses()
    -- Example: read relevant RAM addresses (battle state, HP, etc.)
    return {}
end

while true do
    local client = server:accept()
    if client then
        client:settimeout(0.01)
        local line, err = client:receive()
        if line then
            -- parse JSON command
            local ok, req = pcall(function() return json.decode(line) end)
            if ok and req then
                local resp = {obs = get_screen_png_base64(), reward = 0, done = false, info = {}}
                if req.cmd == 'reset' then
                    -- implement soft reset in emulator or load state
                elseif req.cmd == 'step' then
                    -- press buttons (req.buttons is list) using emulator API
                    -- e.g., joypad.set({A=true, LEFT=true}) or similar
                end
                client:send(json.encode(resp) .. "\n")
            end
        end
        client:close()
    end
    emu.frameadvance()
end
