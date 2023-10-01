#pragma once

struct gsh_workdir {
	/* Current working directory of the shell process. */
	char *cwd;

	/*
	 *      Runtime constants.
	 */
	/* Maximum length of newline-terminated input line on terminal. */
	long max_input;

	/* Maximum length of pathnames, including null character. */
	long max_path;
};

void gsh_getcwd(struct gsh_workdir *wd);