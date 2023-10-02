#include <search.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "gnuish.h"
#include "history.h"

#define GSH_MAX_HIST 10

void gsh_bad_cmd(const char *msg, int err);

static void new_hist_ent(struct gsh_cmd_hist *sh_hist,
					 size_t len, const char *line)
{
	struct gsh_hist_ent *last_cmd = malloc(sizeof(*last_cmd));

	insque(last_cmd, sh_hist->cmd_history);
	sh_hist->cmd_history = last_cmd;

	last_cmd->line = strcpy(malloc(len + 1), line);
	last_cmd->len = len;

        if (sh_hist->hist_n == 0)
		sh_hist->oldest_cmd = last_cmd;

	++sh_hist->hist_n;
}

static void drop_hist_ent(struct gsh_cmd_hist *sh_hist,
			  struct gsh_hist_ent *dropped_ent)
{
	sh_hist->oldest_cmd = dropped_ent->back;

	remque(dropped_ent);

	free(dropped_ent->line);
	free(dropped_ent);

	--sh_hist->hist_n;
}

void gsh_add_hist(struct gsh_cmd_hist *sh_hist, size_t len,
			 const char *line)
{
	// The recall command `r` itself should NOT be added to history.
	if (line[0] == 'r' && (!line[1] || isspace(line[1])))
		return;

	new_hist_ent(sh_hist, len, line);

	if (sh_hist->hist_n > GSH_MAX_HIST)
		drop_hist_ent(sh_hist, sh_hist->oldest_cmd);
}
// TODO: Clear history option
int gsh_list_hist(const struct gsh_hist_ent *cmd_it)
{
	// It is not possible for `cmd_history` to be NULL here,
	// as it will contain at least the `hist` invocation.
	int cmd_n = 1;

	for (; cmd_it; cmd_it = cmd_it->forw)
		printf("%d: %s\n", cmd_n++, cmd_it->line);

	return 0;
}

/* Re-run the n-th previous line of input. */
int gsh_recall(struct gsh_state *sh, const char *recall_arg)
{
	int n_arg = (recall_arg ? atoi(recall_arg) : 1);

	if (0 >= n_arg || sh->hist->hist_n < n_arg) {
		gsh_bad_cmd("no matching history entry", 0);
		return -1;
	}

	struct gsh_hist_ent *cmd_it = sh->hist->cmd_history;

	while (cmd_it->forw && n_arg-- > 1)
		cmd_it = cmd_it->forw;

	printf("%s\n", cmd_it->line);

        // Make a copy so we don't lose it if the history entry
        // gets deleted.
        char *ent_line_cpy = strcpy(malloc(cmd_it->len + 1), cmd_it->line);

        gsh_run_cmd(sh, cmd_it->len, ent_line_cpy);
        free(ent_line_cpy);

	return sh->params.last_status;
}