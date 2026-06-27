#include "py/mphal.h"
#include "../../include/video.h"
#include "../../include/shell.h"

void mp_hal_stdout_tx_strn_cooked(const char *str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '\n') {
            cursor_x = splits[active_split].x + 5;
            cursor_y += font_height;
        } else {
            video_draw_char(str[i], cursor_x, cursor_y, 15);
            cursor_x += font_width;
        }
    }
}
