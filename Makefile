OBJECTS = common.o pw_elements.o cjson.o idmap.o
ALL_OBJECTS := $(OBJECTS) export.o import.o
CFLAGS := -O0 -g -MD -MP -fno-strict-aliasing -Wall -Wno-format-truncation $(CFLAGS)

$(shell mkdir -p build &>/dev/null)

all: build/export build/import

clean:
	rm -f $(ALL_OBJECTS:%.o=build/%.o) $(ALL_OBJECTS:%.o=build/%.d)

build/export: $(OBJECTS:%.o=build/%.o) export.o
	gcc $(CFLAGS) -o $@ $^

build/import: $(OBJECTS:%.o=build/%.o) import.o
	gcc $(CFLAGS) -o $@ $^

build/%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

-include $(OBJECTS:%.o=build/%.d)
