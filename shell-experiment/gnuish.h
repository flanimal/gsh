#pragma once

enum gnuish_built_in {
	EXIT,
	CHDIR,
	KILL,
	PWD,
	ECHO,
	HIST,
	HELP,
	RECALL,
};

struct gnuish_hist_ent {
	struct gnuish_hist_ent *forward, *backward;
	char *line;
};

struct gnuish_state {
	long max_input;
	long max_path;
	// If we need a buffer to get the current working directory anyway,
	// then we might as well store it in the shell state structure
	// to have on hand.
	char *cwd;
	
	struct gnuish_hist_ent *cmd_history, *oldest_cmd;
	int hist_n;

	char *const *env;
};

void gnuish_init(struct gnuish_state *sh_state, char *const *envp);

void gnuish_echo(struct gnuish_state *sh_state, char **args);

ssize_t gnuish_read_line(struct gnuish_state *sh_state, char *out_line);

void gnuish_run_cmd(struct gnuish_state *sh_state, char *line);

void gnuish_chdir(struct gnuish_state *sh_state, const char *pathname);

void gnuish_exec(struct gnuish_state *sh_state, char *const *args);

void gnuish_exit(struct gnuish_state *sh_state);