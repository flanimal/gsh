VPATH = src
SHELL = /bin/bash

objects = main.o gnuish.o builtin.o history.o

CFLAGS = -O0 -Werror -Wall -Wextra -fno-omit-frame-pointer -std=gnu11

gnuish : $(objects)
	gcc -o gnuish $(addprefix obj/, $(objects))

$(objects): %.o: %.c
	$(CC) $(CFLAGS) -c $< -o obj/$@ 

.PHONY : clean
clean :
	rm -f obj/*.o src/*.{h,c} 