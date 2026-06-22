/*
 * MAIN.C - cofeuOS Kernel Entry Point
 */

#include "../include/types.h"
#include "../include/io.h"
#include "../include/memory.h"
#include "../include/fs.h"
#include "../include/shell.h"
#include "../include/video.h"
#include "../include/string.h"

shell_control g_shell;
fs_control_block g_fs;
memory_arena g_mem_arena;

#include "../include/keyboard.h"

static void next_line(void) {
    cursor_x = splits[active_split].x + 5;
    cursor_y += LINE_HEIGHT;
    if (cursor_y > splits[active_split].y + splits[active_split].h - LINE_HEIGHT) {
        video_scroll_rect(splits[active_split].x, splits[active_split].y, splits[active_split].w, splits[active_split].h);
        cursor_y = splits[active_split].y + splits[active_split].h - LINE_HEIGHT;
    }
}

static void erase_at(int x, int y) {
    if (x >= splits[active_split].x && y >= splits[active_split].y) {
        video_clear_rect(x, y, font_width, font_height);
    }
}

static int get_login_input(char* buffer, int max_len, int start_x, int y, int masked) {
    int x = start_x;
    int len = 0;

    while (1) {
        char ch = read_key();
        if (ch == '\n') {
            buffer[len] = '\0';
            return len;
        }
        if (ch == '\b') {
            if (len > 0) {
                len--;
                x -= font_width;
                if (x < start_x) {
                    x = start_x;
                }
                erase_at(x, y);
            }
            continue;
        }
        if (ch >= ' ' && len < max_len - 1) {
            buffer[len++] = ch;
            video_draw_char(masked ? '*' : ch, x, y, 15);
            x += font_width;
        }
    }
}

static void show_login(void) {
    char username[32];
    char password[32];

    while (1) {
        video_clear(0);

        video_print("cofeuOS Login", 5, 20, 14);
        video_print("====================", 5, 20 + font_height + 2, 7);

        int login_y = 20 + font_height + 2 + font_height + 5;
        video_print("Login: ", 5, login_y, 15);
        int login_x = 5 + 7 * font_width;

        int ulen = get_login_input(username, sizeof(username), login_x, login_y, 0);
        if (ulen == 0) {
            video_print("Username required", 5, login_y + font_height + 5, 12);
            continue;
        }

        int pass_y = login_y + font_height + 5;
        video_print("Password: ", 5, pass_y, 15);
        int pass_x = 5 + 10 * font_width;
        get_login_input(password, sizeof(password), pass_x, pass_y, 1);

        strcpy(g_shell.user, username);
        cursor_x = 5;
        cursor_y = pass_y + font_height + 5;
        return;
    }
}

int main_get_input(char* buffer, int max_len) {
    int pos = splits[active_split].cmd_pos;

    while (1) {
        char ch = read_key();
        
        if (ch == 16) { // Ctrl+P
            splits[active_split].cmd_pos = pos;
            if (num_splits == 1) {
                splits[0].h = SCREEN_HEIGHT / 2;
                splits[1].x = 0;
                splits[1].y = SCREEN_HEIGHT / 2;
                splits[1].w = SCREEN_WIDTH;
                splits[1].h = SCREEN_HEIGHT / 2;
                splits[1].cx = 5;
                splits[1].cy = splits[1].y + 5;
                splits[1].active = 0;
                splits[1].cmd_pos = 0;
                splits[1].cmd_buf[0] = '\0';
                splits[1].needs_prompt = 1;
                num_splits = 2;
                
                video_fill_rect(0, splits[1].y - 2, SCREEN_WIDTH, 2, 8);
                active_split = 1;
            } else {
                active_split = (active_split + 1) % 2;
            }
            return -2;
        }
        
        if (ch == 24) { // Ctrl+X
            if (num_splits == 2) {
                num_splits = 1;
                active_split = 0;
                splits[0].h = SCREEN_HEIGHT;
                video_clear(0);
                splits[0].cx = 5;
                splits[0].cy = 5;
                splits[0].cmd_pos = 0;
                splits[0].cmd_buf[0] = '\0';
                splits[0].needs_prompt = 1;
                return -2;
            }
        }

        if (ch == '\n') {
            buffer[pos] = '\0';
            splits[active_split].cmd_pos = 0;
            next_line();
            return pos;
        }
        if (ch == '\b') {
            if (pos > 0) {
                pos--;
                if (cursor_x > splits[active_split].x + 5) {
                    cursor_x -= font_width;
                } else {
                    cursor_x = splits[active_split].x + 5;
                }
                erase_at(cursor_x, cursor_y);
            }
            continue;
        }
        if (ch >= ' ' && pos < max_len - 1) {
            if (cursor_x >= splits[active_split].x + splits[active_split].w - font_width) {
                next_line();
            }
            buffer[pos++] = ch;
            video_draw_char(ch, cursor_x, cursor_y, 15);
            cursor_x += font_width;
        }
    }
}

static void print_shell_prompt(void) {
    int x = splits[active_split].x + 5;
    video_print(g_shell.user, x, cursor_y, 10);
    x += video_text_width(g_shell.user);

    video_print("@", x, cursor_y, 10);
    x += font_width;

    video_print(g_shell.host, x, cursor_y, 14);
    x += video_text_width(g_shell.host);

    video_print(" [", x, cursor_y, 11);
    x += video_text_width(" [");

    video_print(g_shell.partition, x, cursor_y, 11);
    x += video_text_width(g_shell.partition);

    video_print("] ", x, cursor_y, 11);
    x += video_text_width("] ");

    video_print(g_shell.cwd, x, cursor_y, 12);
    x += video_text_width(g_shell.cwd);

    video_print(" # ", x, cursor_y, 15);
    x += video_text_width(" # ");
    cursor_x = x;
}

void kernel_main(vbe_mode_info_t* vbe_info) {
    video_init(vbe_info);
    video_clear(0);

    void* mem_base = (void*)0x100000;
    mem_init(&g_mem_arena, mem_base, 16 * 1024 * 1024);
    fs_init(&g_fs);

    splits[0].x = 0;
    splits[0].y = 0;
    splits[0].w = SCREEN_WIDTH;
    splits[0].h = SCREEN_HEIGHT;
    splits[0].cx = 5;
    splits[0].cy = 5;
    splits[0].active = 1;
    splits[0].needs_prompt = 1;
    splits[0].cmd_pos = 0;
    splits[0].cmd_buf[0] = '\0';
    num_splits = 1;
    active_split = 0;

    video_print("cofeuOS v3.0 - Boot", 5, 5, 14);
    video_print("====================", 5, 5 + font_height + 2, 7);

    strcpy(g_shell.host, "cofeu");
    strcpy(g_shell.partition, "/dev/sda1");
    strcpy(g_shell.cwd, "/");

    cursor_x = 5;
    cursor_y = 5 + font_height + 2 + font_height + 8;

    show_login();

    while (1) {
        if (splits[active_split].needs_prompt) {
            next_line();
            print_shell_prompt();
            splits[active_split].needs_prompt = 0;
        }
        int len = main_get_input(splits[active_split].cmd_buf, sizeof(splits[0].cmd_buf));
        if (len == -2) {
            continue;
        }
        if (len > 0) {
            shell_execute(splits[active_split].cmd_buf);
        }
        splits[active_split].cmd_pos = 0;
        splits[active_split].needs_prompt = 1;
    }
}
