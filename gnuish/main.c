#include <stdlib.h>
#include <stddef.h>

#include "gnuish.h"

int main(__attribute_maybe_unused__ int argc,
	 __attribute_maybe_unused__ char *argv[], char *envp[])
{
	struct gnuish_state sh_state;

	gnuish_init(&sh_state, envp);
	
	/* Main loop. */
	char *line = malloc((size_t)sh_state.max_input);

	for (size_t len; (len = gnuish_read_line(&sh_state, &line));)
		gnuish_run_cmd(&sh_state, len, line);

	return 0;
}