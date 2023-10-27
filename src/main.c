#include <sys/cdefs.h>

#include "gsh.h"
#include "input.h"

int main(int argc, char *argv[])
{
	struct gsh_state sh;
	struct gsh_parse_bufs *parsebufs = gsh_new_parsebufs();

	gsh_init(&sh, parsebufs);

	for (;;) {
		gsh_put_prompt(&sh);
		
		// 1. Read input into input_buf.
		while (gsh_read_line(sh.inputbuf))
			;

		// 2. Run the input stored in the input_buf.
		gsh_run_cmd(&sh);
	}

	return 0;
}