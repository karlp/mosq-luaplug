# luaplug: mosquitto broker plugins, in lua.

Do you hate writing C?  Do you yearn for the simplicity of 1 based counting
and the dichotomy of arrays/tables that lua can give you?

Do you want to do insane things inside the broker, instead of via a separate process?
Are you _really_ excited about runtime errors bringing down your entire broker?!

You're in luck!

# Usage
Goals are to be "least surprise" when compared to the [mosquitto broker api](https://mosquitto.org/api/files/mosquitto_broker-h.html)
and [mosquitto plugin api](https://mosquitto.org/api/files/mosquitto_plugin-h.html).

In mosquitto.conf

```
plugin /path/to/luaplug.so
plugin_opt_plug /path/to/your-plugin.lua
```

In your plugin (this is attached as [example.lua](example.lua))
```lua

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
```


# Example output

```
$ mosquitto -v -c kt.conf 
1667317718: mosquitto version 2.0.15 starting
1667317718: Config loaded from kt.conf.
1667317718: Loading plugin: /home/karlp/src/l4-mosquitto-plugin/luaplug.so
1667317718: KARL: plugin init
1667317718: loading lua plugin: /home/karlp/src/l4-mosquitto-plugin/example.lua
1667317718: this goes to mosquitto_log_printf with appropriate levels
1667317718: Opening ipv4 listen socket on port 1883.
1667317718: Opening ipv6 listen socket on port 1883.
1667317718: mosquitto version 2.0.15 running
MAGIC	tickle: 	0.0	0.0	0.0	0.0
MAGIC	tickle: 	0.0	0.0	0.0	0.0
MAGIC	tickle: 	0.0	0.0	0.0	0.0
MAGIC	tickle: 	0.0	0.0	0.0	0.0
MAGIC	tickle: 	0.0	0.0	0.0	0.0
MAGIC	tickle: 	0.0	0.0	0.0	0.0
1667317721: New connection from ::1:58262 on port 1883.
MAGIC	tickle: 	0.0	0.0	0.0	0.0
1667317721: New client connected from ::1:58262 as auto-85B272FE-27F0-2B32-4E5D-CEBB7FF1F6D9 (p2, c1, k60).
1667317721: No will message specified.
1667317721: Sending CONNACK to auto-85B272FE-27F0-2B32-4E5D-CEBB7FF1F6D9 (0, 0)
MAGIC	tickle: 	0.0	0.0	0.0	0.0
1667317721: Received PUBLISH from auto-85B272FE-27F0-2B32-4E5D-CEBB7FF1F6D9 (d0, q0, r0, m0, 'hello karl', ... (4 bytes))
MAGIC	on message: 	auto-85B272FE-27F0-2B32-4E5D-CEBB7FF1F6D9	hello karl	4.0
MAGIC	tickle: 	0.0	0.0	0.0	0.0
1667317721: Received DISCONNECT from auto-85B272FE-27F0-2B32-4E5D-CEBB7FF1F6D9
1667317721: Client auto-85B272FE-27F0-2B32-4E5D-CEBB7FF1F6D9 disconnected.
MAGIC	tickle: 	0.0	0.0	0.0	0.0
MAGIC	tickle: 	0.0	0.0	0.0	0.0
MAGIC	tickle: 	0.0	0.0	0.0	0.0
MAGIC	tickle: 	0.0	0.0	0.0	0.0

```
