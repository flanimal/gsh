#include "gsh.h"
#include "input.h"

int main(int argc, char *argv[])
{
	struct gsh_state sh;
	gsh_init(&sh);

	for (;;) {
		gsh_put_prompt(&sh);
		
		while (gsh_read_line(sh.inputbuf))
			;

		gsh_run_cmd(&sh);
	}

	return 0;
}