#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#include "gnuish.h"

int main(int argc, char *argv[], char *envp[])
{
	struct gnuish_state sh_state;
	// TODO: We will probably need to copy the original environment.
	sh_state.env = envp;

	sh_state.cwd = getcwd(NULL, 0);
	sh_state.max_input = pathconf(sh_state.cwd, _PC_MAX_INPUT);

	/* Main loop. */
	char *line = malloc((size_t)sh_state.max_input);

	for (ssize_t len; (len = gnuish_read_line(&sh_state, line)) > 0;) {
		gnuish_run_cmd(&sh_state, line);
	}

	printf("Process ending\n");
	return 0;
}