-- test.lua
collectgarbage("setpause", 100)
collectgarbage("setstepmul", 10000)

---
local a = collectgarbage("count") * 1024
local b = collectgarbage("count") * 1024

-- calling twice as i've seen this recommended before
collectgarbage()
collectgarbage()
a = collectgarbage("count") * 1024
b = collectgarbage("count") * 1024
print("total start usage:", a);

-- duplicating as results seem to change after the first run of the above
collectgarbage()
collectgarbage()
a = collectgarbage("count") * 1024
b = collectgarbage("count") * 1024
print("total start usage:", a);

(function()
    local f = function() end
    for i = 0, 50000, 1 do
        -- local a = {}
        local c = Test.new()
        c.func = f
    end
end)()

-- same as above
collectgarbage()
collectgarbage()
b = collectgarbage("count") * 1024
print("total end usage", b);
collectgarbage()
collectgarbage()
b = collectgarbage("count") * 1024
print("total end usage", b);

print()
print("total growth: ", b - a);
