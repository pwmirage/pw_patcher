OBJECTS = common.o serializer.o chain_arr.o pw_elements.o cjson.o idmap.o pw_npc.o pw_tasks.o pw_tasks_npc.o
ALL_OBJECTS := $(OBJECTS) export.o srv_patcher.o idmap_gen.o
CFLAGS := -O0 -g -MD -MP -fno-strict-aliasing -Wall -Wno-format-truncation $(CFLAGS)

ifeq ($(OS),Windows_NT)
	ALL_OBJECTS := $(ALL_OBJECTS) gui.o client_patcher.o client_launcher.o updater.o sha1.o
endif

$(shell mkdir -p build &>/dev/null)

all: build/export build/srv_patcher
client: build/client_patcher build/updater build/client_launcher

clean:
	rm -f $(ALL_OBJECTS:%.o=build/%.o) $(ALL_OBJECTS:%.o=build/%.d)

build/export: $(OBJECTS:%.o=build/%.o) build/export.o
	gcc $(CFLAGS) -o $@ $^

build/srv_patcher: $(OBJECTS:%.o=build/%.o) build/srv_patcher.o
	gcc $(CFLAGS) -o $@ $^

build/idmap_gen: $(OBJECTS:%.o=build/%.o) build/idmap_gen.o
	gcc $(CFLAGS) -o $@ $^

build/client_patcher: $(OBJECTS:%.o=build/%.o) build/client_patcher.o
	windres -i patcher.rc -o patcher_rc.o
	gcc $(CFLAGS) -o $@ $^ patcher_rc.o -s -lwininet -mwindows -lcrypt32 -Wl,-Bstatic -liconv -Wl,-Bdynamic

build/client_launcher: build/cjson.o build/common.o build/sha1.o build/gui.o build/client_launcher.o
	windres -i launcher.rc -o launcher_rc.o
	gcc $(CFLAGS) -o $@ $^ launcher_rc.o -s -Wl,--subsystem,windows -lwininet -mwindows -lcrypt32 -Wl,-Bstatic -liconv -Wl,-Bdynamic


build/updater: build/updater.o
	windres -i updater.rc -o updater_rc.o
	gcc $(CFLAGS) -o $@ $^ updater_rc.o -s -Wl,--subsystem,windows -lwininet -Wno-address-of-packed-member

build/%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

-include $(ALL_OBJECTS:%.o=build/%.d)
