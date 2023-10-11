#pragma once

#include <stddef.h>

struct gsh_cmd_hist {
	/* Tail and head of command history queue. */
	struct gsh_hist_ent *cmd_history, *oldest_cmd;

	/* Number of commands in history (maximum 10). */
	int hist_n;
};

void gsh_add_hist(struct gsh_cmd_hist *sh_hist, size_t len, const char *line);

int gsh_list_hist(const struct gsh_cmd_hist *sh_hist);

int gsh_recall(struct gsh_state *sh, const char *recall_arg);