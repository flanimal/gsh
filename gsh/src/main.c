#include <sys/cdefs.h>

#include "gsh.h"

int main(__attribute_maybe_unused__ int argc,
	 __attribute_maybe_unused__ char *argv[])
{
	struct gsh_state sh_state;
	gsh_init(&sh_state);

	// TODO: !!! Create the parse bufs here?
	// TODO: Use put_prompt() to print secondary prompt too,
	// and continue to next loop iteration if more input is needed?
	for (;;) {
		gsh_put_prompt(&sh_state);
		gsh_run_cmd(&sh_state);
	}
}