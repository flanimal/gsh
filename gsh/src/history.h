#pragma once

#include <stddef.h>

/* Line history entry. */
struct gsh_hist_ent {
	struct gsh_hist_ent *back, *forw;

	char *line;
	size_t len;
};

struct gsh_cmd_hist {
	/* Tail and head of command history queue. */
	struct gsh_hist_ent *cmd_history, *oldest_cmd;

	/* Number of commands in history (maximum 10). */
	int hist_n;
};

void gsh_add_hist(struct gsh_cmd_hist *sh_hist, size_t len, const char *line);

int gsh_list_hist(const struct gsh_hist_ent *cmd_it);