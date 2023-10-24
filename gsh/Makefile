SRC	:= $(wildcard src/*.c)
OBJECTS := $(notdir $(SRC:.c=.o))

ERRORS	:= -Werror -Wall -Wextra -pedantic-errors
DEFINES := _GNU_SOURCE

CFLAGS	:= $(ERRORS) -std=gnu11 $(addprefix -D, $(DEFINES))

.PHONY : all debug clean reset

all : bin/gsh

debug : CFLAGS += -O0 -fno-omit-frame-pointer -g
debug : bin/debug/gsh

clean : 
	find -not \( -wholename bin/gsh -or -name README.md \) -delete
reset :
	rm -rf obj/* bin/*

.SECONDEXPANSION:

BINPATH = $$(@D)/
OBJPATH = $$(subst bin, obj, $$(@D)/)

bin/gsh bin/debug/gsh : $(addprefix $(OBJPATH), $(OBJECTS)) | $(BINPATH)
	$(CC) -o $@ $^

obj/%.o obj/debug/%.o : src/%.d | $(OBJPATH)
	$(CC) $(CFLAGS) -c src/$*.c -o $@

%/ %/debug/ :
	mkdir -p $@

src/%.d : src/%.c
	$(CPP) -MMD -MF $@ -MP -MT "$(@:.d=.o) $@" $< -o /dev/null

include $(SRC:.c=.d)