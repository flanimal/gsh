#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#include "gnuish.h"

int main(int argc, char* argv[], char* envp[])
{
	struct gnuish_state sh_state;
	sh_state.env = envp; // TODO: We will probably need to copy the original environment.

	gnuish_chdir(&sh_state, "~");
	char* line = malloc((size_t)sh_state.max_input);

	/* Main loop. */
	for (ssize_t nbytes; (nbytes = gnuish_read_ln(&sh_state, line)) > 0; )
	{
		gnuish_run_cmd(&sh_state, line);
	}

	printf("Process ending\n");
	return 0;
}