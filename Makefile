
NAME = luaplug
MOSQUITTO_HOME ?= ../mosquitto

PLUG_CFLAGS = -Wall -g3
PLUG_CFLAGS += -I${MOSQUITTO_HOME}/include
PLUG_LDFLAGS = $(shell pkg-config --libs lua)

binary: ${NAME}.so

${NAME}.so: ${NAME}.c
	$(CC) ${PLUG_CFLAGS} ${PLUG_LDFLAGS} -fPIC -shared $< -o $@

clean:
	$(RM) *.o ${NAME}.so
