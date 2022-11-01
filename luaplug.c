// SPDX-License-Identifier: Artistic-2.0 OR BSD-2-Clause OR BSD-3-Clause OR ISC OR LGPL-2.0-or-later OR MIT OR WTFPL
// Karl Palsson, 2022 <karlp@tweak.au>

#include <stdio.h>
#include <string.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "mosquitto.h"
#include "mosquitto_broker.h"
#include "mosquitto_plugin.h"

struct plug_state_t {
	mosquitto_plugin_id_t *pid;
	lua_State *L;
	int on_reload;
	int on_acl_check;
	int on_basic_auth;
	int on_ext_auth_start;
	int on_ext_auth_continue;
	int on_control;
	int on_message;
	int on_psk_key;
	int on_tick;
	int on_disconnect;
};

static struct plug_state_t plug_state;

static int ml_log_simple(lua_State *L)
{
	int level = luaL_checkinteger(L, 1);
	const char *s = luaL_checkstring(L, 2);
	mosquitto_log_printf(level, s);
	return 0;
}

static int ml_client_address(lua_State *L)
{
	struct mosquitto *c = lua_touserdata(L, 1);
	const char *a = mosquitto_client_address(c);
	lua_pushstring(L, a);
	return 1;
}

static int ml_client_id(lua_State *L)
{
	struct mosquitto *c = lua_touserdata(L, 1);
	const char *a = mosquitto_client_id(c);
	lua_pushstring(L, a);
	return 1;
}

static int ml_client_username(lua_State *L)
{
	struct mosquitto *c = lua_touserdata(L, 1);
	const char *a = mosquitto_client_username(c);
	lua_pushstring(L, a);
	return 1;
}



static int ml_publish(lua_State *L)
{
	const char *cid = NULL;
	if (!lua_isnil(L, 1)) {
		cid = luaL_checkstring(L, 1);
	}
	const char *topic = luaL_checkstring(L, 2);
	size_t len_pl = 0;
	const void *pl = NULL;
	if (!lua_isnil(L, 3)) {
		pl = lua_tolstring(L, 3, &len_pl);
	}
	int qos = luaL_optinteger(L, 4, 0);
	bool retain = lua_toboolean(L, 5);
	// FIXME - properties?
	int rc = mosquitto_broker_publish_copy(cid, topic, len_pl, pl, qos, retain, NULL);
	if (rc != MOSQ_ERR_SUCCESS) {
		return luaL_error(L, "blah, strerror is in libmosquito?");
	}
	lua_pushboolean(L, true);
	return 1;
}

static int ml_callback_handler(int event, void *event_data, void *userdata)
{
	struct plug_state_t *ps = userdata;
	int code;
	switch(event) {
	case MOSQ_EVT_MESSAGE:
		lua_rawgeti(ps->L, LUA_REGISTRYINDEX, ps->on_message);
		struct mosquitto_evt_message *ev = event_data;
		// GROSS, how do we let lua modify the data?
		// for demo, just push a few constants
		lua_pushlightuserdata(ps->L, ev->client);
		lua_pushstring(ps->L, ev->topic);
		lua_pushnumber(ps->L, ev->payloadlen);
		lua_call(ps->L, 3, 0);
		return MOSQ_ERR_SUCCESS;
	case MOSQ_EVT_TICK:
		lua_rawgeti(ps->L, LUA_REGISTRYINDEX, ps->on_tick);
		// mosquitto just sets this all to zero, but... one day maybe...
		struct mosquitto_evt_tick *evt = event_data;
		lua_pushnumber(ps->L, evt->now_s);
		lua_pushnumber(ps->L, evt->now_ns);
		lua_pushnumber(ps->L, evt->next_s);
		lua_pushnumber(ps->L, evt->next_ns);
		lua_call(ps->L, 4, 0);
		return MOSQ_ERR_SUCCESS;
	case MOSQ_EVT_ACL_CHECK:
		lua_rawgeti(ps->L, LUA_REGISTRYINDEX, ps->on_acl_check);
		struct mosquitto_evt_acl_check *ev_acl = event_data;
		lua_pushlightuserdata(ps->L, ev_acl->client);
		lua_pushnumber(ps->L, ev_acl->access);
		lua_pushstring(ps->L, ev_acl->topic);
		lua_pushnumber(ps->L, ev_acl->qos);
		lua_pushboolean(ps->L, ev_acl->retain);
		// FIXME - v5 properties are glossed over again...
		lua_call(ps->L, 5, LUA_MULTRET);
		if (lua_gettop(ps->L) == 1) {
			bool rv = lua_toboolean(ps->L, -1);
			lua_pop(ps->L, 1);
			return rv ? MOSQ_ERR_SUCCESS :  MOSQ_ERR_ACL_DENIED;
		}
		// assume normal "nil, code" multi return style
		code = lua_tonumber(ps->L, -1);
		lua_settop(ps->L, 0);
		return code;

	case MOSQ_EVT_BASIC_AUTH:
		lua_rawgeti(ps->L, LUA_REGISTRYINDEX, ps->on_basic_auth);
		struct mosquitto_evt_basic_auth *ev_ba = event_data;
		lua_pushlightuserdata(ps->L, ev_ba->client);
		lua_pushstring(ps->L, ev_ba->username);
		lua_pushstring(ps->L, ev_ba->password);
		lua_call(ps->L, 3, 1);
		if (lua_gettop(ps->L) == 1) {
			bool rv = lua_toboolean(ps->L, -1);
			lua_pop(ps->L, 1);
			return rv ? MOSQ_ERR_SUCCESS :  MOSQ_ERR_AUTH;
		}
		// assume normal "nil, code" multi return style
		code = lua_tonumber(ps->L, -1);
		lua_settop(ps->L, 0);
		return code;


	default:
		break;
	}
	mosquitto_log_printf(MOSQ_LOG_ERR, "haven't implemented the rest!");
	return MOSQ_ERR_NOT_SUPPORTED; // I think it's meant to return it to me?
}


static int ml_register_cb(lua_State *L)
{
	// TODO - do I have a context here? or did I register these as globals?
	int event = luaL_checkinteger(L, 1);
	if (!lua_isfunction(L, 2)) {
		return luaL_argerror(L, 2, "Expected a callback function");
	}
	// pop and save the function
	int ref = luaL_ref(L, LUA_REGISTRYINDEX);
	switch(event) {
	case MOSQ_EVT_MESSAGE:
		plug_state.on_message = ref;
		break;
	case MOSQ_EVT_TICK:
		plug_state.on_tick = ref;
		break;
	case MOSQ_EVT_ACL_CHECK:
		plug_state.on_acl_check = ref;
		break;
	case MOSQ_EVT_BASIC_AUTH:
		plug_state.on_basic_auth = ref;
		break;
	default:
		return luaL_argerror(L, 1, "Unimplemented support for this callback!");
	}

	int rc = mosquitto_callback_register(plug_state.pid, event, ml_callback_handler, NULL, &plug_state);
	if (rc != MOSQ_ERR_SUCCESS) {
		luaL_error(L, "Failed to register a plugin: %d %s", rc, "strerr is not in plugins :(");
	}
	return 0;
}

static int ml_unregister_cb(lua_State *L)
{
	luaL_error(L, "unregistering callbacks is unimplemented!");
	return 1;
}

struct define {
	const char* name;
	int val;
};
static const struct define D[] = {
	{"ACL_SUBSCRIBE", MOSQ_ACL_SUBSCRIBE},
	{"ACL_READ", MOSQ_ACL_READ},
	{"ACL_WRITE", MOSQ_ACL_WRITE},

	{"LOG_DEBUG", MOSQ_LOG_DEBUG},
	{"LOG_INFO", MOSQ_LOG_INFO},
	{"LOG_NOTICE", MOSQ_LOG_NOTICE},
	{"LOG_WARNING", MOSQ_LOG_WARNING},
	{"LOG_ERR", MOSQ_LOG_ERR},

	{"EVT_RELOAD", MOSQ_EVT_RELOAD},
	{"EVT_ACL_CHECK", MOSQ_EVT_ACL_CHECK},
	{"EVT_BASIC_AUTH", MOSQ_EVT_BASIC_AUTH},
	{"EVT_EXT_AUTH_START", MOSQ_EVT_EXT_AUTH_START},
	{"EVT_EXT_AUTH_CONTINUE", MOSQ_EVT_EXT_AUTH_CONTINUE},
	{"EVT_CONTROL", MOSQ_EVT_CONTROL},
	{"EVT_MESSAGE", MOSQ_EVT_MESSAGE},
	{"EVT_PSK_KEY", MOSQ_EVT_PSK_KEY},
	{"EVT_TICK", MOSQ_EVT_TICK},
	{"EVT_DISCONNECT", MOSQ_EVT_DISCONNECT},

	{"ERR_ACL_DENIED", MOSQ_ERR_ACL_DENIED},
	{"ERR_AUTH", MOSQ_ERR_AUTH},
	{"ERR_PLUGIN_DEFER", MOSQ_ERR_PLUGIN_DEFER},
	{"ERR_UNKNOWN", MOSQ_ERR_UNKNOWN},

	{NULL, 0},
};

static const struct luaL_Reg R[] = {
	// TODO FIXME - these should ideally be metatable methods on the client?
	{"client_address", ml_client_address},
//	{"client_clean_session", ml_client_clean_session},
	{"client_id", ml_client_id},
//	{"client_keepalive", ml_client_keepalive},
//	{"client_certificate", ml_client_certificate},
//	{"client_protocol", ml_client_protocol},
//	{"client_protocol_version", ml_client_protocol_version},
//	{"client_sub_count", ml_client_sub_count},
	{"client_username", ml_client_username},

	{"publish", ml_publish},
	{"log", ml_log_simple},
	{"register", ml_register_cb},
	{"unregister", ml_unregister_cb},
	{NULL,		NULL}
};



// internal machinery below here...
static void lp_register_defs(lua_State *L, const struct define *D)
{
	while (D->name != NULL) {
		lua_pushinteger(L, D->val);
		lua_setfield(L, -2, D->name);
		D++;
	}
}

static int lp_open(struct plug_state_t *ps, char *fn)
{
	ps->L = luaL_newstate();
	ps->on_message = LUA_REFNIL;
	/// FIXME - fill in the rest!
	if (!ps->L) {
		mosquitto_log_printf(MOSQ_LOG_ERR, "Failed to create a lua state!");
		return MOSQ_ERR_UNKNOWN; // nothing else looks good
	}
	luaL_openlibs(ps->L);
	if (luaL_loadfile(ps->L, fn)) {
		mosquitto_log_printf(MOSQ_LOG_ERR, "Failed to load plugin file %s: %s", fn, lua_tostring(ps->L, -1));
		lua_settop(ps->L, 0);
		return MOSQ_ERR_UNKNOWN;
	}
	if (lua_pcall(ps->L, 0, LUA_MULTRET, 0)) {
		printf("Preparing file failed: %s\n", lua_tostring(ps->L, -1));
		return MOSQ_ERR_UNKNOWN;
	}
	luaL_newlib(ps->L, R);
	lp_register_defs(ps->L, D);
	lua_setglobal(ps->L, "plug"); // We _could_ make this a config option...

	int t = lua_getglobal(ps->L, "init");
	if (t != LUA_TFUNCTION) {
		mosquitto_log_printf(MOSQ_LOG_ERR, "Plugin has no init? how will it do anything? %s", fn);
		lua_settop(ps->L, 0);
		return MOSQ_ERR_UNKNOWN;
	}
	if (lua_pcall(ps->L, 0, 0, 0)) {
		printf("Plugin init failed: %s\n", lua_tostring(ps->L, -1));
		return MOSQ_ERR_UNKNOWN;
	}

	return MOSQ_ERR_SUCCESS;
}

// in an ideal world, lua plugins could access this, but it
// happens before plugin init, so, no. not important right now.
int mosquitto_plugin_version(int count, const int *versions)
{
	for (int i = 0; i < count; i++) {
		//printf("KARL: broker advertises plugin api: %d\n", versions[i]);
	}
	return 5;
}


int mosquitto_plugin_init(mosquitto_plugin_id_t *identifier, void **userdata, struct mosquitto_opt *options, int option_count)
{
	mosquitto_log_printf(MOSQ_LOG_DEBUG, "KARL: plugin init");
	plug_state.pid = identifier;
	*userdata = &plug_state;
	for (int i = 0; i < option_count; i++) {
		struct mosquitto_opt opt = options[i];
		// Can I rely on mosquitto not giving me unbounded strings?
		if (strcmp(opt.key, "plug") == 0) {
			mosquitto_log_printf(MOSQ_LOG_INFO, "loading lua plugin: %s", opt.value);
			return lp_open(&plug_state, opt.value);
		}
	}
	mosquitto_log_printf(MOSQ_LOG_ERR, "plugin_opt_plug specifying file name is required!");
	return MOSQ_ERR_INVAL;
}

int mosquitto_plugin_cleanup(void *userdata, struct mosquitto_opt *options, int option_count)
{
	struct plug_state_t *ps = userdata;
	if (ps->L) {
		int t = lua_getglobal(ps->L, "cleanup");
		if (t == LUA_TFUNCTION) {
			if (lua_pcall(ps->L, 0, 0, 0)) {
				mosquitto_log_printf(MOSQ_LOG_ERR, "plugin cleanup failed: %s", lua_tostring(ps->L, -1));
			}
		}
		lua_close(ps->L);
	}
	return MOSQ_ERR_SUCCESS;
}
