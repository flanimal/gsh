objects = main.o gsh.o builtin.o history.o parse.o

DEFINES = _GNU_SOURCE
CFLAGS = -Werror -Wall -Wextra -pedantic-errors -std=gnu11 $(addprefix -D, $(DEFINES))

objpath = obj/

.PHONY : debug clean reset

all : gsh

debug : objpath = obj/debug/
debug : CFLAGS += -O0 -fno-omit-frame-pointer -g
debug : gsh

gsh : $(objects)
	gcc -o gsh $(addprefix $(objpath), $(objects))

$(objects): %.o: src/%.c
	mkdir -p $(objpath)
	$(CC) $(CFLAGS) -c $< -o $(addprefix $(objpath), $@) 

clean : 
	find -not \( -name gsh -or -name README.md \) -delete
reset :
	rm -rf obj/* gsh
