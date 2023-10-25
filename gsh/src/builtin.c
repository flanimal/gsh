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

// TODO: [ ] type builtin.
// TODO: [ ] pwd builtin?
// 
// For example:
//

// 
// When we do "help echo", for example, it should look something like this:
// 
//	<builtin name> : <builtin syntax>
//	<description string>
// 
//	[argument description] ...
// 
static GSH_DEF_BUILTIN(gsh_echo, _, args)
{
	for (++args; *args; ++args) {
		if (fputs(*args, stdout) == EOF)
			return -1;

		if (args[1])
			putchar(' ');
	}

	putchar('\n');

	return 0;
}

static GSH_DEF_BUILTIN(gsh_chdir, sh, args)
{
	if (!args[1]) {
		chdir(gsh_getenv(&sh->params, "HOME"));
	} else if (chdir(args[1]) == -1) {
		printf("%s: %s\n", args[1], strerror(errno));
		return -1;
	}

	gsh_getcwd(sh);

	return 0;
}

static GSH_DEF_BUILTIN(gsh_puthelp, _, __);

static struct gsh_builtin builtins[] = {
	{ "echo", "Write arguments to standard output.", gsh_echo },
	{ "r", "Execute the Nth last line.", gsh_recall },
	{ "cd", "Change the shell working directory.", gsh_chdir },
	{ "hist", "Display or clear line history.", gsh_list_hist },
	{ "help", "Display this help page.", gsh_puthelp },
	{ "exit", "Exit the shell.", NULL },
};

static GSH_DEF_BUILTIN(gsh_puthelp, _, __)
{
	char dots[15];
	memset(dots, '.', sizeof(dots));

	for (size_t i = 0; i < sizeof(builtins) / sizeof(*builtins); ++i) {
		const size_t cmd_len = (size_t)printf("%s ", builtins[i].cmd);

		if (cmd_len > sizeof(dots)) {
			puts(builtins[i].helpstr);
			continue;
		}

		dots[sizeof(dots) - cmd_len] = '\0';

		printf("%s %s\n", dots, builtins[i].helpstr);
		dots[sizeof(dots) - cmd_len] = '.';
	}

	return 0;
}

void gsh_set_builtins(struct hsearch_data **builtin_tbl)
{
	create_hashtable(builtins, .cmd, , *builtin_tbl);
}
