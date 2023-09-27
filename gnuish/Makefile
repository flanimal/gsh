SHELL = /bin/bash

objects = main.o gnuish.o

CFLAGS = -O0 -Werror -Wall -Wextra -fno-omit-frame-pointer -std=gnu11

gnuish : $(objects)
	gcc -o gnuish $(objects)

main.o : main.c gnuish.h
gnuish.o : gnuish.c gnuish.h

.PHONY : clean
clean :
	rm -f *.{o,h,c}