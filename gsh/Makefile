objects = main.o gsh.o builtin.o history.o parse.o

DEFINES = _GNU_SOURCE
CFLAGS = -Werror -Wall -Wextra -pedantic-errors -std=gnu11 $(addprefix -D, $(DEFINES))

.PHONY : debug clean reset

all : gsh

debug : CFLAGS += -O0 -fno-omit-frame-pointer -g
debug : $(addprefix obj/debug/, $(objects))
	mkdir -p bin/debug
	gcc -o bin/debug/gsh $?

gsh : $(addprefix obj/, $(objects))
	mkdir -p bin
	gcc -o bin/gsh $?

obj/%.o obj/debug/%.o: src/%.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@ 

clean : 
	find -not \( -wholename bin/gsh -or -name README.md \) -delete
reset :
	rm -rf obj/* bin/*
