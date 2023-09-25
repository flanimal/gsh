SHELL = /bin/bash

objects = main.o gnuish.o

CFLAGS = -O0 -W"switch" -W"no-deprecated-declarations" \
	-W"empty-body" -W"conversion" -W"return-type" -W"parentheses" -W"no-format" \
	-W"uninitialized" -W"unreachable-code" -W"unused-function" -W"unused-value" \
	-W"unused-variable" -Werror -fno-omit-frame-pointer -std=gnu11

gnuish : $(objects)
	gcc -o gnuish $(objects)

main.o : main.c gnuish.h
gnuish.o : gnuish.c gnuish.h

.PHONY : clean
clean :
	rm -f *.{o,h,c}