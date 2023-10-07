#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "gsh.h"

int main(__attribute_maybe_unused__ int argc,
	 __attribute_maybe_unused__ char *argv[])
{
	struct gsh_state sh_state;
	gsh_init(&sh_state);

	for (;;) {
		gsh_read_line(&sh_state);
		gsh_run_cmd(&sh_state);
	}
}