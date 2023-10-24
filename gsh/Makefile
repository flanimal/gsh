src = $(wildcard src/*.c)
objects = $(notdir $(src:.c=.o))

ERRORS	= -Werror -Wall -Wextra -pedantic-errors
DEFINES = _GNU_SOURCE

CFLAGS = $(ERRORS) -std=gnu11 $(addprefix -D, $(DEFINES))

.PHONY : debug clean reset

all : gsh

clean : 
	find -not \( -wholename bin/gsh -or -name README.md \) -delete
reset :
	rm -rf obj/* bin/*

debug : CFLAGS += -O0 -fno-omit-frame-pointer -g
debug : $(addprefix obj/debug/, $(objects))
	mkdir -p bin/debug
	$(CC) -o bin/debug/gsh $<

gsh : $(addprefix obj/, $(objects))
	mkdir -p bin
	$(CC) -o bin/gsh $?

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
include $(src:.c=.d)