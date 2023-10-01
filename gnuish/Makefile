SHELL = /bin/bash

objects = main.o gnuish.o builtin.o history.o

CFLAGS = -O0 -Werror -Wall -Wextra -fno-omit-frame-pointer -std=gnu11

gnuish : $(objects)
	gcc -o gnuish $(objects)

main.o          : main.c gnuish.h
gnuish.o        : gnuish.c gnuish.h builtin.h history.h system.h
builtin.o       : builtin.c builtin.h system.h
history.o       : history.h gnuish.h

.PHONY : clean
clean :
	rm -f *.{o,h,c}