#pragma once
#include "py/misc.h"

#define CHAR_CTRL_C (3)
#define CHAR_CTRL_D (4)

int mp_hal_readline(vstr_t *line, const char *prompt);
#define readline mp_hal_readline
