#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>

#include "gnuish.h"

int main(__attribute_maybe_unused__ int argc,
	 __attribute_maybe_unused__ char *argv[])
{
	struct gsh_state sh_state;
	gsh_init(&sh_state);
	
	/* Main loop. */
	char *line = malloc(gsh_max_input(&sh_state));

	while (gsh_read_line(&sh_state, &line))
		gsh_run_cmd(&sh_state, line);
        
	printf("%s\n", strerror(errno));
	exit(EXIT_FAILURE);
}