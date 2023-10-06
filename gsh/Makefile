objects = $(addprefix obj/, main.o gsh.o builtin.o history.o parse.o)

CFLAGS = -O0 -Werror -Wall -Wextra -fno-omit-frame-pointer -std=gnu11

gsh : $(objects)
	gcc -o gsh $(objects)

$(objects): obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@ 

.PHONY : clean softclean 
clean : 
	rm -rf obj/* src/*
softclean :
	rm -f obj/*
