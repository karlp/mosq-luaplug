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

static mosquitto_plugin_id_t *pid;

struct plug_state_t {
	mosquitto_plugin_id_t *pid;
	lua_State *L;
};

static struct plug_state_t plug_state;

static int ml_log_simple(lua_State *L)
{
	int level = luaL_checkinteger(L, 1);
	const char *s = luaL_checkstring(L, 2);
	mosquitto_log_printf(level, s);
	return 0;
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

struct define {
	const char* name;
	int val;
};
static const struct define D[] = {
	{"LOG_DEBUG", MOSQ_LOG_DEBUG},
	{"LOG_INFO", MOSQ_LOG_INFO},
	{"LOG_NOTICE", MOSQ_LOG_NOTICE},
	{"LOG_WARNING", MOSQ_LOG_WARNING},
	{"LOG_ERR", MOSQ_LOG_ERR},
	{NULL, 0},
};

static const struct luaL_Reg R[] = {
	{"broker_publish", ml_publish},
	{"log", ml_log_simple},
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
