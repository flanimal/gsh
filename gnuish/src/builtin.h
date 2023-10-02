#pragma once

#include "system.h"

int gsh_puthelp();

int gsh_chdir(struct gsh_workdir *wd, const char *pathname);

int gsh_echo(char *const *args);