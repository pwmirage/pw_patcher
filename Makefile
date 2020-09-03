OBJECTS = export.o common.o pw_elements.o
CFLAGS := -O3 -MD -MP $(CFLAGS)

$(shell mkdir -p build &>/dev/null)

all: build/export

clean:
		rm -f $(OBJECTS:%.o=build/%.o) $(OBJECTS:%.o=build/%.d)

build/export: $(OBJECTS:%.o=build/%.o)
		gcc $(CFLAGS) -o $@ -s $^

build/%.o: %.c
		gcc $(CFLAGS) -c -o $@ $<

-include $(OBJECTS:%.o=build/%.d)
