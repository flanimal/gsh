#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "gsh.h"
#include "history.h"
#include "builtin.h"

#define GSH_DEF_BUILTIN(name, sh_param, args_param)                     \
	int name(__attribute_maybe_unused__ struct gsh_state *sh_param, \
		 __attribute_maybe_unused__ char *const *args_param)

GSH_DEF_BUILTIN(gsh_recall, sh, args);
GSH_DEF_BUILTIN(gsh_list_hist, sh, args);

static GSH_DEF_BUILTIN(gsh_echo, _, args)
{
	for (++args; *args; putchar(' '), ++args)
		if (fputs(*args, stdout) == EOF)
			return -1;

	putchar('\n');

	return 0;
}

static GSH_DEF_BUILTIN(gsh_chdir, sh, args)
{
	if (chdir(args[1]) == -1) {
		printf("%s: %s\n", args[1], strerror(errno));
		return -1;
	}

	gsh_getcwd(sh->wd);

	return 0;
}

static GSH_DEF_BUILTIN(gsh_puthelp, _, __)
{
	puts("\ngsh - GNU island shell");
	puts("\ngsh displays the current working directory in the shell prompt :");
	puts("\t~@ /");
	puts("\tusr/ @");
	puts("\t/mnt/.../repos @");

	puts("\nCommands");
	puts("\n\t<command> [<arg>...]\tRun command or program with optional arguments.");

	puts("\n\tr[<n>]\tExecute the nth last line.");
	puts("\t\tThe line will be placed in history--not the `r` invocation.");
	puts("\t\tThe line in question will be echoed to the screen before being executed.");

	puts("\nShell builtins");
	puts("\n\texit\tExit the shell.");
	puts("\thist\tDisplay up to 10 last lines entered, numbered.");

	puts("\t----");

	puts("\techo\tWrite to standard output.");
	puts("\thelp\tDisplay this help page.");

	putchar('\n');

	return 0;
}

void gsh_set_builtins(struct hsearch_data **builtin_tbl)
{
	static struct gsh_builtin builtins[] = {
		{ "echo", gsh_echo },	 { "r", gsh_recall },
		{ "cd", gsh_chdir },	 { "hist", gsh_list_hist },
		{ "help", gsh_puthelp },
	};

	create_hashtable(builtins, .cmd, , *builtin_tbl);
}
