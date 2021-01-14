OBJECTS = common.o pw_elements.o cjson.o idmap.o
ALL_OBJECTS := $(OBJECTS) export.o srv_patcher.o
CFLAGS := -O3 -MD -MP -fno-strict-aliasing -Wall -Wno-format-truncation $(CFLAGS)

ifeq ($(OS),Windows_NT)
	OBJECTS := $(OBJECTS) gui.o
	ALL_OBJECTS := $(ALL_OBJECTS) client_patcher.o
	CFLAGS := $(CFLAGS)
endif

$(shell mkdir -p build &>/dev/null)

all: build/export build/srv_patcher
client: build/client_patcher

clean:
	rm -f $(ALL_OBJECTS:%.o=build/%.o) $(ALL_OBJECTS:%.o=build/%.d)

build/export: $(OBJECTS:%.o=build/%.o) export.o
	gcc $(CFLAGS) -o $@ $^

build/srv_patcher: $(OBJECTS:%.o=build/%.o) srv_patcher.o
	gcc $(CFLAGS) -o $@ $^

build/client_patcher: $(OBJECTS:%.o=build/%.o) client_patcher.o
	windres -i res.rc -o resource.o
	gcc $(CFLAGS) -o $@ $^ -s resource.o -Wl,--subsystem,windows -lwininet -lwininet -mwindows

build/%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

-include $(OBJECTS:%.o=build/%.d)
