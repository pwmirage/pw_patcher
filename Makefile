OBJECTS = common.o serializer.o chain_arr.o pw_elements.o cjson.o idmap.o pw_npc.o pw_tasks.o pw_tasks_npc.o pw_pck.o avl.o zpipe.o
ALL_OBJECTS := $(OBJECTS) export.o srv_patcher.o idmap_gen.o
_CFLAGS := -O3 -MD -MP -fno-strict-aliasing -Wall -Wno-format-truncation $(CFLAGS)

ifeq ($(OS),Windows_NT)
	ALL_OBJECTS := $(ALL_OBJECTS) gui.o client_patcher.o client_launcher.o updater.o sha1.o
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

build/export: build/gcc_ver.h $(OBJECTS:%.o=build/%.o) build/export.o
	gcc $(_CFLAGS) -o $@ -Wl,--whole-archive $^ -Wl,--no-whole-archive -Wl,-Bstatic -lz -Wl,-Bdynamic

build/srv_patcher: build/gcc_ver.h $(OBJECTS:%.o=build/%.o) build/srv_patcher.o
	gcc $(_CFLAGS) -o $@ -Wl,--whole-archive $^ -Wl,--no-whole-archive

build/idmap_gen: build/gcc_ver.h $(OBJECTS:%.o=build/%.o) build/idmap_gen.o
	gcc $(_CFLAGS) -o $@ -Wl,--whole-archive $^ -Wl,--no-whole-archive

build/client_patcher: build/gcc_ver.h $(OBJECTS:%.o=build/%.o) build/client_patcher.o
	windres -i patcher.rc -o patcher_rc.o
	gcc $(_CFLAGS) -o $@ -Wl,--whole-archive $^ patcher_rc.o -Wl,--no-whole-archive -s -lwininet -mwindows -lcrypt32 -Wl,-Bstatic -liconv -Wl,-Bdynamic

build/client_launcher: build/gcc_ver.h build/cjson.o build/common.o build/sha1.o build/gui.o build/client_launcher.o
	windres -i launcher.rc -o launcher_rc.o
	gcc $(_CFLAGS) -o $@ -Wl,--whole-archive $^ launcher_rc.o -Wl,--no-whole-archive -s -Wl,--subsystem,windows -lwininet -mwindows -lcrypt32

build/updater: build/gcc_ver.h build/updater.o
	windres -i updater.rc -o updater_rc.o
	gcc $(_CFLAGS) -o $@ -Wl,--whole-archive $^ updater_rc.o -Wl,--no-whole-archive -s -Wl,--subsystem,windows -lwininet -Wno-address-of-packed-member

build/%.o: %.c
	gcc $(_CFLAGS) -c -o $@ $<

-include $(ALL_OBJECTS:%.o=build/%.d)
