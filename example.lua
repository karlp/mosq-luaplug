--Example lua plugin...
local function pp(...)
    print("MAGIC", ...)
end

function k_ontick(nows, nowns, nexts, nextns)
    pp("tickle: ", nowns, nowns, nexts, nextns)
end

function k_onmsg(cid, topic, plen)
    pp("on message: ", cid, topic, plen)
end

function init()
    -- setup whatever you need 
    plug.log(plug.LOG_WARNING, "this goes to mosquitto_log_printf with appropriate levels")
    plug.register(plug.EVT_MESSAGE, k_onmsg)
    plug.register(plug.EVT_TICK, k_ontick)

end

function cleanup()
    -- tear down whatever you need
end

