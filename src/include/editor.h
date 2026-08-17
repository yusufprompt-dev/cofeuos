#ifndef EDITOR_H
#define EDITOR_H

#include "types.h"

#define ED_MAX_LINES   64
#define ED_LINE_LEN    128
#define ED_NAME_MAX    64

typedef struct {
    char lines[ED_MAX_LINES][ED_LINE_LEN];
    int  line_count;
    int  ed_cx;
    int  ed_cy;
    int  scroll_top;
    int  modified;
    char filename[ED_NAME_MAX];
    int  running;
} editor_state_t;

void editor_init(editor_state_t *ed);
int  editor_open(editor_state_t *ed, const char *filename);
int  editor_save(editor_state_t *ed, const char *filename);
int  editor_run(editor_state_t *ed);
void editor_insert_char(editor_state_t *ed, char c);
void editor_delete_char(editor_state_t *ed);
void editor_insert_line(editor_state_t *ed);
void editor_move_cursor(editor_state_t *ed, int dx, int dy);
void editor_scroll(editor_state_t *ed, int lines);

#endif
