SRC	:= $(wildcard src/*.c)

ERRORS	:= -Werror -Wall -Wextra -pedantic-errors
DEFINES := _GNU_SOURCE

CFLAGS	:= $(ERRORS) -std=gnu11 $(addprefix -D, $(DEFINES))

OBJECTS := $(notdir $(SRC:.c=.o))
OUT	:= gsh

.PHONY : all debug clean reset

all : bin/$(OUT)

debug : CFLAGS += -O0 -fno-omit-frame-pointer -g
debug : bin/debug/$(OUT)

clean : 
	find -not \( -wholename bin/$(OUT) -or -name README.md \) -delete
reset :
	rm -rf obj/* bin/* src/*.d

.SECONDEXPANSION:

BINPATH = $$(@D)/
OBJPATH = $$(subst bin, obj, $$(@D)/)

bin/$(OUT) bin/debug/$(OUT) : $(addprefix $(OBJPATH), $(OBJECTS)) | $(BINPATH)
	$(CC) -o $@ $^

obj/%.o obj/debug/%.o : src/%.d | $(OBJPATH)
	$(CC) $(CFLAGS) -c src/$*.c -o $@

%/ %/debug/ :
	mkdir -p $@

src/%.d : src/%.c
	$(CPP) -MMD -MF $@ -MP -MT "$(@:.d=.o) $@" $< -o /dev/null

include $(SRC:.c=.d)