# luaplug mosquitto broker plugins, in lua.

Do you hate writing C?  Do you yearn for the simplicity of 1 based counting
and the dichotomy of arrays/tables that lua can give you?

Do you want to do insane things inside the broker, instead of via a separate process?

You're in luck!

# Usage
Goals are to be "least surprise" when compared to the [mosquitto broker api](https://mosquitto.org/api/files/mosquitto_broker-h.html)
and [mosquitto plugin api](https://mosquitto.org/api/files/mosquitto_plugin-h.html).

In mosquitto.conf

```
plugin /path/to/luaplug.so
plugin_opt_plug /path/to/your-plugin.lua
```

In your plugin
```lua

function init()
    -- setup whatever you need
    plug.log(plug.LOG_WARNING, "goes to mosquitto_log_printf(MOSQ_LOG_WARNING, ...)")
end

function cleanup()
    -- tear down whatever you need
end
```
