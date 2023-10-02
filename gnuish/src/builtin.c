#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "builtin.h"

int gsh_puthelp()
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

int gsh_chdir(struct gsh_workdir *wd, const char *pathname)
{
	if (chdir(pathname) == -1) {
		printf("%s: %s\n", pathname, strerror(errno));
		return -1;
	}

	gsh_getcwd(wd);

	return 0;
}

int gsh_echo(char *const *args)
{
	for (; *args; putchar(' '), ++args)
		if (fputs(*args, stdout) == EOF)
			return -1;

	putchar('\n');

	return 0;
}