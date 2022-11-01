--Example lua plugin...
local function pp(...)
    print("MAGIC", ...)
end

local access_codes

function k_ontick(nows, nowns, nexts, nextns)
    pp("tickle: ", nowns, nowns, nexts, nextns)
end

function k_onmsg(client, topic, plen)
    pp(string.format("on message: cu: %s, cid: %s, caddr: %s, topic: %s payload len: %d",
    	client:username(), client:id(), client:address(), topic, plen))
end

function k_onacl_check(client, access, topic, qos, retain)
    pp(string.format("on acl_check: cu: %s, cid: %s, caddr: %s, access: %s, topic: %s qos: %d",
    	client:username(), client:id(), client:address(), access_codes[access], topic, qos))
    if topic:find("good") then
	    return true
    end
    if topic:find("bad") then
	    return false, plug.ERR_ACL_DENIED
    end
    return false, plug.ERR_PLUGIN_DEFER
end

function k_on_basic_auth(client, user, pass)
	pp(string.format("on basic auth: cu: %s ci: %s caddr: %s, user: %s, pass: %s",
		client:username(), client:id(), client:address(), user, pass))
	if user == "badman" then
		return false
	end
	if user == "karl" then
		return true
	end
	pp("Deferring...")
	plug.publish(nil, "luaplug/log/basic_auth", string.format("deferring auth for %s:%s", user, pass)) 
	return nil, plug.ERR_PLUGIN_DEFER
end

function k_unhandled()
	pp("never called, not implemented yet")
end


function init()
    -- setup whatever you need 
    plug.log(plug.LOG_WARNING, "this goes to mosquitto_log_printf with appropriate levels")
    -- the c api could provide these for us too, if we were excited.
    access_codes = {
	[plug.ACL_SUBSCRIBE] = "SUBSCRIBE",
	[plug.ACL_READ] = "READ",
	[plug.ACL_WRITE] = "WRITE",
	}
    --plug.register(plug.EVT_MESSAGE, k_onmsg)
    --plug.register(plug.EVT_TICK, k_ontick)
    --plug.register(plug.EVT_ACL_CHECK, k_onacl_check)
    plug.register(plug.EVT_BASIC_AUTH, k_on_basic_auth)
    --plug.register(plug.EVT_CONTROL, k_unhandled)

end

function cleanup()
    -- tear down whatever you need
    pp("Exiting lua plugin")
end

