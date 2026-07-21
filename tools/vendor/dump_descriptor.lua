-- One-time port helper: load an xpkg descriptor (a `package = {...}` file)
-- in a sandbox env and print its table as JSON on stdout.
-- Usage: lua tools/vendor/dump_descriptor.lua <descriptor.lua>
local path = assert(arg[1], "usage: dump_descriptor.lua <descriptor.lua>")
local env = setmetatable({}, { __index = _G })
local chunk = assert(loadfile(path, "t", env))
chunk()
local pkg = assert(env.package, "descriptor did not assign `package`")

local function esc(s)
    s = s:gsub("\\", "\\\\"):gsub('"', '\\"')
    s = s:gsub("\n", "\\n"):gsub("\r", "\\r"):gsub("\t", "\\t")
    s = s:gsub("%c", function(c) return string.format("\\u%04x", c:byte()) end)
    return s
end

local function isarray(t)
    local n = 0
    for k in pairs(t) do
        if type(k) ~= "number" then return false end
        n = n + 1
    end
    for i = 1, n do if t[i] == nil then return false end end
    return true, n
end

local out = {}
local function emit(v)
    local tv = type(v)
    if tv == "string" then
        out[#out + 1] = '"' .. esc(v) .. '"'
    elseif tv == "number" or tv == "boolean" then
        out[#out + 1] = tostring(v)
    elseif tv == "table" then
        local arr, n = isarray(v)
        if arr then
            out[#out + 1] = "["
            for i = 1, n do
                if i > 1 then out[#out + 1] = "," end
                emit(v[i])
            end
            out[#out + 1] = "]"
        else
            out[#out + 1] = "{"
            local keys = {}
            for k in pairs(v) do keys[#keys + 1] = k end
            table.sort(keys, function(a, b) return tostring(a) < tostring(b) end)
            for i, k in ipairs(keys) do
                if i > 1 then out[#out + 1] = "," end
                out[#out + 1] = '"' .. esc(tostring(k)) .. '":'
                emit(v[k])
            end
            out[#out + 1] = "}"
        end
    else
        out[#out + 1] = "null"
    end
end
emit(pkg)
io.write(table.concat(out))
