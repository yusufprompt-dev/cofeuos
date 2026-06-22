/*
 * SHELL.C - cofeuOS Unix-Like Shell (Build Fixed)
 */

#include "../include/shell.h"
#include "../include/video.h"
#include "../include/fs.h"
#include "../include/string.h"
#include "../include/io.h"
#include "../include/types.h"
#include "../include/games.h"
#include "../include/python.h"
#include "../include/keyboard.h"

extern shell_control g_shell;
extern fs_control_block g_fs;

shell_split_t splits[MAX_SPLITS];
int active_split = 0;
int num_splits = 1;

int main_get_input(char* buffer, int max_len);

/* Prototypes */
static void shell_print(const char* str, u8 color);
static void shell_newline(void);
static int shell_tokenize(const char* cmd, char** args, int max_args);
static int cmd_ls(int argc, char** argv);
static int cmd_cat(int argc, char** argv);
static int cmd_pwd(int argc, char** argv);
static int cmd_cd(int argc, char** argv);
static int cmd_whoami(int argc, char** argv);
static int cmd_uname(int argc, char** argv);
static int cmd_clear(int argc, char** argv);
static int cmd_neofetch(int argc, char** argv);
static int cmd_vim(int argc, char** argv);
static int cmd_ifconfig(int argc, char** argv);
static int cmd_ping(int argc, char** argv);
static int cmd_reboot(int argc, char** argv);
static int cmd_halt(int argc, char** argv);
static int cmd_touch(int argc, char** argv);
static int cmd_mkdir(int argc, char** argv);
static int cmd_rm(int argc, char** argv);
static int cmd_rmdir(int argc, char** argv);
static int cmd_nano(int argc, char** argv);
static int cmd_help(int argc, char** argv);
static int cmd_date(int argc, char** argv);
static int cmd_uptime(int argc, char** argv);
static int cmd_free(int argc, char** argv);
static int cmd_ps(int argc, char** argv);
static int cmd_df(int argc, char** argv);
static int cmd_echo(int argc, char** argv);
static int cmd_env(int argc, char** argv);
static int cmd_rodo(int argc, char** argv);
static int cmd_sudo(int argc, char** argv);
static int cmd_pacman(int argc, char** argv);
static int cmd_apps(int argc, char** argv);
static int cmd_about(int argc, char** argv);
static int cmd_sysinfo(int argc, char** argv);
static int cmd_calc(int argc, char** argv);
static int cmd_write(int argc, char** argv);
static int cmd_desktop(int argc, char** argv);
static int cmd_theme(int argc, char** argv);
static int cmd_split(int argc, char** argv);

int shell_execute(const char* cmd);

static void shell_print(const char* str, u8 color) {
    int x = cursor_x;
    int y = cursor_y;

    for (const char* p = str; *p; p++) {
        if (*p == '\n') {
            x = splits[active_split].x + 5;
            y += LINE_HEIGHT;
            if (y > splits[active_split].y + splits[active_split].h - LINE_HEIGHT) {
                video_scroll_rect(splits[active_split].x, splits[active_split].y, splits[active_split].w, splits[active_split].h);
                y = splits[active_split].y + splits[active_split].h - LINE_HEIGHT;
            }
            continue;
        }

        video_draw_char(*p, x, y, color);
        x += CHAR_WIDTH;
        if (x > splits[active_split].x + splits[active_split].w - CHAR_WIDTH) {
            x = splits[active_split].x + 5;
            y += LINE_HEIGHT;
            if (y > splits[active_split].y + splits[active_split].h - CHAR_HEIGHT) {
                video_scroll_rect(splits[active_split].x, splits[active_split].y, splits[active_split].w, splits[active_split].h);
                y = splits[active_split].y + splits[active_split].h - CHAR_HEIGHT;
            }
        }
    }

    cursor_x = x;
    cursor_y = y;
}

static void shell_newline(void) {
    cursor_x = splits[active_split].x + 5;
    cursor_y += LINE_HEIGHT;
    if (cursor_y > splits[active_split].y + splits[active_split].h - LINE_HEIGHT) {
        video_scroll_rect(splits[active_split].x, splits[active_split].y, splits[active_split].w, splits[active_split].h);
        cursor_y = splits[active_split].y + splits[active_split].h - LINE_HEIGHT;
    }
}

static int shell_tokenize(const char* cmd, char** args, int max_args) {
    static char buf[512];
    strcpy(buf, cmd);
    int i = 0;
    char* token = strtok(buf, " \t\n");
    while (token && i < max_args - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;
    return i;
}

int shell_execute(const char* cmd) {
    char* args[16];
    int argc = shell_tokenize(cmd, args, 16);
    if (!argc) return 0;

    if (strcmp(args[0], "help") == 0) return cmd_help(argc, args);
    if (strcmp(args[0], "ls") == 0) return cmd_ls(argc, args);
    if (strcmp(args[0], "cat") == 0) return cmd_cat(argc, args);
    if (strcmp(args[0], "pwd") == 0) return cmd_pwd(argc, args);
    if (strcmp(args[0], "cd") == 0) return cmd_cd(argc, args);
    if (strcmp(args[0], "whoami") == 0) return cmd_whoami(argc, args);
    if (strcmp(args[0], "uname") == 0) return cmd_uname(argc, args);
    if (strcmp(args[0], "clear") == 0) return cmd_clear(argc, args);
    if (strcmp(args[0], "neofetch") == 0) return cmd_neofetch(argc, args);
    if (strcmp(args[0], "reboot") == 0) return cmd_reboot(argc, args);
    if (strcmp(args[0], "halt") == 0) return cmd_halt(argc, args);
    if (strcmp(args[0], "touch") == 0) return cmd_touch(argc, args);
    if (strcmp(args[0], "mkdir") == 0) return cmd_mkdir(argc, args);
    if (strcmp(args[0], "rm") == 0) return cmd_rm(argc, args);
    if (strcmp(args[0], "rmdir") == 0) return cmd_rmdir(argc, args);
    if (strcmp(args[0], "nano") == 0) return cmd_nano(argc, args);
    if (strcmp(args[0], "vim") == 0) return cmd_vim(argc, args);
    if (strcmp(args[0], "rodo") == 0) return cmd_rodo(argc, args);
    if (strcmp(args[0], "sudo") == 0) return cmd_sudo(argc, args);
    if (strcmp(args[0], "pacman") == 0) return cmd_pacman(argc, args);
    if (strcmp(args[0], "ifconfig") == 0) return cmd_ifconfig(argc, args);
    if (strcmp(args[0], "ping") == 0) return cmd_ping(argc, args);
    if (strcmp(args[0], "date") == 0) return cmd_date(argc, args);
    if (strcmp(args[0], "uptime") == 0) return cmd_uptime(argc, args);
    if (strcmp(args[0], "free") == 0) return cmd_free(argc, args);
    if (strcmp(args[0], "ps") == 0) return cmd_ps(argc, args);
    if (strcmp(args[0], "df") == 0) return cmd_df(argc, args);
    if (strcmp(args[0], "echo") == 0) return cmd_echo(argc, args);
    if (strcmp(args[0], "env") == 0) return cmd_env(argc, args);
    if (strcmp(args[0], "apps") == 0) return cmd_apps(argc, args);
    if (strcmp(args[0], "about") == 0) return cmd_about(argc, args);
    if (strcmp(args[0], "sysinfo") == 0) return cmd_sysinfo(argc, args);
    if (strcmp(args[0], "calc") == 0) return cmd_calc(argc, args);
    if (strcmp(args[0], "write") == 0) return cmd_write(argc, args);
    if (strcmp(args[0], "desktop") == 0) return cmd_desktop(argc, args);
    if (strcmp(args[0], "startx") == 0) return cmd_desktop(argc, args);
    if (strcmp(args[0], "theme") == 0) return cmd_theme(argc, args);
    if (strcmp(args[0], "split") == 0) return cmd_split(argc, args);
    if (strcmp(args[0], "python") == 0) { python_repl(); return 0; }
    if (strcmp(args[0], "python3") == 0) { python_repl(); return 0; }
 
    
    shell_print("cofeuOS: '", 12);
    shell_print(args[0], 12);
    shell_print("': unknown command", 12);
    shell_newline();
    return -1;
}

static int cmd_help(int argc, char** argv) {
    shell_print("cofeuOS Unix Shell Komutlari:\n", 14);
    shell_newline();
    shell_print("Dosya: ls cat pwd cd touch mkdir rm rmdir\n", 15);
    shell_newline();
    shell_print("Dosya: write nano vim\n", 15);
    shell_newline();
    shell_print("Sistem: whoami uname clear date uptime free ps df echo env sysinfo\n", 15);
    shell_newline();
    shell_print("Diger: neofetch calc apps about theme desktop startx rodo reboot halt\n", 15);
    shell_newline();
    shell_print("Paket: pacman\n", 15);
    shell_newline();
    shell_print("Ag: ifconfig ping\n", 15);
    shell_newline();
    shell_newline();
    shell_print("Python: python / python3\n", 11);
    shell_newline();
    return 0;
}

static int cmd_ls(int argc, char** argv) {
    char buf[1024];
    const char* p = argc > 1 ? argv[1] : g_shell.cwd;
    int sz = fs_list_dir(&g_fs, p, buf, 1024);
    if (sz < 0) {
        shell_print("ls: no such dir: ", 12);
        shell_print(p, 12);
        shell_newline();
        return -1;
    }
    shell_print(buf[0] ? buf : "(empty)", 11);
    shell_newline();
    return 0;
}

static int cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        shell_print("cat: missing file", 12);
        shell_newline();
        return -1;
    }
    char res[256], buf[4096];
    fs_resolve_path(g_shell.cwd, argv[1], res);
    int sz = fs_read_file(&g_fs, res, buf, 4096);
    if (sz < 0) {
        shell_print("cat: ", 12);
        shell_print(argv[1], 12);
        shell_print(": no such file", 12);
        shell_newline();
        return -1;
    }
    buf[sz] = 0;
    shell_print(buf, 15);
    shell_newline();
    return 0;
}

static int cmd_pwd(int argc, char** argv) {
    shell_print(g_shell.cwd, 11);
    shell_newline();
    return 0;
}

static int cmd_cd(int argc, char** argv) {
    const char* p = argc > 1 ? argv[1] : "/";
    char res[256];
    fs_resolve_path(g_shell.cwd, p, res);
    if (!fs_dir_exists(&g_fs, res)) {
        shell_print("cd: no such directory: ", 12);
        shell_print(p, 12);
        shell_newline();
        return -1;
    }
    strcpy(g_shell.cwd, res);
    return 0;
}

static int cmd_whoami(int argc, char** argv) {
    shell_print(g_shell.user, 10);
    shell_newline();
    return 0;
}

static int cmd_uname(int argc, char** argv) {
    shell_print("cofeuOS v3.0 x86_32", 14);
    shell_newline();
    return 0;
}

static int cmd_clear(int argc, char** argv) {
    video_clear_rect(splits[active_split].x, splits[active_split].y, splits[active_split].w, splits[active_split].h);
    cursor_x = splits[active_split].x + 5;
    cursor_y = splits[active_split].y + 5;
    return 0;
}

static int cmd_split(int argc, char** argv) {
    (void)argc; (void)argv;
    shell_print("Press Ctrl+P to split screen, and Ctrl+X to close the active split.", 14);
    shell_newline();
    return 0;
}

static int cmd_neofetch(int argc, char** argv) {
    (void)argc; (void)argv;
    
    // Ensure 9 lines fit without scrolling during animation
    while (cursor_y + 9 * LINE_HEIGHT > splits[active_split].y + splits[active_split].h) {
        video_scroll_rect(splits[active_split].x, splits[active_split].y, splits[active_split].w, splits[active_split].h);
        cursor_y -= LINE_HEIGHT;
    }
    
    int frame = 0;
    int start_y = cursor_y;
    int start_x = cursor_x;
    
    while (1) {
        cursor_y = start_y;
        cursor_x = start_x;
        
        const char* smoke_f1[3] = {
            "    )  (      ",
            "   (   ) )    ",
            "    ) ( (     "
        };
        const char* smoke_f2[3] = {
            "   (    )     ",
            "    )  ( (    ",
            "   (   ) )    "
        };
        const char* smoke_f3[3] = {
            "  (      )    ",
            "   )    (     ",
            "  (    ) )    "
        };
        
        const char** smoke = smoke_f1;
        if (frame % 3 == 1) smoke = smoke_f2;
        if (frame % 3 == 2) smoke = smoke_f3;
        
        shell_print(smoke[0], 14); shell_print(g_shell.user, 12); shell_print("@", 15); shell_print("cofeu", 14); shell_newline();
        shell_print(smoke[1], 14); shell_print("----------", 8); shell_newline();
        shell_print(smoke[2], 14); shell_print("OS: ", 11); shell_print("cofeuOS v3.0", 15); shell_newline();
        shell_print("  _______)_   ", 15); shell_print("Kernel: ", 11); shell_print("x86_32 i686", 15); shell_newline();
        shell_print(".-'---------| ", 15); shell_print("Shell: ", 11); shell_print("Cofeu Shell", 15); shell_newline();
        shell_print("( C|/\\/\\/\\/\\| ", 15); shell_print("Resolution: ", 11); shell_print("320x200", 15); shell_newline();
        shell_print(" '-./\\/\\/\\/\\| ", 15); shell_print("Memory: ", 11); shell_print("16MB", 15); shell_newline();
        shell_print("   '________' ", 15); shell_print("Disk: ", 11); shell_print("/dev/sda1", 15); shell_newline();
        shell_print("    '-------' ", 15); shell_print("Uptime: ", 11); shell_print("0 days", 15); shell_newline();
        
        for (volatile int i=0; i<3000000; i++) {} // simple delay
        frame++;
        
        if (try_read_key()) {
            break;
        }
    }
    
    return 0;
}

static int cmd_text_editor(int argc, char** argv, const char* editor_name) {
    shell_print("========================================", 14);
    shell_newline();
    shell_print(editor_name, 14);
    shell_print(" editor - :w = save, :q = quit, :wq = save+quit", 14);
    shell_newline();

    char path[MAX_PATH_LEN] = "";
    if (argc > 1) {
        fs_resolve_path(g_shell.cwd, argv[1], path);
    }
    shell_print("File: ", 7);
    shell_print(path[0] ? path : "<no file>", 15);
    shell_newline();
    shell_print("========================================", 14);
    shell_newline();

    char buffer[FILE_CONTENT_SIZE];
    int len = 0;
    if (path[0]) {
        int sz = fs_read_file(&g_fs, path, buffer, sizeof(buffer) - 1);
        if (sz >= 0) {
            buffer[sz] = '\0';
            len = sz;
            shell_print("Loaded file content. Append lines and save with :wq.", 15);
            shell_newline();
        } else {
            buffer[0] = '\0';
            shell_print("New file will be created. Use :wq to save.", 15);
            shell_newline();
        }
    } else {
        buffer[0] = '\0';
        shell_print("No file specified. Use :q to exit.", 15);
        shell_newline();
    }

    char line[80];
    while (1) {
        cursor_x = 5;
        video_print("> ", 7, cursor_y, 7);
        cursor_x = 25;
        main_get_input(line, 80);

        if (line[0] == ':' && strcmp(line, ":q") == 0) {
            break;
        }
        if (line[0] == ':' && strcmp(line, ":wq") == 0) {
            if (path[0]) {
                fs_write_file(&g_fs, path, buffer, len);
                shell_print("Saved file.", 10);
                shell_newline();
            } else {
                shell_print("No filename given. Use nano <file> or vim <file>.", 12);
                shell_newline();
            }
            break;
        }
        if (line[0] == ':' && strcmp(line, ":w") == 0) {
            if (path[0]) {
                fs_write_file(&g_fs, path, buffer, len);
                shell_print("Saved file.", 10);
                shell_newline();
            } else {
                shell_print("No filename given. Use nano <file> or vim <file>.", 12);
                shell_newline();
            }
            continue;
        }

        int line_len = strlen(line);
        if (len + line_len + 1 < (int)sizeof(buffer)) {
            memcpy(buffer + len, line, line_len);
            len += line_len;
            buffer[len++] = '\n';
            buffer[len] = '\0';
            shell_print(line, 15);
            shell_newline();
        } else {
            shell_print("Editor buffer full.", 12);
            shell_newline();
            break;
        }
    }

    shell_print(editor_name, 14);
    shell_print(" exited.", 14);
    shell_newline();
    return 0;
}

static int cmd_vim(int argc, char** argv) {
    return cmd_text_editor(argc, argv, "vim");
}

static int cmd_reboot(int argc, char** argv) {
    shell_print("Rebooting cofeuOS...", 14);
    shell_newline();
    outb(0x64, 0xFE);
    while(1);
    return 0;
}

static int cmd_halt(int argc, char** argv) {
    shell_print("cofeuOS halted.", 12);
    shell_newline();
    while(1);
    return 0;
}

static int cmd_ifconfig(int argc, char** argv) {
    shell_print("eth0: no hardware driver installed", 12);
    shell_newline();
    shell_print("IP: 0.0.0.0", 12);
    shell_newline();
    shell_print("Netmask: 255.255.255.0", 12);
    shell_newline();
    shell_print("Gateway: 0.0.0.0", 12);
    shell_newline();
    shell_print("Note: Ethernet support requires NIC driver and TCP/IP stack.", 12);
    shell_newline();
    return 0;
}

static int cmd_ping(int argc, char** argv) {
    if (argc < 2) {
        shell_print("ping: hostname required", 12);
        shell_newline();
        return -1;
    }
    shell_print("ping: network stack not implemented yet", 12);
    shell_newline();
    return -1;
}

static int cmd_touch(int argc, char** argv) {
    if (argc < 2) {
        shell_print("touch: filename required", 12);
        shell_newline();
        return -1;
    }
    char res[256];
    fs_resolve_path(g_shell.cwd, argv[1], res);
    fs_create_file(&g_fs, res, "", 0);
    shell_print("Created file: ", 10);
    shell_print(argv[1], 10);
    shell_newline();
    return 0;
}

static int cmd_mkdir(int argc, char** argv) {
    if (argc < 2) {
        shell_print("mkdir: dirname required", 12);
        shell_newline();
        return -1;
    }
    char res[256];
    fs_resolve_path(g_shell.cwd, argv[1], res);
    fs_create_dir(&g_fs, res);
    shell_print("Created directory: ", 10);
    shell_print(argv[1], 10);
    shell_newline();
    return 0;
}

static int cmd_rm(int argc, char** argv) {
    if (argc < 2) {
        shell_print("rm: filename required", 12);
        shell_newline();
        return -1;
    }
    char res[256];
    fs_resolve_path(g_shell.cwd, argv[1], res);
    fs_delete_file(&g_fs, res);
    shell_print("Removed: ", 10);
    shell_print(argv[1], 10);
    shell_newline();
    return 0;
}

static int cmd_rodo(int argc, char** argv) {
    if (argc < 2) {
        shell_print("rodo: command required", 12);
        shell_newline();
        return -1;
    }

    shell_print("rodo: running as root", 10);
    shell_newline();

    char command[512];
    int pos = 0;
    for (int i = 1; i < argc; i++) {
        int len = strlen(argv[i]);
        if (pos + len + 2 >= (int)sizeof(command)) break;
        strcpy(&command[pos], argv[i]);
        pos += len;
        if (i < argc - 1) {
            command[pos++] = ' ';
        }
    }
    command[pos] = '\0';
    return shell_execute(command);
}

static int cmd_sudo(int argc, char** argv) {
    return cmd_rodo(argc, argv);
}

static int cmd_pacman(int argc, char** argv) {
    if (argc < 2) {
        shell_print("pacman: usage: pacman -S <pkg> | -Ss <pkg> | -Sy | -Syu", 12);
        shell_newline();
        return -1;
    }

    if (strcmp(argv[1], "-S") == 0) {
        if (argc < 3) {
            shell_print("pacman: package name required", 12);
            shell_newline();
            return -1;
        }
        shell_print("installing package: ", 10);
        shell_print(argv[2], 10);
        shell_newline();
        return 0;
    }

    if (strcmp(argv[1], "-Ss") == 0) {
        if (argc < 3) {
            shell_print("pacman: package search required", 12);
            shell_newline();
            return -1;
        }
        shell_print("search results for: ", 11);
        shell_print(argv[2], 11);
        shell_newline();
        shell_print("community/" , 10); shell_print(argv[2], 10); shell_newline();
        return 0;
    }

    if (strcmp(argv[1], "-Sy") == 0) {
        shell_print("syncing package databases... done", 10);
        shell_newline();
        return 0;
    }

    if (strcmp(argv[1], "-Syu") == 0) {
        shell_print("synchronizing package databases and upgrading... done", 10);
        shell_newline();
        return 0;
    }

    shell_print("pacman: unsupported operation", 12);
    shell_newline();
    return -1;
}

static int cmd_rmdir(int argc, char** argv) {
    if (argc < 2) {
        shell_print("rmdir: dirname required", 12);
        shell_newline();
        return -1;
    }
    char res[256];
    fs_resolve_path(g_shell.cwd, argv[1], res);
    fs_delete_dir(&g_fs, res);
    shell_print("Removed directory: ", 10);
    shell_print(argv[1], 10);
    shell_newline();
    return 0;
}

static int cmd_nano(int argc, char** argv) {
    return cmd_text_editor(argc, argv, "nano");
}

static int shell_atoi(const char* s) {
    int sign = 1;
    int value = 0;

    if (*s == '-') {
        sign = -1;
        s++;
    }

    while (*s >= '0' && *s <= '9') {
        value = value * 10 + (*s - '0');
        s++;
    }

    return value * sign;
}

static void shell_print_int(int value, u8 color) {
    char buf[16];
    int pos = 0;

    if (value == 0) {
        shell_print("0", color);
        return;
    }

    if (value < 0) {
        shell_print("-", color);
        value = -value;
    }

    while (value > 0 && pos < (int)sizeof(buf)) {
        buf[pos++] = '0' + (value % 10);
        value /= 10;
    }

    while (pos > 0) {
        char out[2] = { buf[--pos], '\0' };
        shell_print(out, color);
    }
}

static void shell_join_args(char** argv, int start, int argc, char* out, int out_size) {
    int pos = 0;
    for (int i = start; i < argc; i++) {
        int len = strlen(argv[i]);
        if (pos + len + 2 >= out_size) {
            break;
        }
        strcpy(&out[pos], argv[i]);
        pos += len;
        if (i < argc - 1) {
            out[pos++] = ' ';
        }
    }
    out[pos] = '\0';
}

static void desktop_text(const char* str, int x, int y, u8 color) {
    video_print(str, x, y, color);
}

static void desktop_draw_window(int x, int y, int w, int h, const char* title, u8 body_color) {
    video_fill_rect(x, y, w, h, body_color);
    video_fill_rect(x, y, w, 14, 1);
    video_draw_rect(x, y, w, h, 15);
    desktop_text(title, x + 5, y + 3, 15);
    video_fill_rect(x + w - 13, y + 3, 8, 8, 12);
}

static void desktop_draw_base(const char* status) {
    video_clear(1);
    video_fill_rect(0, 0, SCREEN_WIDTH, 18, 9);
    video_fill_rect(0, SCREEN_HEIGHT - 20, SCREEN_WIDTH, 20, 8);
    desktop_text("cofeuDE", 6, 5, 15);
    desktop_text("Desktop", SCREEN_WIDTH - 70, 5, 15);

    video_fill_rect(10, 28, 34, 34, 3);
    video_draw_rect(10, 28, 34, 34, 15);
    desktop_text("Files", 8, 66, 15);

    video_fill_rect(58, 28, 34, 34, 5);
    video_draw_rect(58, 28, 34, 34, 15);
    desktop_text("Notes", 55, 66, 15);

    video_fill_rect(106, 28, 34, 34, 6);
    video_draw_rect(106, 28, 34, 34, 15);
    desktop_text("Info", 108, 66, 15);

    desktop_draw_window(152, 34, SCREEN_WIDTH - 164, SCREEN_HEIGHT - 74, "Welcome", 7);
    desktop_text("apps: files notes info term exit", 160, 54, 0);
    desktop_text("type a desktop command below", 160, 68, 0);

    desktop_text(status, 6, SCREEN_HEIGHT - 15, 15);
}

static void desktop_show_files(void) {
    char buf[512];
    int sz = fs_list_dir(&g_fs, g_shell.cwd, buf, sizeof(buf));

    desktop_draw_base("Files app");
    desktop_draw_window(48, 82, SCREEN_WIDTH - 96, 72, "Files", 7);
    desktop_text("Path:", 56, 104, 0);
    desktop_text(g_shell.cwd, 94, 104, 1);
    desktop_text(sz >= 0 && buf[0] ? buf : "(empty)", 56, 120, 0);
}

static void desktop_show_notes(void) {
    char note[256];
    int sz = fs_read_file(&g_fs, "/home/notes.txt", note, sizeof(note) - 1);

    desktop_draw_base("Notes app");
    desktop_draw_window(38, 82, SCREEN_WIDTH - 76, 76, "Notes", 7);
    if (sz >= 0) {
        note[sz] = '\0';
        desktop_text(note, 46, 104, 0);
    } else {
        desktop_text("No notes yet. Use: write /home/notes.txt hello", 46, 104, 0);
    }
}

static void desktop_show_info(void) {
    desktop_draw_base("System info");
    desktop_draw_window(48, 82, SCREEN_WIDTH - 96, 84, "System", 7);
    desktop_text("cofeuOS v3.0", 58, 104, 0);
    desktop_text("Kernel: x86 protected mode", 58, 118, 0);
    desktop_text("Video: VESA framebuffer", 58, 132, 0);
    desktop_text("Shell: cofeu shell + cofeuDE", 58, 146, 0);
}

static int cmd_apps(int argc, char** argv) {
    shell_print("Installed apps:\n", 14);
    shell_newline();
    shell_print("desktop/startx  graphical desktop session\n", 15);
    shell_newline();
    shell_print("files           built into desktop\n", 15);
    shell_newline();
    shell_print("notes           built into desktop, reads /home/notes.txt\n", 15);
    shell_newline();
    shell_print("calc write nano vim sysinfo neofetch pacman\n", 15);
    shell_newline();
    return 0;
}

static int cmd_about(int argc, char** argv) {
    shell_print("cofeuOS v3.0\n", 14);
    shell_newline();
    shell_print("A tiny protected-mode x86 OS with VESA graphics, shell, RAM fs and cofeuDE.", 15);
    shell_newline();
    return 0;
}

static int cmd_sysinfo(int argc, char** argv) {
    shell_print("System information:\n", 14);
    shell_newline();
    shell_print("OS: cofeuOS v3.0", 11);
    shell_newline();
    shell_print("Kernel: i386 protected mode", 11);
    shell_newline();
    shell_print("Video: VESA framebuffer ", 11);
    shell_print_int(SCREEN_WIDTH, 11);
    shell_print("x", 11);
    shell_print_int(SCREEN_HEIGHT, 11);
    shell_print("x", 11);
    shell_print_int(screen_bpp, 11);
    shell_newline();
    shell_print("Memory arena: 16MB", 11);
    shell_newline();
    shell_print("Filesystem: in-memory cofeuFS", 11);
    shell_newline();
    return 0;
}

static int cmd_calc(int argc, char** argv) {
    if (argc < 4) {
        shell_print("usage: calc <a> +|-|*|/ <b>", 12);
        shell_newline();
        return -1;
    }

    int a = shell_atoi(argv[1]);
    int b = shell_atoi(argv[3]);
    int result = 0;

    if (strcmp(argv[2], "+") == 0) result = a + b;
    else if (strcmp(argv[2], "-") == 0) result = a - b;
    else if (strcmp(argv[2], "*") == 0) result = a * b;
    else if (strcmp(argv[2], "/") == 0) {
        if (b == 0) {
            shell_print("calc: division by zero", 12);
            shell_newline();
            return -1;
        }
        result = a / b;
    } else {
        shell_print("calc: unknown operator", 12);
        shell_newline();
        return -1;
    }

    shell_print_int(result, 10);
    shell_newline();
    return 0;
}

static int cmd_write(int argc, char** argv) {
    if (argc < 3) {
        shell_print("usage: write <file> <text...>", 12);
        shell_newline();
        return -1;
    }

    char path[MAX_PATH_LEN];
    char text[512];
    fs_resolve_path(g_shell.cwd, argv[1], path);
    shell_join_args(argv, 2, argc, text, sizeof(text));
    fs_write_file(&g_fs, path, text, strlen(text));
    shell_print("wrote ", 10);
    shell_print(argv[1], 10);
    shell_newline();
    return 0;
}

static int cmd_theme(int argc, char** argv) {
    video_clear(0);
    video_fill_rect(0, 0, SCREEN_WIDTH, 16, 9);
    video_fill_rect(0, 16, SCREEN_WIDTH, 16, 3);
    video_fill_rect(0, 32, SCREEN_WIDTH, 16, 5);
    cursor_x = 5;
    cursor_y = 56;
    shell_print("Theme preview applied. Use clear to return to terminal background.", 14);
    shell_newline();
    return 0;
}

static int cmd_desktop(int argc, char** argv) {
    char input[64];

    fs_create_dir(&g_fs, "/home");
    if (!fs_file_exists(&g_fs, "/home/notes.txt")) {
        fs_create_file(&g_fs, "/home/notes.txt", "Welcome to cofeuDE.", 19);
    }

    desktop_draw_base("Ready");

    while (1) {
        cursor_x = 8;
        cursor_y = SCREEN_HEIGHT - 15;
        video_fill_rect(0, SCREEN_HEIGHT - 20, SCREEN_WIDTH, 20, 8);
        desktop_text("desktop> ", 6, SCREEN_HEIGHT - 15, 15);
        cursor_x = 78;
        main_get_input(input, sizeof(input));

        if (strcmp(input, "exit") == 0 || strcmp(input, "term") == 0 || strcmp(input, "shell") == 0) {
            video_clear(0);
            cursor_x = 5;
            cursor_y = 30;
            shell_print("Exited cofeuDE.", 10);
            shell_newline();
            return 0;
        }
        if (strcmp(input, "files") == 0) {
            desktop_show_files();
            continue;
        }
        if (strcmp(input, "notes") == 0) {
            desktop_show_notes();
            continue;
        }
        if (strcmp(input, "info") == 0 || strcmp(input, "about") == 0) {
            desktop_show_info();
            continue;
        }
        if (strcmp(input, "clear") == 0 || strcmp(input, "home") == 0) {
            desktop_draw_base("Ready");
            continue;
        }

        desktop_draw_base("Unknown app. Try files, notes, info, term, exit.");
    }
}

static int cmd_date(int argc, char** argv) { shell_print("Thu Jan 1 00:00:00 2025", 14); shell_newline(); return 0; }
static int cmd_uptime(int argc, char** argv) { shell_print("uptime 0 days", 14); shell_newline(); return 0; }
static int cmd_free(int argc, char** argv) { shell_print("free: 16MB total", 14); shell_newline(); return 0; }
static int cmd_ps(int argc, char** argv) { shell_print("PID 1 shell\nPID 2 cofeuDE-ready", 15); shell_newline(); return 0; }
static int cmd_df(int argc, char** argv) { shell_print("/dev/sda1 16MB 6% used", 15); shell_newline(); return 0; }
static int cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        shell_print(argv[i], 15);
        if (i < argc - 1) shell_print(" ", 15);
    }
    shell_newline();
    return 0;
}
static int cmd_env(int argc, char** argv) { shell_print("USER=", 15); shell_print(g_shell.user, 15); shell_print(" HOST=cofeu PATH=/bin SHELL=/bin/cofeush", 15); shell_newline(); return 0; }
