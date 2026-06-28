#include "../../include/keyboard.h"
#include "../../include/string.h"
#include "../../include/video.h"
#include "../../include/shell.h"
#include "py/mphal.h"
#include "shared/readline/readline.h"

void mp_hal_stdout_tx_strn_cooked(const char *str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '\n' || str[i] == '\r') {
            cursor_x = splits[active_split].x + 5;
            cursor_y += font_height;
        } else {
            video_draw_char(str[i], cursor_x, cursor_y, 15);
            cursor_x += font_width;
        }
    }
}

int mp_hal_stdin_rx_chr(void) {
    return (int)read_key();
}

uintptr_t mp_hal_stdio_poll(uintptr_t poll_flags) {
    return poll_flags;
}

int mp_hal_readline(vstr_t *line, const char *prompt) {
    if (prompt) {
        mp_hal_stdout_tx_strn_cooked(prompt, strlen(prompt));
    }
    vstr_reset(line);
    while (1) {
        char ch = read_key();
        if (ch == '\n' || ch == '\r') {
            mp_hal_stdout_tx_strn_cooked("\n", 1);
            return 0;
        } else if (ch == '\b') {
            if (line->len > 0) {
                vstr_cut_tail_bytes(line, 1);
                mp_hal_stdout_tx_strn_cooked("\b \b", 3);
            }
        } else if (ch == 3) {
            return 3; /* CHAR_CTRL_C */
        } else if (ch >= 32) {
            vstr_add_byte(line, ch);
            mp_hal_stdout_tx_strn_cooked(&ch, 1);
        }
    }
}
