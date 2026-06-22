/*
 * SHELL.H - cofeuOS Shell API
 */

#ifndef _SHELL_H
#define _SHELL_H

#include "types.h"
#include "fs.h"
#include "string.h"

#define MAX_PATH_LEN 256

typedef struct {
    int x, y, w, h;
    int cx, cy;
    int active;
    char cmd_buf[512];
    int cmd_pos;
    int needs_prompt;
} shell_split_t;

#define MAX_SPLITS 2
extern shell_split_t splits[MAX_SPLITS];
extern int active_split;
extern int num_splits;

#define cursor_x (splits[active_split].cx)
#define cursor_y (splits[active_split].cy)

typedef struct {
    char user[32];
    char host[32];
    char partition[32];
    char cwd[MAX_PATH_LEN];
} shell_control;

extern shell_control g_shell;
extern int shell_execute(const char* cmd);

#endif

