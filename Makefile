OBJECTS = common.o serializer.o chain_arr.o pw_elements.o cjson.o idmap.o pw_npc.o pw_tasks.o
ALL_OBJECTS := $(OBJECTS) export.o srv_patcher.o
CFLAGS := -O0 -g -MD -MP -fno-strict-aliasing -Wall -Wno-format-truncation $(CFLAGS)

ifeq ($(OS),Windows_NT)
	OBJECTS := $(OBJECTS) gui.o
	ALL_OBJECTS := $(ALL_OBJECTS) client_patcher.o updater.o sha1.o
	CFLAGS := $(CFLAGS)
endif

$(shell mkdir -p build &>/dev/null)

all: build/export build/srv_patcher
client: build/client_patcher build/updater

clean:
	rm -f $(ALL_OBJECTS:%.o=build/%.o) $(ALL_OBJECTS:%.o=build/%.d)

build/export: $(OBJECTS:%.o=build/%.o) build/export.o
	gcc $(CFLAGS) -o $@ $^

build/srv_patcher: $(OBJECTS:%.o=build/%.o) build/srv_patcher.o
	gcc $(CFLAGS) -o $@ $^

build/client_patcher: $(OBJECTS:%.o=build/%.o) build/sha1.o build/client_patcher.o
	windres -i res.rc -o resource.o
	gcc $(CFLAGS) -o $@ $^ resource.o -s -Wl,--subsystem,windows -lwininet -mwindows -lcrypt32 -Wl,-Bstatic -liconv -Wl,-Bdynamic

build/updater: build/updater.o
	windres -i updater.rc -o updater_rc.o
	gcc $(CFLAGS) -o $@ $^ updater_rc.o -s -Wl,--subsystem,windows -lwininet -Wno-address-of-packed-member

build/%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

-include $(ALL_OBJECTS:%.o=build/%.d)
