SHELL = /bin/bash

objects = $(addprefix obj/, main.o gnuish.o builtin.o history.o)

CFLAGS = -O0 -Werror -Wall -Wextra -fno-omit-frame-pointer -std=gnu11

gsh : $(objects)
	gcc -o gsh $(objects)

$(objects): obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@ 

.PHONY : clean
clean :
	rm -f obj/*.o src/*.{h,c} 