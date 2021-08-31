OBJECTS = common.o serializer.o chain_arr.o pw_elements.o cjson.o idmap.o pw_npc.o pw_tasks.o pw_tasks_npc.o avl.o pw_item_desc.o
ALL_OBJECTS := $(OBJECTS) export.o srv_patcher.o idmap_gen.o mgpck.o zpipe.o extra_drops.o
_CFLAGS := -O3 -MD -MP -fno-strict-aliasing -Wall -Wno-format-truncation $(CFLAGS)

ifeq ($(OS),Windows_NT)
	ALL_OBJECTS := $(ALL_OBJECTS) gui.o client_patcher.o client_launcher.o client_launcher_settings.o updater.o sha1.o pw_pck.o game_config.o
endif

$(@shell mkdir -p build &>/dev/null)

.PHONY: clean all build/gcc_ver.h

all: build/export build/srv_patcher

clean:
	rm -f $(ALL_OBJECTS:%.o=build/%.o) $(ALL_OBJECTS:%.o=build/%.d) build/gcc_ver.h

build/gcc_ver.h:
	$(shell echo "#define BUILD_GCC_VER (\"$$(gcc --version | head -n1)\")\n#define BUILD_CFLAGS (\"$(_CFLAGS)\")" > build/gcc_ver_tmp.h)
	$(shell if ! cmp build/gcc_ver_tmp.h build/gcc_ver.h >/dev/null; then make clean; fi)
	cp build/gcc_ver_tmp.h build/gcc_ver.h

build/export: build/gcc_ver.h $(OBJECTS:%.o=build/%.o) build/export.o build/extra_drops.o
	gcc $(_CFLAGS) -o $@ -Wl,--whole-archive $^ -Wl,--no-whole-archive -Wl,-Bstatic -lz -Wl,-Bdynamic

build/srv_patcher: build/gcc_ver.h $(OBJECTS:%.o=build/%.o) build/srv_patcher.o
	gcc $(_CFLAGS) -o $@ -Wl,--whole-archive $^ -Wl,--no-whole-archive

build/idmap_gen: build/gcc_ver.h $(OBJECTS:%.o=build/%.o) build/idmap_gen.o
	gcc $(_CFLAGS) -o $@ -Wl,--whole-archive $^ -Wl,--no-whole-archive

build/client_patcher: build/gcc_ver.h $(OBJECTS:%.o=build/%.o) build/client_patcher.o build/pw_pck.o build/zpipe.o
	windres -i patcher.rc -o patcher_rc.o
	gcc $(_CFLAGS) -o $@ -Wl,--whole-archive $^ patcher_rc.o -Wl,--no-whole-archive -lwininet -mwindows -lcrypt32 -Wl,-Bstatic -lz -liconv -Wl,-Bdynamic

build/client_launcher: build/gcc_ver.h build/cjson.o build/common.o build/sha1.o build/gui.o build/client_launcher.o build/client_launcher_settings.o build/game_config.o build/avl.o
	windres -i launcher.rc -o launcher_rc.o
	gcc $(_CFLAGS) -o $@ -Wl,--whole-archive $^ launcher_rc.o -Wl,--no-whole-archive -Wl,--subsystem,windows -lwininet -mwindows -lcrypt32

build/updater: build/gcc_ver.h build/updater.o
	windres -i updater.rc -o updater_rc.o
	gcc $(_CFLAGS) -o $@ -Wl,--whole-archive $^ updater_rc.o -Wl,--no-whole-archive -Wl,--subsystem,windows -lwininet -Wno-address-of-packed-member

build/mgpck: build/gcc_ver.h build/cjson.o build/common.o build/mgpck.o build/pw_pck.o build/avl.o build/zpipe.o
	gcc $(_CFLAGS) -o $@ $^ -Wl,--subsystem,console -lwininet -Wl,-Bstatic -lz -liconv -Wl,-Bdynamic

build/%.o: %.c
	gcc $(_CFLAGS) -c -o $@ $<

-include $(ALL_OBJECTS:%.o=build/%.d)
