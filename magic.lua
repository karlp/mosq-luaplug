

local function pp(...)
	print("MAGIC", ...)
end

function init(a)
	pp("init yay", a)
	pp("we can access our defines", plug.LOG_INFO, plug.LOG_WARNING)
	plug.log(plug.LOG_WARNING, "we can call functions!")
end

function close()
	pp("exiting with broker cleanup")
end
