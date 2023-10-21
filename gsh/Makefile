objects = $(addprefix obj/, main.o gsh.o builtin.o history.o parse.o)

CFLAGS = -O0 -Werror -Wall -Wextra -pedantic-errors -fno-omit-frame-pointer -std=gnu11 -D _GNU_SOURCE

gsh : $(objects)
	gcc -o gsh $(objects)

$(objects): obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@ 

.PHONY : clean reset 
clean : 
	find -not \( -name gsh -or -name README.md \) -delete
reset :
	rm -f obj/* gsh
