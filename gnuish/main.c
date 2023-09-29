#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>

#include "gnuish.h"

int main(__attribute_maybe_unused__ int argc,
	 __attribute_maybe_unused__ char *argv[])
{
	struct gnuish_state sh_state;
	gnuish_init(&sh_state);
	
	/* Main loop. */
	char *line = malloc(gnuish_max_input(&sh_state));

	for (ssize_t len; (len = gnuish_read_line(&sh_state, &line)) != -1; )
		gnuish_run_cmd(&sh_state, (size_t)len, line);

	printf("%s\n", strerror(errno));
	exit(EXIT_FAILURE);
}