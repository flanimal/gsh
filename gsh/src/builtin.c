#define _GNU_SOURCE
#include <search.h>

#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "gsh.h"
#include "history.h"
#include "builtin.h"

#define GSH_NUM_BUILTINS 20

#define GSH_DEF_BUILTIN_CB(name, sh, args)                        \
	int name(__attribute_maybe_unused__ struct gsh_state *sh, \
		 __attribute_maybe_unused__ char *const *args)

#define GSH_ENTER_BUILTIN_CB(cmd, name, callbacks, tbl)                       \
	*callbacks = (struct gsh_cb_wrapper){ name };                    \
	hsearch_r((ENTRY){ .key = cmd, .data = callbacks++ }, ENTER, &retval, \
		  tbl)

GSH_DEF_BUILTIN_CB(gsh_echo, sh, args)
{
	for (++args; *args; putchar(' '), ++args)
		if (fputs(*args, stdout) == EOF)
			return -1;

	putchar('\n');

	return 0;
}

GSH_DEF_BUILTIN_CB(gsh_chdir, sh, args)
{
	if (chdir(args[1]) == -1) {
		printf("%s: %s\n", args[1], strerror(errno));
		return -1;
	}

	gsh_getcwd(sh->wd);

	return 0;
}

GSH_DEF_BUILTIN_CB(gsh_recall, sh, args);
GSH_DEF_BUILTIN_CB(gsh_list_hist, sh, args);

GSH_DEF_BUILTIN_CB(gsh_puthelp, sh, args)
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
	*builtin_tbl = calloc(1, sizeof(**builtin_tbl));

	// TODO: Number of builtins.
	hcreate_r(GSH_NUM_BUILTINS, *builtin_tbl);

	struct gsh_cb_wrapper *callbacks =
		malloc(sizeof(*callbacks) * GSH_NUM_BUILTINS);

	ENTRY *retval;

	GSH_ENTER_BUILTIN_CB("echo", gsh_echo, callbacks, *builtin_tbl);
	GSH_ENTER_BUILTIN_CB("r", gsh_recall, callbacks, *builtin_tbl);
	GSH_ENTER_BUILTIN_CB("cd", gsh_chdir, callbacks, *builtin_tbl);
	GSH_ENTER_BUILTIN_CB("hist", gsh_list_hist, callbacks, *builtin_tbl);
	// GSH_ENTER_BUILTIN_CB("shopt", gsh_set_opt, *builtin_tbl);
	GSH_ENTER_BUILTIN_CB("help", gsh_puthelp, callbacks, *builtin_tbl);
}