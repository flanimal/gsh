#include <unistd.h>
#include <limits.h>

#include <stdlib.h>
#include <stdio.h>

#include "gnuish.h"

int main(int argc, char *argv[], char *envp[])
{
	struct gnuish_state sh_state;

	gnuish_init(&sh_state, envp);
	
	/* Main loop. */
	char *line = malloc((size_t)sh_state.max_input);

	for (ssize_t len; (len = gnuish_read_line(&sh_state, line)) > 0;) {
		gnuish_run_cmd(&sh_state, line);
	}
	
	return 0;
}