#include <sys/cdefs.h>

#include "gsh.h"

int main(int argc, char *argv[])
{
	struct gsh_state sh;
	struct gsh_parse_bufs *parsebufs = gsh_new_parsebufs();

	gsh_init(&sh, parsebufs);

	for (;;) {
		gsh_put_prompt(&sh);
		
		while (gsh_read_line(sh.inputbuf))
			;

		gsh_run_cmd(&sh);
	}
}