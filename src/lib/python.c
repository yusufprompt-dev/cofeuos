/*
 * PYTHON.C - MicroPython REPL entegrasyonu
 */
#include "../include/python.h"
#include "../include/video.h"
#include "../include/keyboard.h"
#include "../include/string.h"
#include "../include/types.h"
#include "../include/shell.h"
#include "../../src/micropython_embed/port/micropython_embed.h"

static char mp_heap[64 * 1024];

void python_repl(void) {
    int stack_top;
    mp_embed_init(&mp_heap[0], sizeof(mp_heap), &stack_top);
    mp_embed_exec_str("def input(prompt=\"\"):\n  import sys\n  if prompt: print(prompt, end=\"\")\n  return sys.stdin.readline().rstrip(\"\\n\")");

    video_print("MicroPython REPL - exit() ile cik", cursor_x, cursor_y, 14);
    cursor_y += font_height;
    cursor_x = splits[active_split].x + 5;

    char line[256];
    while (1) {
        video_print(">>> ", cursor_x, cursor_y, 11);
        cursor_x += 4 * font_width;

        int pos = 0;
        while (1) {
            char ch = read_key();
            if (ch == '\n') {
                line[pos] = '\0';
                cursor_y += font_height;
                cursor_x = splits[active_split].x + 5;
                break;
            } else if (ch == '\b') {
                if (pos > 0) {
                    pos--;
                    cursor_x -= font_width;
                    video_clear_rect(cursor_x, cursor_y, font_width, font_height);
                }
            } else if (ch >= ' ' && pos < 255) {
                line[pos++] = ch;
                video_draw_char(ch, cursor_x, cursor_y, 15);
                cursor_x += font_width;
            }
        }

        if (strcmp(line, "exit()") == 0) break;
        if (pos == 0) continue;

        mp_embed_exec_str(line);
    }

    mp_embed_deinit();
}

void python_run_file(const char *filename) {
    /* Dosyayı fs'den oku */
    extern fs_control_block g_fs;
    
    char content[4096];
    int result = fs_read_file(&g_fs, filename, content, sizeof(content));
    
    if (result < 0) {
        video_print("python: dosya bulunamadi: ", cursor_x, cursor_y, 12);
        video_print(filename, cursor_x + 25 * font_width, cursor_y, 12);
        cursor_y += font_height;
        cursor_x = splits[active_split].x + 5;
        return;
    }

    int stack_top;
    mp_embed_init(&mp_heap[0], sizeof(mp_heap), &stack_top);
    mp_embed_exec_str(
    "import sys\n"
);
    mp_embed_exec_str("def input(prompt=\"\"):\n  import sys\n  if prompt: print(prompt, end=\"\")\n  return sys.stdin.readline().rstrip(\"\\n\")");
    mp_embed_exec_str(content);
    mp_embed_deinit();
}
