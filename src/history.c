#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "gsh.h"
#include "input.h"
#include "history.h"

#define GSH_MAX_HIST 20

/* Line history entry. */
struct gsh_hist_ent {
	struct gsh_hist_ent *back, *forw;

	char *line;
	size_t len;
};

struct gsh_cmd_hist {
	/* Tail and head of command history queue. */
	struct gsh_hist_ent *newest, *oldest;

	/* Number of commands in history (maximum 10). */
	int count;
};

struct gsh_cmd_hist *gsh_new_hist()
{
	struct gsh_cmd_hist *hist = malloc(sizeof(*hist));

	hist->newest = hist->oldest = NULL;
	hist->count = 0;

	return hist;
}

static void new_hist_ent(struct gsh_cmd_hist *hist, size_t len,
			 const char *line)
{
	struct gsh_hist_ent *ent = malloc(sizeof(*ent));

	insque(ent, hist->newest);
	hist->newest = ent;

	ent->line = strcpy(malloc(len + 1), line);
	ent->len = len;

	if (hist->count == 0)
		hist->oldest = ent;

	++hist->count;
}

static void drop_hist_ent(struct gsh_cmd_hist *hist,
			  struct gsh_hist_ent *ent)
{
	if (!(hist->oldest = ent->back))
		hist->newest = NULL;

	remque(ent);

	free(ent->line);
	free(ent);

	--hist->count;
}

void gsh_add_hist(struct gsh_cmd_hist *hist, size_t len, const char *line)
{
	// The recall command `r` itself should NOT be added to history.
	if (line[0] == 'r' && (!line[1] || isspace(line[1])))
		return;

	new_hist_ent(hist, len, line);

	if (hist->count > GSH_MAX_HIST)
		drop_hist_ent(hist, hist->oldest);
}

/* Builtins. */

int gsh_list_hist(struct gsh_state *sh, int argc, char *const *argv)
{
	if (argc > 1 && strcmp(argv[1], "-c") == 0) {
		while (sh->hist->count > 0)
			drop_hist_ent(sh->hist, sh->hist->oldest);

		return 0;
	}

	// It is not possible for `newest` to be NULL here,
	// as it will contain at least the `hist` invocation.
	int n = 1;

	for (struct gsh_hist_ent *hist_it = sh->hist->newest; hist_it;
	     hist_it = hist_it->forw)
		printf("%d: %s\n", n++, hist_it->line);

	return 0;
}

/* Re-run the n-th previous line of input. */
int gsh_recall(struct gsh_state *sh, int argc, char *const *argv)
{
	int n = (argv[1]) ? atoi(argv[1]) : 1;

	if (0 >= n || sh->hist->count < n) {
		gsh_bad_cmd("no matching history entry", 0);
		return -1;
	}

	struct gsh_hist_ent *hist_it = sh->hist->newest;

	while (hist_it->forw && n-- > 1)
		hist_it = hist_it->forw;

	printf("%s\n", hist_it->line);

	// Make a copy so we don't lose it if the history entry
	// gets deleted.
	strcpy(sh->inputbuf->line, hist_it->line);
	sh->inputbuf->len = hist_it->len;

	gsh_run_cmd(sh);
	return sh->params.last_status;
}