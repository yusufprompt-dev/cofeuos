#include "../include/editor.h"
#include "../include/string.h"

static void str_copy(char *dst, const char *src, int max) {
    int i = 0; while (src[i] && i < max - 1) { dst[i] = src[i]; i++; } dst[i] = '\0'; }
static int str_len(const char *s) { int n = 0; while (s[n]) n++; return n; }

void editor_init(editor_state_t *ed) {
    if (!ed) return;
    ed->line_count = 1;
    ed->ed_cx = 0;
    ed->ed_cy = 0;
    ed->scroll_top = 0;
    ed->modified = 0;
    ed->filename[0] = '\0';
    ed->running = 0;
    ed->lines[0][0] = '\0';
}

int editor_open(editor_state_t *ed, const char *filename) {
    if (!ed || !filename) return -1;
    str_copy(ed->filename, filename, ED_NAME_MAX);
    ed->line_count = 1;
    ed->ed_cx = 0;
    ed->ed_cy = 0;
    ed->scroll_top = 0;
    ed->modified = 0;
    ed->lines[0][0] = '\0';
    str_copy(ed->lines[0], "Dosya yukleniyor...", ED_LINE_LEN);
    ed->line_count = 1;
    return 0;
}

int editor_save(editor_state_t *ed, const char *filename) {
    if (!ed) return -1;
    if (filename) str_copy(ed->filename, filename, ED_NAME_MAX);
    ed->modified = 0;
    return 0;
}

void editor_insert_char(editor_state_t *ed, char c) {
    if (!ed || ed->ed_cy >= ED_MAX_LINES) return;
    int len = str_len(ed->lines[ed->ed_cy]);
    if (len >= ED_LINE_LEN - 1) return;
    for (int i = len; i > ed->ed_cx; i--) ed->lines[ed->ed_cy][i] = ed->lines[ed->ed_cy][i-1];
    ed->lines[ed->ed_cy][ed->ed_cx] = c;
    ed->lines[ed->ed_cy][len + 1] = '\0';
    ed->ed_cx++;
    ed->modified = 1;
}

void editor_delete_char(editor_state_t *ed) {
    if (!ed) return;
    if (ed->ed_cx > 0) {
        int len = str_len(ed->lines[ed->ed_cy]);
        for (int i = ed->ed_cx - 1; i < len; i++) ed->lines[ed->ed_cy][i] = ed->lines[ed->ed_cy][i+1];
        ed->ed_cx--;
        ed->modified = 1;
    } else if (ed->ed_cy > 0) {
        int prev_len = str_len(ed->lines[ed->ed_cy - 1]);
        ed->ed_cx = prev_len;
        for (int i = ed->ed_cy; i < ed->line_count - 1; i++) {
            str_copy(ed->lines[i], ed->lines[i + 1], ED_LINE_LEN);
        }
        ed->line_count--;
        ed->ed_cy--;
        ed->modified = 1;
    }
}

void editor_insert_line(editor_state_t *ed) {
    if (!ed || ed->line_count >= ED_MAX_LINES) return;
    int rest_len = str_len(ed->lines[ed->ed_cy] + ed->ed_cx);
    if (rest_len > 0) {
        for (int i = ed->line_count; i > ed->ed_cy + 1; i--) {
            str_copy(ed->lines[i], ed->lines[i - 1], ED_LINE_LEN);
        }
        str_copy(ed->lines[ed->ed_cy + 1], ed->lines[ed->ed_cy] + ed->ed_cx, ED_LINE_LEN);
        ed->lines[ed->ed_cy][ed->ed_cx] = '\0';
    } else {
        for (int i = ed->line_count; i > ed->ed_cy + 1; i--) {
            str_copy(ed->lines[i], ed->lines[i - 1], ED_LINE_LEN);
        }
        ed->lines[ed->ed_cy + 1][0] = '\0';
    }
    ed->line_count++;
    ed->ed_cy++;
    ed->ed_cx = 0;
    ed->modified = 1;
}

void editor_move_cursor(editor_state_t *ed, int dx, int dy) {
    if (!ed) return;
    ed->ed_cx += dx;
    ed->ed_cy += dy;
    if (ed->ed_cy < 0) ed->ed_cy = 0;
    if (ed->ed_cy >= ed->line_count) ed->ed_cy = ed->line_count - 1;
    int len = str_len(ed->lines[ed->ed_cy]);
    if (ed->ed_cx < 0) ed->ed_cx = 0;
    if (ed->ed_cx > len) ed->ed_cx = len;
}

void editor_scroll(editor_state_t *ed, int lines) {
    if (!ed) return;
    ed->scroll_top += lines;
    if (ed->scroll_top < 0) ed->scroll_top = 0;
    if (ed->scroll_top > ed->line_count - 20) ed->scroll_top = ed->line_count - 20;
    if (ed->scroll_top < 0) ed->scroll_top = 0;
}

int editor_run(editor_state_t *ed) {
    if (!ed) return -1;
    ed->running = 1;
    return 0;
}
