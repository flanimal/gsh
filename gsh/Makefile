SRC := $(wildcard src/*.c)
OBJECTS := $(notdir $(SRC:.c=.o))

ERRORS	:= -Werror -Wall -Wextra -pedantic-errors
DEFINES := _GNU_SOURCE

CFLAGS := $(ERRORS) -std=gnu11 $(addprefix -D, $(DEFINES))

.PHONY : all debug clean reset

all : bin/gsh

debug : CFLAGS += -O0 -fno-omit-frame-pointer -g
debug : bin/debug/gsh

bin/debug/gsh : $(addprefix obj/debug/, $(OBJECTS))
	mkdir -p $(@D)
	$(CC) -o $@ $^

bin/gsh : $(addprefix obj/, $(OBJECTS))
	mkdir -p $(@D)
	$(CC) -o $@ $^

obj/%.o obj/debug/%.o : src/%.d
	$(CC) $(CFLAGS) -c src/$*.c -o $@

# STEP 1: Generate the list of included header files in the source file.
#
# This rule updates the .d file when the _list_ has changed.
src/%.d : src/%.c
	$(CPP) -MMD -MF $@ -MP -MT "$(@:.d=.o) $@" $< -o /dev/null

# STEP 2: Include the rules for each object file.
#
# These update the .d (and .o) files when the _contents of the header files_
# have changed.
include $(SRC:.c=.d)

clean : 
	find -not \( -wholename bin/gsh -or -name README.md \) -delete
reset :
	rm -rf obj/* bin/*