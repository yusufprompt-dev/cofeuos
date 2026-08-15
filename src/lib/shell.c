/*
 * SHELL.C - cofeuOS Unix-Like Shell (Build Fixed)
 */

#include "../include/shell.h"
shell_split_t splits[MAX_SPLITS];
int active_split = 0;
int num_splits = 1;
#include "../include/video.h"
#include "../include/fs.h"
#include "../include/string.h"
#include "../include/io.h"
#include "../include/types.h"
#include "../include/python.h"
#include "../include/keyboard.h"
#include "../include/network.h"
#include "../include/memory.h"
#include "../include/session.h"
#include "../include/js.h"


extern shell_control g_shell;
extern fs_control_block g_fs;

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
static int cmd_logout(int argc, char** argv);
static int cmd_uname(int argc, char** argv);
static int cmd_clear(int argc, char** argv);
static int cmd_neofetch(int argc, char** argv);
static int cmd_vim(int argc, char** argv);
static int cmd_ifconfig(int argc, char** argv);
static int cmd_ping(int argc, char** argv);
static int cmd_wget(int argc, char** argv);
static void shell_print_int(int value, u8 color);
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
static int cmd_curl(int argc, char** argv);
static int cmd_unzip(int argc, char** argv);
static int cmd_untar(int argc, char** argv);
static int cmd_nettest(int argc, char** argv);
static int cmd_nslookup(int argc, char** argv);
static int cmd_js(int argc, char** argv);

int shell_execute(const char* cmd);

static desktop_window_t *g_gui_term_target = NULL;

static void gui_term_print_char(desktop_window_t *win, char c, u8 color) {
    if (!win) return;
    if (c == '\n') {
        win->term_cx = 0;
        win->term_cy++;
        if (win->term_cy >= GUI_TERM_ROWS) {
            for (int r = 0; r < GUI_TERM_ROWS - 1; r++) {
                memcpy(win->term_screen[r], win->term_screen[r + 1], GUI_TERM_COLS + 1);
                memcpy(win->term_colors[r], win->term_colors[r + 1], GUI_TERM_COLS + 1);
            }
            memset(win->term_screen[GUI_TERM_ROWS - 1], 0, GUI_TERM_COLS + 1);
            memset(win->term_colors[GUI_TERM_ROWS - 1], 15, GUI_TERM_COLS + 1);
            win->term_cy = GUI_TERM_ROWS - 1;
        }
        return;
    }
    if (c == '\r') return;
    if (win->term_cx >= GUI_TERM_COLS) {
        win->term_cx = 0;
        win->term_cy++;
        if (win->term_cy >= GUI_TERM_ROWS) {
            for (int r = 0; r < GUI_TERM_ROWS - 1; r++) {
                memcpy(win->term_screen[r], win->term_screen[r + 1], GUI_TERM_COLS + 1);
                memcpy(win->term_colors[r], win->term_colors[r + 1], GUI_TERM_COLS + 1);
            }
            memset(win->term_screen[GUI_TERM_ROWS - 1], 0, GUI_TERM_COLS + 1);
            memset(win->term_colors[GUI_TERM_ROWS - 1], 15, GUI_TERM_COLS + 1);
            win->term_cy = GUI_TERM_ROWS - 1;
        }
    }
    win->term_screen[win->term_cy][win->term_cx] = c;
    win->term_colors[win->term_cy][win->term_cx] = color;
    win->term_cx++;
}

static void shell_print(const char* str, u8 color) {
    if (g_gui_term_target) {
        for (const char* p = str; *p; p++) {
            gui_term_print_char(g_gui_term_target, *p, color);
        }
        return;
    }

    int x = cursor_x;
    int y = cursor_y;

    for (const char* p = str; *p; p++) {
        if (*p == '\n') {
            x = 5;
            y += LINE_HEIGHT;
            if (y > SCREEN_HEIGHT - LINE_HEIGHT) {
                video_scroll();
                y = SCREEN_HEIGHT - LINE_HEIGHT;
            }
            continue;
        }

        video_draw_char(*p, x, y, color);
        x += CHAR_WIDTH;
        if (x > SCREEN_WIDTH - CHAR_WIDTH) {
            x = 5;
            y += LINE_HEIGHT;
            if (y > SCREEN_HEIGHT - CHAR_HEIGHT) {
                video_scroll();
                y = SCREEN_HEIGHT - CHAR_HEIGHT;
            }
        }
    }

    cursor_x = x;
    cursor_y = y;
}

static void shell_newline(void) {
    if (g_gui_term_target) {
        gui_term_print_char(g_gui_term_target, '\n', 15);
        return;
    }
    cursor_x = 5;
    cursor_y += LINE_HEIGHT;
    if (cursor_y > SCREEN_HEIGHT - LINE_HEIGHT) {
        video_scroll();
        cursor_y = SCREEN_HEIGHT - LINE_HEIGHT;
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
    dbg_write("[KRN] exec: ");
    dbg_write(cmd);
    dbg_write("\n");
    if (strcmp(args[0], "help") == 0) return cmd_help(argc, args);
    if (strcmp(args[0], "ls") == 0) return cmd_ls(argc, args);
    if (strcmp(args[0], "cat") == 0) return cmd_cat(argc, args);
    if (strcmp(args[0], "pwd") == 0) return cmd_pwd(argc, args);
    if (strcmp(args[0], "cd") == 0) return cmd_cd(argc, args);
    if (strcmp(args[0], "whoami") == 0) return cmd_whoami(argc, args);
    if (strcmp(args[0], "logout") == 0) return cmd_logout(argc, args);
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
    if (strcmp(args[0], "wget") == 0) return cmd_wget(argc, args);
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
    if (strcmp(args[0], "curl") == 0) return cmd_curl(argc, args);
    if (strcmp(args[0], "unzip") == 0) return cmd_unzip(argc, args);
    if (strcmp(args[0], "untar") == 0 || strcmp(args[0], "tar") == 0) return cmd_untar(argc, args);
    if (strcmp(args[0], "nettest") == 0) return cmd_nettest(argc, args);
    if (strcmp(args[0], "nslookup") == 0) return cmd_nslookup(argc, args);
    if (strcmp(args[0], "js") == 0) return cmd_js(argc, args);
    if (strcmp(args[0], "python") == 0 || strcmp(args[0], "python3") == 0) { if (argc > 1) { python_run_file(args[1]); } else { python_repl(); } return 0; }

    shell_print("cofeuOS: '", 12);
    shell_print(args[0], 12);
    shell_print("': unknown command", 12);

    return -1;
}

static int cmd_help(int argc, char** argv) {
    shell_print("cofeuOS Unix Shell Komutlari:\n", 14);
    shell_print("Dosya: ls cat pwd cd touch mkdir rm rmdir\n", 15);
    shell_print("Dosya: write nano vim\n", 15);
    shell_print("Arsiv: unzip untar\n", 15);
    shell_print("Sistem: whoami uname clear date uptime free ps df echo env sysinfo\n", 15);
    shell_print("Diger: neofetch calc apps about theme desktop startx rodo reboot halt\n", 15);
    shell_print("Paket: pacman\n", 15);
    shell_print("Ag: ifconfig nslookup nettest ping wget curl\n", 15);
    shell_print("Python: python / python3\n", 11);
    return 0;
}

static int cmd_ls(int argc, char** argv) {
    const char* p = argc > 1 ? argv[1] : g_shell.cwd;
    int found = 0;

    for (size_t i = 0; i < g_fs.dir_count; i++) {
        if (g_fs.dirs[i].active) {
            char parent[MAX_PATH_LEN];
            fs_get_parent_path(g_fs.dirs[i].path, parent);
            if (strcmp(parent, p) == 0) {
                const char *name = g_fs.dirs[i].path;
                const char *slash = name;
                for (const char *c = name; *c; c++)
                    if (*c == '/') slash = c + 1;
                shell_print(slash, 14);
                shell_print("/", 14);
                shell_newline();
                found = 1;
            }
        }
    }

    for (size_t i = 0; i < g_fs.file_count; i++) {
        if (g_fs.files[i].active) {
            char parent[MAX_PATH_LEN];
            fs_get_parent_path(g_fs.files[i].path, parent);
            if (strcmp(parent, p) == 0) {
                shell_print(g_fs.files[i].name, 11);
                shell_newline();
                found = 1;
            }
        }
    }

    if (!found) {
        shell_print("(empty)", 7);
        shell_newline();
    }
    return 0;
}

static int cmd_cat(int argc, char** argv) {
    if (argc < 2) { shell_print("cat: missing file", 12); return -1; }
    char res[256], buf[4096];
    fs_resolve_path(g_shell.cwd, argv[1], res);
    int sz = fs_read_file(&g_fs, res, buf, 4096);
    if (sz < 0) {
        shell_print("cat: ", 12); shell_print(argv[1], 12); shell_print(": no such file", 12);
        return -1;
    }
    buf[sz] = 0;
    shell_print(buf, 15);
    return 0;
}

static int cmd_pwd(int argc, char** argv) { shell_print(g_shell.cwd, 11); return 0; }

static int cmd_cd(int argc, char** argv) {
    const char* p = argc > 1 ? argv[1] : "/";
    char res[256];
    fs_resolve_path(g_shell.cwd, p, res);
    if (!fs_dir_exists(&g_fs, res)) {
        shell_print("cd: no such directory: ", 12); shell_print(p, 12); return -1;
    }
    strcpy(g_shell.cwd, res);
    return 0;
}

static int cmd_whoami(int argc, char** argv) { shell_print(g_shell.user, 10); return 0; }

static int cmd_logout(int argc, char** argv) {
    (void)argc; (void)argv;
    session_clear();
    shell_print("Oturum kapatildi. Kalici giris hafizasi silindi.\n", 13);
    shell_print("Bir sonraki acilista tekrar giris yapmaniz gerekecek.\n", 13);
    return 0;
}
static int cmd_uname(int argc, char** argv) { shell_print("cofeuOS v3.0 x86_64 UEFI", 14); return 0; }

static int cmd_clear(int argc, char** argv) {
    (void)argc; (void)argv;
    if (g_gui_term_target) {
        memset(g_gui_term_target->term_screen, 0, sizeof(g_gui_term_target->term_screen));
        memset(g_gui_term_target->term_colors, 0, sizeof(g_gui_term_target->term_colors));
        g_gui_term_target->term_cx = 0;
        g_gui_term_target->term_cy = 0;
        return 0;
    }
    video_clear(0); cursor_x = 5; cursor_y = 30; return 0;
}

static int cmd_neofetch(int argc, char** argv) {
    shell_print("    .-\"-.     ", 15);
    shell_print("   / ..  \\    ", 15); shell_print("OS     : cofeuOS v3.0", 11);
    shell_print("  | (  )  |   ", 15); shell_print("Kernel : x86_64 cofeu", 10);
    shell_print("   \\ ..  /    ", 15); shell_print("Shell  : Cofeu Shell", 11);
    shell_print("    `---'      ", 15); shell_print("Host   : cofeu", 11);
    shell_print("Python : MicroPython", 11);
    shell_print("Disk   : /dev/sda1", 11);
    shell_print("Memory : 16MB", 11);
    shell_print("Net    : cofeu-net (TCP/IP)", 14);
    return 0;
}

static int cmd_text_editor(int argc, char** argv, const char* editor_name) {
    extern memory_arena g_mem_arena;
    
    shell_print("========================================", 14);
    shell_print(editor_name, 14);
    shell_print(" editor - :w = save, :q = quit, :wq = save+quit", 14);

    char path[MAX_PATH_LEN] = "";
    if (argc > 1) fs_resolve_path(g_shell.cwd, argv[1], path);
    shell_print("File: ", 7);
    shell_print(path[0] ? path : "<no file>", 15);
    shell_print("========================================", 14);

    /* Büyük buffer için heap bellek ayır */
    int buf_size = 32 * 1024; /* 32KB */
    char *buffer = (char*)kmalloc(&g_mem_arena, buf_size);
    if (!buffer) {
        shell_print("editor: bellek yetersiz", 12);
        shell_newline();
        return -1;
    }
    
    int len = 0;
    if (path[0]) {
        int sz = fs_read_file(&g_fs, path, buffer, buf_size - 1);
        if (sz >= 0) { buffer[sz] = '\0'; len = sz; }
        else { buffer[0] = '\0'; }
    } else { buffer[0] = '\0'; }

    char line[256];
    while (1) {
        cursor_x = 5;
        video_print("> ", 7, cursor_y, 7);
        cursor_x = 25;
        main_get_input(line, sizeof(line));

        if (strcmp(line, ":q") == 0) break;
        if (strcmp(line, ":wq") == 0 || strcmp(line, ":w") == 0) {
            if (path[0]) {
                fs_write_file(&g_fs, path, buffer, len);
                shell_print("Saved.", 10);
            }
            if (strcmp(line, ":wq") == 0) break;
            continue;
        }

        int ll = strlen(line);
        if (len + ll + 2 < buf_size) {
            memcpy(buffer + len, line, ll);
            len += ll;
            buffer[len++] = '\n';
            buffer[len] = '\0';
        } else {
            shell_print("editor: buffer dolu!", 12);
            shell_newline();
        }
    }
    
    kfree(&g_mem_arena, buffer);
    return 0;
}

static int cmd_vim(int argc, char** argv)  { return cmd_text_editor(argc, argv, "vim"); }
static int cmd_nano(int argc, char** argv) { return cmd_text_editor(argc, argv, "nano"); }

static int cmd_reboot(int argc, char** argv) {
    dbg_write("[KRN] cmd_reboot\n");
    shell_print("Rebooting...", 14);
    uefi_reset_system();
    for (;;) { __asm__ volatile("hlt"); }
    return 0;
}

static int cmd_halt(int argc, char** argv) {
    dbg_write("[KRN] cmd_halt\n");
    shell_print("cofeuOS halted.", 12);
    uefi_shutdown();
    for (;;) { __asm__ volatile("hlt"); }
    return 0;
}

static int cmd_ifconfig(int argc, char** argv) {
    network_dhcp();
    
    if (!network_available()) {
        shell_print("eth0: NIC bulunamadi", 12);
        shell_newline();
        return -1;
    }
    
    unsigned char mac[6];
    network_get_mac(mac);
    
    shell_print("eth0: aktif\n", 10);
    shell_print("MAC: ", 15);
    char hex[3];
    for (int i = 0; i < 6; i++) {
        hex[0] = "0123456789ABCDEF"[mac[i] >> 4];
        hex[1] = "0123456789ABCDEF"[mac[i] & 0xF];
        hex[2] = '\0';
        shell_print(hex, 15);
        if (i < 5) shell_print(":", 15);
    }
    shell_newline();
    
    unsigned char ip[4];
    network_get_ip(ip);
    if (ip[0] != 0) {
        shell_print("IP: ", 15);
        char num[4];
        for (int i = 0; i < 4; i++) {
            int n = ip[i], pos = 0;
            if (n == 0) { num[pos++] = '0'; }
            else { while (n > 0) { num[pos++] = '0' + (n % 10); n /= 10; } }
            /* ters çevir */
            for (int a = 0, b = pos-1; a < b; a++, b--) {
                char t = num[a]; num[a] = num[b]; num[b] = t;
            }
            num[pos] = '\0';
            shell_print(num, 15);
            if (i < 3) shell_print(".", 15);
        }
        shell_newline();
    } else {
        shell_print("IP: atanmamis (dhcp kullan)", 12);
        shell_newline();
    }
    return 0;
}

static int cmd_ping(int argc, char** argv) {
    if (argc < 2) { shell_print("kullanim: ping <ip>", 12); shell_newline(); return -1; }
    if (!network_available()) { shell_print("ping: network yok", 12); shell_newline(); return -1; }

    /* IP'yi parse et */
    unsigned char ip[4] = {0,0,0,0};
    const char *p = argv[1];
    for (int i = 0; i < 4; i++) {
        int n = 0;
        while (*p >= '0' && *p <= '9') { n = n*10 + (*p - '0'); p++; }
        ip[i] = (unsigned char)n;
        if (*p == '.') p++;
    }

    shell_print("PING ", 15); shell_print(argv[1], 15); shell_newline();

    for (int i = 0; i < 4; i++) {
        int r = network_ping(ip);
        if (r == 0) {
            shell_print("64 bytes from ", 10);
            shell_print(argv[1], 10);
            shell_print(": icmp_seq=", 10);
            char seq[2] = {'1'+(char)i, '\0'};
            shell_print(seq, 10);
            shell_newline();
        } else {
            shell_print("Request timeout", 12);
            shell_newline();
        }
    }
    return 0;
}

static int cmd_touch(int argc, char** argv) {
    if (argc < 2) { shell_print("touch: filename required", 12); return -1; }
    char res[256];
    fs_resolve_path(g_shell.cwd, argv[1], res);
    fs_create_file(&g_fs, res, "", 0);
    shell_print("Created: ", 10); shell_print(argv[1], 10); shell_newline();
    return 0;
}

static int cmd_mkdir(int argc, char** argv) {
    if (argc < 2) { shell_print("mkdir: dirname required", 12); return -1; }
    char res[256];
    fs_resolve_path(g_shell.cwd, argv[1], res);
    fs_create_dir(&g_fs, res);
    shell_print("Created directory: ", 10); shell_print(argv[1], 10); shell_newline();
    return 0;
}
static int parse_http_url(const char* input, char* host, int host_len, char* path, int path_len, int *use_https, int *port) {
    const char* p = input;
    if (p == NULL || host == NULL || path == NULL) return -1;

    if (use_https) *use_https = 0;
    if (port) *port = 80;
    if (strncmp(p, "http://", 7) == 0) {
        p += 7;
    } else if (strncmp(p, "https://", 8) == 0) {
        p += 8;
        if (use_https) *use_https = 1;
        if (port) *port = 443;
    } else {
        return -1;
    }

    const char* slash = strchr(p, '/');
    const char* query = strchr(p, '?');
    const char* end = slash ? slash : (query ? query : p + strlen(p));

    /* :port ayikla (host kisminin sonuna kadar) */
    const char *colon = NULL;
    for (const char *q = p; q < end; q++) {
        if (*q == ':') { colon = q; break; }
    }
    if (colon) {
        int pv = 0;
        const char *q = colon + 1;
        while (q < end && *q >= '0' && *q <= '9') {
            pv = pv * 10 + (*q - '0');
            if (pv > 65535) return -1;
            q++;
        }
        if (q == colon + 1) return -1;          /* bos port: hata */
        if (q < end) return -1;                 /* port icinde harf: hata */
        if (port) *port = pv;
        end = colon;
    }

    size_t host_len_value = (size_t)(end - p);
    if (host_len_value == 0 || host_len_value >= (size_t)host_len) {
        host_len_value = (size_t)(host_len - 1);
    }
    memcpy(host, p, host_len_value);
    host[host_len_value] = '\0';

    const char* path_start = slash ? slash : "/";
    const char* path_end = query ? query : path_start + strlen(path_start);
    size_t path_len_value = (size_t)(path_end - path_start);
    if (path_len_value == 0) {
        path_len_value = 1;
    }
    if (path_len_value >= (size_t)path_len) {
        path_len_value = (size_t)(path_len - 1);
    }
    memcpy(path, path_start, path_len_value);
    path[path_len_value] = '\0';

    if (path[0] != '/') {
        size_t len = strlen(path);
        if (len + 2 <= (size_t)path_len) {
            memmove(path + 1, path, len + 1);
            path[0] = '/';
        }
    }

    if (path[0] == '\0') {
        strcpy(path, "/index.html");
    }
    return 0;
}

static void join_remote_path(const char* base, const char* child, char* out, int out_len) {
    if (base == NULL || child == NULL || out == NULL || out_len <= 0) return;
    out[0] = '\0';
    if (base[0] == '\0') {
        if (child[0] != '/') {
            out[0] = '/'; out[1] = '\0';
        }
        strncat(out, child, (size_t)(out_len - 1));
        return;
    }

    strncat(out, base, (size_t)(out_len - 1));
    if (out[0] != '\0' && out[strlen(out) - 1] != '/' && child[0] != '/') {
        if (strlen(out) + 1 < (size_t)out_len) {
            strcat(out, "/");
        }
    }
    if (child[0] != '\0' && child[0] != '/') {
        strncat(out, child, (size_t)(out_len - 1));
    } else {
        strncat(out, child, (size_t)(out_len - 1));
    }
}

static void join_local_path(const char* base, const char* child, char* out, int out_len) {
    if (base == NULL || child == NULL || out == NULL || out_len <= 0) return;
    out[0] = '\0';
    if (base[0] == '\0') {
        strncat(out, child, (size_t)(out_len - 1));
        return;
    }

    strncat(out, base, (size_t)(out_len - 1));
    if (out[0] != '\0' && out[strlen(out) - 1] != '/' && child[0] != '/') {
        if (strlen(out) + 1 < (size_t)out_len) {
            strcat(out, "/");
        }
    }
    strncat(out, child, (size_t)(out_len - 1));
}

static int shell_fetch_url(const char *host, const char *path, int use_https, unsigned short port, char *buf, int maxlen) {
    if (use_https) return https_get_port(host, path, buf, maxlen, port);
    return http_get_port(host, path, buf, maxlen, port);
}

static int download_directory_contents(const char* host, const char* remote_dir, const char* local_dir, char* buf, int buflen, int use_https, unsigned short port) {
    char remote_path[256];
    char child_remote[256];
    char child_local[256];
    char temp_target[256];
    char temp_buf[4096];
    char *href;

    if (remote_dir == NULL || local_dir == NULL || buf == NULL || buflen <= 0) return -1;

    strcpy(temp_target, "/.wget_tmp");
    if (fs_create_dir(&g_fs, local_dir) < 0 && !fs_dir_exists(&g_fs, local_dir)) {
        return -1;
    }

    strcpy(remote_path, remote_dir);
    if (remote_path[0] == '\0') {
        strcpy(remote_path, "/");
    }
    if (remote_path[strlen(remote_path) - 1] != '/') {
        strcat(remote_path, "/");
    }

    int len = shell_fetch_url(host, remote_path, use_https, port, temp_buf, sizeof(temp_buf) - 1);
    if (len <= 0) return -1;
    temp_buf[len] = '\0';

    href = strstr(temp_buf, "href=");
    while (href != NULL) {
        char link[256];
        char *start = strchr(href + 6, '"');
        char *end;
        if (start == NULL) break;
        start++;
        end = strchr(start, '"');
        if (end == NULL) break;

        int link_len = (int)(end - start);
        if (link_len <= 0 || link_len >= (int)sizeof(link)) link_len = (int)sizeof(link) - 1;
        memcpy(link, start, link_len);
        link[link_len] = '\0';

        if (strcmp(link, ".") == 0 || strcmp(link, "..") == 0 || link[0] == '#' || strcmp(link, "/") == 0) {
            href = strstr(end + 1, "href=");
            continue;
        }

        if (strncmp(link, "http://", 7) == 0 || strncmp(link, "https://", 8) == 0) {
            href = strstr(end + 1, "href=");
            continue;
        }

        join_remote_path(remote_dir, link, child_remote, sizeof(child_remote));
        join_local_path(local_dir, link, child_local, sizeof(child_local));
        if (child_local[strlen(child_local) - 1] == '/') {
            child_local[strlen(child_local) - 1] = '\0';
        }

        if (link[strlen(link) - 1] == '/') {
            if (fs_create_dir(&g_fs, child_local) < 0 && !fs_dir_exists(&g_fs, child_local)) {
                href = strstr(end + 1, "href=");
                continue;
            }
            download_directory_contents(host, child_remote, child_local, buf, buflen, use_https, port);
        } else {
            shell_fetch_url(host, child_remote, use_https, port, buf, buflen);
        }

        href = strstr(end + 1, "href=");
    }

    return 0;
}

static int cmd_wget(int argc, char** argv) {
    if (argc < 2) { shell_print("kullanim: wget <url>", 12); shell_newline(); return -1; }
    if (!network_available()) { shell_print("wget: network yok", 12); shell_newline(); return -1; }
    
    shell_print("wget: indiriliyor...", 15); shell_newline();

    char host[64];
    char http_path[256];
    char target[256];
    char target_dir[256];
    char local_path[256];
    char remote_path[256];
    int directory_mode = 0;
    int use_https = 0;
    unsigned short port = 80;

    if (parse_http_url(argv[1], host, sizeof(host), http_path, sizeof(http_path), &use_https, &port) == 0) {
        strcpy(remote_path, http_path);
        strcpy(local_path, http_path);
    } else {
        strcpy(host, "10.0.2.2");
        if (argv[1][0] == '/') {
            strcpy(remote_path, argv[1]);
            strcpy(local_path, argv[1]);
            directory_mode = 0;
        } else {
            remote_path[0] = '/';
            remote_path[1] = '\0';
            strcat(remote_path, argv[1]);
            strcpy(local_path, argv[1]);
            directory_mode = 1;
        }
    }

    if (remote_path[0] == '\0' || strcmp(remote_path, "/") == 0) {
        strcpy(remote_path, "/index.html");
        strcpy(local_path, "/index.html");
        directory_mode = 0;
    }

    if (directory_mode) {
        if (fs_resolve_path(g_shell.cwd, local_path, target_dir) != 0) {
            shell_print("wget: yol hatasi", 12); shell_newline();
            return -1;
        }
        if (fs_create_dir(&g_fs, target_dir) < 0 && !fs_dir_exists(&g_fs, target_dir)) {
            shell_print("wget: dizin olusturulamadi", 12); shell_newline();
            return -1;
        }
        strcpy(target, target_dir);
        shell_print("wget: klasor modu", 10); shell_newline();
    } else {
        if (fs_resolve_path(g_shell.cwd, local_path, target) != 0) {
            shell_print("wget: yol hatasi", 12); shell_newline();
            return -1;
        }
    }

    shell_print("wget: kayit yolu: ", 10); shell_print(target, 10); shell_newline();

    static char buf[4096];
    int len;
    if (directory_mode) {
        if (download_directory_contents(host, remote_path, target_dir, buf, sizeof(buf) - 1, use_https, port) < 0) {
            shell_print("wget: dizin indirilemedi", 12); shell_newline();
            return -1;
        }
        shell_print("wget: dizin indirildi", 10); shell_newline();
        return 0;
    }

    len = shell_fetch_url(host, remote_path, use_https, port, buf, sizeof(buf) - 1);
    
    if (len < 0) { 
        shell_print("wget: baglanti hatasi (", 12); 
        shell_print_int(len, 10);
        shell_print(")", 12);
        shell_newline(); 
        return -1; 
    }
    
    if (len == 0) {
        shell_print("wget: veri alinmadi", 12); shell_newline();
        return -1;
    }
    
    buf[len] = '\0';
    shell_print("wget: ", 10); shell_print_int(len, 10);
    shell_print(" byte alindi", 10); shell_newline();
    
    /* Dosyanin yazilip yazilmadigini kontrol et */
    if (fs_file_exists(&g_fs, target)) {
        int file_size = fs_read_file(&g_fs, target, buf, sizeof(buf) - 1);
        shell_print("wget: dosya kaydedildi (", 10);
        shell_print_int(file_size, 10);
        shell_print(" byte)", 10);
        shell_newline();
    } else {
        shell_print("wget: dosya kaydedilemedi!", 12); shell_newline();
    }
    
    shell_print(buf, 15);
    shell_newline();
    return 0;
}
static int cmd_rm(int argc, char** argv) {
    if (argc < 2) { shell_print("rm: filename required", 12); return -1; }
    char res[256];
    fs_resolve_path(g_shell.cwd, argv[1], res);
    fs_delete_file(&g_fs, res);
    shell_print("Removed: ", 10); shell_print(argv[1], 10); shell_newline();
    return 0;
}

static int cmd_rmdir(int argc, char** argv) {
    if (argc < 2) { shell_print("rmdir: dirname required", 12); return -1; }
    char res[256];
    fs_resolve_path(g_shell.cwd, argv[1], res);
    fs_delete_dir(&g_fs, res);
    shell_print("Removed directory: ", 10); shell_print(argv[1], 10); shell_newline();
    return 0;
}

/* ─── RODO / SUDO ─────────────────────────────────────────────── */

/* Rodo şifre veritabanı */
#define RODO_MAX_USERS 8
typedef struct {
    char username[32];
    char password[32];
    int  is_root;
} rodo_user_t;

static rodo_user_t rodo_users[RODO_MAX_USERS];
static int rodo_user_count = 0;
static int rodo_initialized = 0;

static void rodo_init(void) {
    if (rodo_initialized) return;
    strcpy(rodo_users[0].username, "root");
    rodo_users[0].password[0] = '\0'; /* Şifre yok henüz */
    rodo_users[0].is_root = 1;
    rodo_user_count = 1;
    rodo_initialized = 1;
}

static int rodo_get_password(char *buf, int maxlen) {
    int pos = 0;
    while (pos < maxlen - 1) {
        char ch = read_key();
        if (ch == '\n') {
            buf[pos] = '\0';
            shell_newline();
            return pos;
        } else if (ch == '\b') {
            if (pos > 0) {
                pos--;
                cursor_x -= font_width;
                video_clear_rect(cursor_x, cursor_y, font_width, font_height);
            }
        } else if (ch >= 32) {
            buf[pos++] = ch;
            video_draw_char('*', cursor_x, cursor_y, 15);
            cursor_x += font_width;
        }
    }
    buf[pos] = '\0';
    return pos;
}

static int cmd_rodo(int argc, char** argv) {
    rodo_init();

    /* rodo passwd — şifre değiştirme */
    if (argc == 2 && strcmp(argv[1], "passwd") == 0) {
        /* Eğer şifre yoksa direkt yeni şifre al */
        if (rodo_users[0].password[0] != '\0') {
            shell_print("[rodo] mevcut root sifresi: ", 15);
            char oldpass[32];
            rodo_get_password(oldpass, sizeof(oldpass));
            if (strcmp(rodo_users[0].password, oldpass) != 0) {
                shell_print("rodo: yanlis sifre", 12); shell_newline();
                return -1;
            }
        }
        shell_print("[rodo] yeni sifre: ", 15);
        char newpass[32];
        rodo_get_password(newpass, sizeof(newpass));
        shell_print("[rodo] tekrar: ", 15);
        char newpass2[32];
        rodo_get_password(newpass2, sizeof(newpass2));
        if (strcmp(newpass, newpass2) != 0) {
            shell_print("rodo: sifreler uyusmuyor", 12); shell_newline();
            return -1;
        }
        strcpy(rodo_users[0].password, newpass);
        shell_print("rodo: sifre belirlendi", 10); shell_newline();
        return 0;
    }

    if (argc < 2) {
        shell_print("kullanim: rodo <komut>", 12); shell_newline();
        shell_print("         rodo passwd  (sifre degistir)", 12); shell_newline();
        return -1;
    }

    /* İlk kullanımda şifre belirle */
    if (rodo_users[0].password[0] == '\0') {
        shell_print("[rodo] ilk kurulum - yeni root sifresi: ", 14);
        char newpass[32];
        rodo_get_password(newpass, sizeof(newpass));
        shell_print("[rodo] tekrar: ", 14);
        char newpass2[32];
        rodo_get_password(newpass2, sizeof(newpass2));
        if (strcmp(newpass, newpass2) != 0) {
            shell_print("rodo: sifreler uyusmuyor", 12); shell_newline();
            return -1;
        }
        strcpy(rodo_users[0].password, newpass);
        shell_print("rodo: sifre belirlendi", 10); shell_newline();
    }

    /* Şifre iste */
    shell_print("[rodo] ", 10);
    shell_print(g_shell.user, 10);
    shell_print(" icin root sifresi: ", 15);

    char password[32];
    rodo_get_password(password, sizeof(password));

    if (strcmp(rodo_users[0].password, password) != 0) {
        shell_print("rodo: yanlis sifre", 12); shell_newline();
        return -1;
    }

    shell_print("rodo: yetkilendirildi", 10); shell_newline();

    char command[512];
    int pos = 0;
    for (int i = 1; i < argc; i++) {
        int len = strlen(argv[i]);
        if (pos + len + 2 >= (int)sizeof(command)) break;
        strcpy(&command[pos], argv[i]);
        pos += len;
        if (i < argc - 1) command[pos++] = ' ';
    }
    command[pos] = '\0';
    return shell_execute(command);
}

/* ─── Diğer komutlar ──────────────────────────────────────────── */

static int cmd_pacman(int argc, char** argv) {
    if (argc < 2) { shell_print("kullanim: pacman -S/-Ss/-Sy/-Syu <paket>", 12); shell_newline(); return -1; }
    if (strcmp(argv[1], "-S") == 0) {
        if (argc < 3) { shell_print("pacman: paket adi gerekli", 12); shell_newline(); return -1; }
        shell_print("yukleniyor: ", 10); shell_print(argv[2], 10); shell_newline(); return 0;
    }
    if (strcmp(argv[1], "-Ss") == 0) {
        if (argc < 3) { shell_print("pacman: arama terimi gerekli", 12); shell_newline(); return -1; }
        shell_print("arama sonucu: ", 11); shell_print(argv[2], 11); shell_newline(); return 0;
    }
    if (strcmp(argv[1], "-Sy") == 0)  { shell_print("paket veritabani guncellendi", 10); shell_newline(); return 0; }
    if (strcmp(argv[1], "-Syu") == 0) { shell_print("sistem guncellendi", 10); shell_newline(); return 0; }
    shell_print("pacman: bilinmeyen islem", 12); shell_newline();
    return -1;
}

static int shell_atoi(const char* s) {
    int sign = 1, value = 0;
    if (*s == '-') { sign = -1; s++; }
    while (*s >= '0' && *s <= '9') { value = value * 10 + (*s - '0'); s++; }
    return value * sign;
}

static void shell_print_int(int value, u8 color) {
    char buf[16]; int pos = 0;
    if (value == 0) { shell_print("0", color); return; }
    if (value < 0) { shell_print("-", color); value = -value; }
    while (value > 0 && pos < (int)sizeof(buf)) { buf[pos++] = '0' + (value % 10); value /= 10; }
    while (pos > 0) { char out[2] = { buf[--pos], '\0' }; shell_print(out, color); }
}

static void shell_join_args(char** argv, int start, int argc, char* out, int out_size) {
    int pos = 0;
    for (int i = start; i < argc; i++) {
        int len = strlen(argv[i]);
        if (pos + len + 2 >= out_size) break;
        strcpy(&out[pos], argv[i]);
        pos += len;
        if (i < argc - 1) out[pos++] = ' ';
    }
    out[pos] = '\0';
}

static int cmd_apps(int argc, char** argv) {
    shell_print("Yuklu uygulamalar:\n", 14);
    shell_print("desktop/startx  grafik masaustu\n", 15);
    shell_print("python/python3  MicroPython REPL\n", 15);
    shell_print("curl            HTTP istekleri (domain destekli)\n", 15);
    shell_print("unzip           ZIP dosyasi cikarma\n", 15);
    shell_print("untar           TAR dosyasi cikarma\n", 15);
    shell_print("calc write nano vim sysinfo neofetch pacman\n", 15);
    return 0;
}

static int cmd_about(int argc, char** argv) {
    shell_print("cofeuOS v3.0\n", 14);
    shell_print("UEFI GOP destekli, MicroPython entegreli mini isletim sistemi.", 15);
    shell_newline();
    return 0;
}

static int cmd_sysinfo(int argc, char** argv) {
    shell_print("Sistem Bilgisi:\n", 14);
    shell_print("OS     : cofeuOS v3.0\n", 11);
    shell_print("Kernel : x86_64 cofeu\n", 10);
    shell_print("Video  : ", 11);
    shell_print_int(SCREEN_WIDTH, 11); shell_print("x", 11);
    shell_print_int(SCREEN_HEIGHT, 11); shell_print("x32bpp (GOP)\n", 11);
    shell_print("Bellek : 16MB\n", 11);
    shell_print("FS     : cofeuFS (RAM)\n", 11);
    shell_print("Python : MicroPython embed\n", 11);
    shell_print("Net    : cofeu-net / TCP+DNS+HTTP\n", 14);
    return 0;
}

static int cmd_calc(int argc, char** argv) {
    if (argc < 4) { shell_print("kullanim: calc <a> +|-|*|/ <b>", 12); shell_newline(); return -1; }
    int a = shell_atoi(argv[1]), b = shell_atoi(argv[3]), result = 0;
    if      (strcmp(argv[2], "+") == 0) result = a + b;
    else if (strcmp(argv[2], "-") == 0) result = a - b;
    else if (strcmp(argv[2], "*") == 0) result = a * b;
    else if (strcmp(argv[2], "/") == 0) {
        if (b == 0) { shell_print("calc: sifira bolme", 12); shell_newline(); return -1; }
        result = a / b;
    } else { shell_print("calc: bilinmeyen operator", 12); shell_newline(); return -1; }
    shell_print_int(result, 10); shell_newline();
    return 0;
}

static int cmd_write(int argc, char** argv) {
    if (argc < 3) { shell_print("kullanim: write <dosya> <metin...>", 12); shell_newline(); return -1; }
    char path[MAX_PATH_LEN], text[512];
    fs_resolve_path(g_shell.cwd, argv[1], path);
    shell_join_args(argv, 2, argc, text, sizeof(text));
    fs_write_file(&g_fs, path, text, strlen(text));
    shell_print("yazildi: ", 10); shell_print(argv[1], 10); shell_newline();
    return 0;
}

static int cmd_theme(int argc, char** argv) {
    video_clear(0);
    video_fill_rect(0, 0, SCREEN_WIDTH, 16, 9);
    video_fill_rect(0, 16, SCREEN_WIDTH, 16, 3);
    video_fill_rect(0, 32, SCREEN_WIDTH, 16, 5);
    cursor_x = 5; cursor_y = 56;
    shell_print("Tema onizlemesi. Terminale donmek icin clear yaz.", 14);
    shell_newline();
    return 0;
}

static int cmd_nslookup(int argc, char** argv) {
    if (argc < 2) {
        shell_print("kullanim: nslookup <domain>", 12); shell_newline();
        return -1;
    }
    if (!network_available()) {
        shell_print("nslookup: ag yok", 12); shell_newline();
        return -1;
    }
    shell_print("Sorgulanıyor: ", 10); shell_print(argv[1], 15); shell_newline();
    shell_print("DNS sunucu  : 10.0.2.3\n", 10);
    unsigned char ip[4];
    int r = dns_resolve(argv[1], ip);
    if (r != 0) {
        shell_print("nslookup: cozulemedi (DNS cevap yok)", 12); shell_newline();
        return -1;
    }
    shell_print("Sonuc       : ", 10);
    for (int i = 0; i < 4; i++) {
        char tmp[4]; int t = 0; unsigned char v = ip[i];
        if (v == 0) { tmp[t++] = '0'; }
        else { while (v > 0) { tmp[t++] = '0' + v % 10; v /= 10; } }
        for (int j = t - 1; j >= 0; j--) { char c[2] = { tmp[j], '\0' }; shell_print(c, 10); }
        if (i < 3) shell_print(".", 10);
    }
    shell_newline();
    return 0;
}

static int cmd_nettest(int argc, char** argv) {
    (void)argc; (void)argv;
    shell_print("[nettest] Ag teshisi basliyor...\n", 14);

    /* 1. Surucu kontrolu */
    shell_print("[1] Ag surucusu: ", 11);
    if (!network_available()) {
        shell_print("YOK! QEMU -netdev secenegi eksik?", 12); shell_newline();
        return -1;
    }
    shell_print("OK\n", 10);

    /* MAC + IP */
    unsigned char mac[6], ip[4];
    network_get_mac(mac);
    network_get_ip(ip);
    shell_print("    MAC: ", 11);
    for (int i = 0; i < 6; i++) {
        unsigned char b = mac[i];
        const char *hex = "0123456789ABCDEF";
        char h[3] = { hex[b>>4], hex[b&0xF], '\0' };
        shell_print(h, 15);
        if (i < 5) shell_print(":", 15);
    }
    shell_newline();
    shell_print("    IP : 10.0.2.15\n", 11);

    /* 2. ARP - gateway */
    shell_print("[2] ARP (gateway 10.0.2.2): ", 11);
    unsigned char gw[4] = {10,0,2,2};
    unsigned char gw_mac[6] = {0,0,0,0,0,0};
    int arp_r = arp_test(gw, gw_mac);
    if (arp_r != 0) {
        shell_print("BASARISIZ (gateway cevap vermiyor)", 12); shell_newline();
        shell_print("    -> QEMU user-mode ag aktif degil olabilir", 12); shell_newline();
        return -1;
    }
    shell_print("OK  MAC: ", 10);
    for (int i = 0; i < 6; i++) {
        const char *hex = "0123456789ABCDEF";
        unsigned char b = gw_mac[i];
        char h[3] = { hex[b>>4], hex[b&0xF], '\0' };
        shell_print(h, 15);
        if (i < 5) shell_print(":", 15);
    }
    shell_newline();

    /* 3. DNS */
    shell_print("[3] DNS (example.com -> 10.0.2.3): ", 11);
    unsigned char resolved[4];
    int dns_r = dns_resolve("example.com", resolved);
    if (dns_r != 0) {
        shell_print("BASARISIZ", 12); shell_newline();
        shell_print("    -> DNS paketi gidip gelmiyor", 12); shell_newline();
        shell_print("    -> Cozum: curl http://93.184.216.34/ dene", 14); shell_newline();
        return -1;
    }
    shell_print("OK  IP: ", 10);
    for (int i = 0; i < 4; i++) {
        char tmp[4]; int t = 0; unsigned char v = resolved[i];
        if (v == 0) { tmp[t++] = '0'; }
        else { while (v > 0) { tmp[t++] = '0' + v % 10; v /= 10; } }
        for (int j = t - 1; j >= 0; j--) { char c[2] = { tmp[j], '\0' }; shell_print(c, 10); }
        if (i < 3) shell_print(".", 10);
    }
    shell_newline();

    /* 4. TCP */
    shell_print("[4] TCP (example.com:80 baglantiyor): ", 11);
    int tcp_r = tcp_connect(resolved, 80);
    if (tcp_r != 0) {
        shell_print("BASARISIZ (SYN-ACK yok)", 12); shell_newline();
        shell_print("    -> Hata kodu: ", 12);
        shell_print_int(tcp_r, 12); shell_newline();
        return -1;
    }
    shell_print("OK\n", 10);
    tcp_disconnect(resolved, 80);

    shell_print("[nettest] Tum testler BASARILI!\n", 10);
    return 0;
}

/* ─── CURL Komutu (Domain + IP Destegi) ─────────────────── */
static void curl_default_save_name(const char *host, char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    (void)host;
    /* curl'ün varsayılan çıktısı her zaman mevcut dizindeki index.html'dir. */
    strncpy(out, "index.html", out_size - 1);
    out[out_size - 1] = '\0';
}

static int cmd_curl(int argc, char** argv) {
    if (argc < 2) {
        shell_print("kullanim: curl [-v] [-o dosya] <url>", 12);
        shell_newline();
        shell_print("  -v        : verbose mod", 15);
        shell_newline();
        shell_print("  -o dosya  : sonucu verilen dosyaya kaydet", 15);
        shell_newline();
        shell_print("  varsayilan: mevcut dizinde index.html", 15);
        shell_newline();
        shell_print("  -X POST   : POST istegi gonder", 15);
        shell_newline();
        shell_print("  -d data   : POST verisi", 15);
        shell_newline();
        return -1;
    }
    
    if (!network_available()) {
        shell_print("curl: ag baglantisi yok", 12);
        shell_newline();
        return -1;
    }
    
    int verbose = 0;
    char save_path[MAX_PATH_LEN] = "";
    char post_data[1024] = "";
    int is_post = 0;
    const char *url = NULL;
    
    /* Argümanları parse et */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            fs_resolve_path(g_shell.cwd, argv[++i], save_path);
        } else if (strcmp(argv[i], "-X") == 0 && i + 1 < argc) {
            if (strcmp(argv[++i], "POST") == 0) is_post = 1;
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            strcpy(post_data, argv[++i]);
        } else {
            url = argv[i];
        }
    }
    
    if (!url) {
        shell_print("curl: URL gerekli", 12);
        shell_newline();
        return -1;
    }
    
    /* URL parse et: http://domain/path veya http://ip:port/path */
    char host[64] = "";
    char path[256] = "/";
    int use_https = 0;
    unsigned short port = 80;

    if (parse_http_url(url, host, sizeof(host), path, sizeof(path), &use_https, &port) != 0) {
        shell_print("curl: URL hatali", 12);
        shell_newline();
        return -1;
    }
    
    if (verbose) {
        shell_print("curl: ", 11);
        shell_print(host, 11);
        shell_print(" -> ", 11);
        shell_print(path, 11);
        shell_newline();
    }
    
    static char buf[16384];
    int len;
    
    if (use_https) {
        if (verbose) shell_print("curl: TLS baglantisi isteniyor...", 11);
        len = https_get_port(host, path, buf, sizeof(buf) - 1, port);
    } else if (is_post && post_data[0]) {
        if (verbose) shell_print("curl: POST istegi gonderiliyor...", 11);
        len = http_post(host, path, post_data, strlen(post_data), buf, sizeof(buf) - 1);
    } else {
        if (verbose) shell_print("curl: GET istegi gonderiliyor...", 11);
        len = http_get_port(host, path, buf, sizeof(buf) - 1, port);
    }
    
    if (len < 0) {
        if (len == NETWORK_ERR_TLS_VERIFICATION_FAILED) {
            shell_print("curl: HTTPS sertifika doğrulaması başarısız", 12);
            shell_newline();
            return -1;
        }
        if (len == NETWORK_ERR_TLS_UNAVAILABLE) {
            shell_print("curl: HTTPS/TLS handshakesi başarısız", 12);
            shell_newline();
            return -1;
        }
        shell_print("curl: baglanti hatasi (", 12);
        shell_print_int(len, 10);
        shell_print(")", 12);
        shell_newline();
        return -1;
    }
    
    if (len == 0) {
        shell_print("curl: veri alinmadi", 12);
        shell_newline();
        return -1;
    }
    
    buf[len] = '\0';
    
    if (!save_path[0]) {
        char filename[MAX_FILENAME];
        curl_default_save_name(host, filename, sizeof(filename));
        if (fs_resolve_path(g_shell.cwd, filename, save_path) != 0) {
            shell_print("curl: kayit yolu olusturulamadi", 12);
            shell_newline();
            return -1;
        }
    }

    if (save_path[0]) {
        int written = fs_write_file(&g_fs, save_path, buf, (size_t)len);
        if (written != len) {
            shell_print("curl: dosya sistemine kaydedilemedi", 12);
            shell_newline();
            return -1;
        }
        shell_print("curl: ", 10);
        shell_print_int(len, 10);
        shell_print(" byte HTML olarak kaydedildi: ", 10);
        shell_print(save_path, 10);
        shell_newline();
    }
    
    return 0;
}

/* ─── ZIP Desteği ──────────────────────────────────────────────── */
/* Basit ZIP okuyucu (sıkıştırılmamış dosyalar için) */
typedef struct {
    unsigned int signature;
    unsigned short version;
    unsigned short flags;
    unsigned short compression;
    unsigned short mod_time;
    unsigned short mod_date;
    unsigned int crc32;
    unsigned int compressed_size;
    unsigned int uncompressed_size;
    unsigned short name_length;
    unsigned short extra_length;
} __attribute__((packed)) zip_local_file_header_t;

typedef struct {
    unsigned int signature;
    unsigned short disk_num;
    unsigned short central_disk;
    unsigned short entries_disk;
    unsigned short entries_total;
    unsigned int central_size;
    unsigned int central_offset;
    unsigned short comment_length;
} __attribute__((packed)) zip_end_of_central_t;

static int zip_extract_file(const char *zip_data, int zip_size, const char *target_dir) {
    int offset = 0;
    int file_count = 0;
    
    while (offset + 30 <= zip_size) {
        zip_local_file_header_t *hdr = (zip_local_file_header_t*)(zip_data + offset);
        
        /* Local file header imzası */
        if (hdr->signature != 0x04034b50) break;
        
        int name_len = hdr->name_length;
        int extra_len = hdr->extra_length;
        int comp_size = hdr->compressed_size;
        int uncomp_size = hdr->uncompressed_size;
        
        /* Dosya adı */
        const char *name = zip_data + offset + 30;
        offset += 30 + name_len + extra_len;
        
        if (offset + comp_size > zip_size) break;
        
        /* Dizin ise atla */
        if (name[name_len - 1] == '/') {
            offset += comp_size;
            continue;
        }
        
        /* Dosyayı kaydet */
        char full_path[MAX_PATH_LEN];
        if (target_dir[0]) {
            strcpy(full_path, target_dir);
            if (full_path[strlen(full_path) - 1] != '/') strcat(full_path, "/");
        } else {
            full_path[0] = '\0';
        }
        
        /* path.basename */
        const char *fname = name;
        for (int i = 0; i < name_len; i++) {
            if (name[i] == '/' || name[i] == '\\') fname = name + i + 1;
        }
        
        strncat(full_path, fname, MAX_PATH_LEN - strlen(full_path) - 1);
        
        /* Sıkıştırma kontrolü */
        if (hdr->compression == 0) {
            /* Sıkıştırılmamış */
            fs_create_file(&g_fs, full_path, zip_data + offset, uncomp_size);
            file_count++;
        }
        /* DEFLATE desteği şu an yok, sadece stored dosyalar */
        
        offset += comp_size;
    }
    
    return file_count;
}

static int cmd_unzip(int argc, char** argv) {
    if (argc < 2) {
        shell_print("kullanim: unzip <dosya.zip> [hedef_dizin]", 12);
        shell_newline();
        return -1;
    }
    
    /* ZIP dosyasını oku */
    char zip_path[MAX_PATH_LEN];
    fs_resolve_path(g_shell.cwd, argv[1], zip_path);
    
    static char zip_buf[65536]; /* 64KB max */
    int zip_size = fs_read_file(&g_fs, zip_path, zip_buf, sizeof(zip_buf) - 1);
    
    if (zip_size < 0) {
        shell_print("unzip: dosya bulunamadi: ", 12);
        shell_print(argv[1], 12);
        shell_newline();
        return -1;
    }
    
    zip_buf[zip_size] = '\0';
    
    /* ZIP imzası kontrolü */
    if (zip_size < 4 || *(unsigned int*)zip_buf != 0x04034b50) {
        shell_print("unzip: gecerli bir ZIP dosyasi degil", 12);
        shell_newline();
        return -1;
    }
    
    /* Hedef dizin */
    char target_dir[MAX_PATH_LEN] = "";
    if (argc > 2) {
        fs_resolve_path(g_shell.cwd, argv[2], target_dir);
        fs_create_dir(&g_fs, target_dir);
    }
    
    int count = zip_extract_file(zip_buf, zip_size, target_dir);
    
    shell_print("unzip: ", 10);
    shell_print_int(count, 10);
    shell_print(" dosya cikarildi", 10);
    shell_newline();
    
    return 0;
}

/* ─── TAR/GZ Desteği ──────────────────────────────────────────── */
/* USTAR başlık formatı */
typedef struct {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag[1];
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
} __attribute__((packed)) tar_ustar_t;

/* Octal string'den int'e çevir */
static int tar_octal(const char *s, int len) {
    int result = 0;
    for (int i = 0; i < len && s[i] >= '0' && s[i] <= '7'; i++) {
        result = result * 8 + (s[i] - '0');
    }
    return result;
}

static int tar_extract_file(const char *tar_data, int tar_size, const char *target_dir) {
    int offset = 0;
    int file_count = 0;
    
    while (offset + 512 <= tar_size) {
        tar_ustar_t *hdr = (tar_ustar_t*)(tar_data + offset);
        
        /* Boş blok kontrolü (tar sonu) */
        int is_empty = 1;
        for (int i = 0; i < 512; i++) {
            if ((unsigned char)tar_data[offset + i] != 0) { is_empty = 0; break; }
        }
        if (is_empty) break;
        
        /* USTAR magic kontrolü */
        if (memcmp(hdr->magic, "ustar", 5) != 0) break;
        
        int file_size = tar_octal(hdr->size, 11);
        char type = hdr->typeflag[0];
        
        /* Dosya adı */
        char name[256] = "";
        if (hdr->prefix[0]) {
            strcpy(name, hdr->prefix);
            strcat(name, "/");
        }
        strcat(name, hdr->name);
        
        int name_len = strlen(name);
        if (name_len > 0 && name[name_len - 1] == '/') {
            name[name_len - 1] = '\0';
        }
        
        /* Dizin ise oluştur */
        if (type == '5') {
            char dir_path[MAX_PATH_LEN];
            if (target_dir[0]) {
                strcpy(dir_path, target_dir);
                if (dir_path[strlen(dir_path) - 1] != '/') strcat(dir_path, "/");
            } else {
                dir_path[0] = '\0';
            }
            strcat(dir_path, name);
            fs_create_dir(&g_fs, dir_path);
            offset += 512;
            /* Dizin verisi atla */
            int data_blocks = (file_size + 511) / 512;
            offset += data_blocks * 512;
            continue;
        }
        
        /* Dosya ise kaydet */
        if (type == '0' || type == '\0') {
            char full_path[MAX_PATH_LEN];
            if (target_dir[0]) {
                strcpy(full_path, target_dir);
                if (full_path[strlen(full_path) - 1] != '/') strcat(full_path, "/");
            } else {
                full_path[0] = '\0';
            }
            strcat(full_path, name);
            
            /* Üst dizinleri oluştur */
            char parent[MAX_PATH_LEN];
            fs_get_parent_path(full_path, parent);
            if (parent[0] && strcmp(parent, "/") != 0) {
                fs_create_dir(&g_fs, parent);
            }
            
            if (file_size > 0 && offset + 512 + file_size <= tar_size) {
                fs_create_file(&g_fs, full_path, tar_data + offset + 512, file_size);
                file_count++;
            }
        }
        
        offset += 512;
        int data_blocks = (file_size + 511) / 512;
        offset += data_blocks * 512;
    }
    
    return file_count;
}

static int cmd_untar(int argc, char** argv) {
    if (argc < 2) {
        shell_print("kullanim: untar <dosya.tar> [hedef_dizin]", 12);
        shell_newline();
        shell_print("  .tar, .tar.gz dosyalarini cikarir", 15);
        shell_newline();
        return -1;
    }
    
    /* TAR dosyasını oku */
    char tar_path[MAX_PATH_LEN];
    fs_resolve_path(g_shell.cwd, argv[1], tar_path);
    
    static char tar_buf[65536]; /* 64KB max */
    int tar_size = fs_read_file(&g_fs, tar_path, tar_buf, sizeof(tar_buf) - 1);
    
    if (tar_size < 0) {
        shell_print("untar: dosya bulunamadi: ", 12);
        shell_print(argv[1], 12);
        shell_newline();
        return -1;
    }
    
    tar_buf[tar_size] = '\0';
    
    /* GZIP kontrolü (0x1f 0x8b) */
    if (tar_size >= 2 && (unsigned char)tar_buf[0] == 0x1f && (unsigned char)tar_buf[1] == 0x8b) {
        shell_print("untar: gzip sikistirilmis dosya tespit edildi", 14);
        shell_newline();
        shell_print("untar: su anlik sadece sikistirilmamis .tar dosyalari destekleniyor", 12);
        shell_newline();
        return -1;
    }
    
    /* TAR header kontrolü */
    if (tar_size < 512) {
        shell_print("untar: dosya cok kucuk veya gecersiz", 12);
        shell_newline();
        return -1;
    }
    
    /* USTAR magic kontrolü */
    tar_ustar_t *hdr = (tar_ustar_t*)tar_buf;
    if (memcmp(hdr->magic, "ustar", 5) != 0) {
        shell_print("untar: gecerli bir TAR dosyasi degil", 12);
        shell_newline();
        return -1;
    }
    
    /* Hedef dizin */
    char target_dir[MAX_PATH_LEN] = "";
    if (argc > 2) {
        fs_resolve_path(g_shell.cwd, argv[2], target_dir);
        fs_create_dir(&g_fs, target_dir);
    }
    
    int count = tar_extract_file(tar_buf, tar_size, target_dir);
    
    shell_print("untar: ", 10);
    shell_print_int(count, 10);
    shell_print(" dosya cikarildi", 10);
    shell_newline();
    
    return 0;
}

/* ─── COFEUDE DESKTOP GUI V2 (Mouse Cursor, Window Manager, Window Terminal) ─── */

/* ─── JS Motoru Bağlantısı (CofeuTarayici) ──────────────────
   console.log → debugcon (port 0xE9), alert/prompt/confirm → basit
   ekran kutusu + klavye. Klavye masaüstüyle aynı kaynaktan okunur. */
static int ps2_kq_pop(key_event_t *ev);

static void js_wait_key(key_event_t *ev) {
    for (;;) {
        ev->key = 0;
        ev->scan_code = 0;
        ps2_kq_pop(ev);
        if (ev->key || ev->scan_code) return;
        *ev = try_read_key_event();
        if (ev->key || ev->scan_code) return;
        pit_delay_ms(1);
    }
}

static void js_log_cb(const char *msg) {
    dbg_write("[JS] ");
    dbg_write(msg);
    dbg_write("\r\n");
}

static void js_dialog_box(const char *title, const char *msg) {
    int w = SCREEN_WIDTH / 2;
    if (w < 220) w = 220;
    if (w > SCREEN_WIDTH - 20) w = SCREEN_WIDTH - 20;
    int h = 5 * CHAR_HEIGHT;
    int x = (SCREEN_WIDTH - w) / 2;
    int y = (SCREEN_HEIGHT - h) / 2;
    video_fill_rect(x, y, w, h, 1);
    video_draw_rect(x, y, w, h, 15);
    video_print(title, x + 8, y + 3, 11);
    int wrap = (w - 16) / CHAR_WIDTH;
    if (wrap < 1) wrap = 1;
    const char *p = msg;
    for (int line = 0; line < 3 && *p; line++) {
        char buf[64];
        int n = 0;
        while (*p && n < wrap - 1 && n < (int)sizeof(buf) - 1) buf[n++] = *p++;
        buf[n] = '\0';
        video_print(buf, x + 8, y + 3 + (line + 2) * CHAR_HEIGHT, 15);
    }
}

static void js_alert_cb(const char *msg) {
    js_dialog_box("JavaScript - alert", msg);
    key_event_t ev;
    js_wait_key(&ev);
}

static int js_confirm_cb(const char *msg) {
    js_dialog_box("JavaScript - confirm", msg);
    int w = SCREEN_WIDTH / 2;
    if (w < 220) w = 220;
    int x = (SCREEN_WIDTH - w) / 2;
    int y = (SCREEN_HEIGHT - 5 * CHAR_HEIGHT) / 2;
    int fy = y + 4 * CHAR_HEIGHT;
    for (;;) {
        video_fill_rect(x + 8, fy, w - 16, CHAR_HEIGHT, 0);
        video_print("[Y] Evet    [N] Hayir", x + 8, fy, 15);
        key_event_t ev;
        js_wait_key(&ev);
        char ch = ev.key;
        if (ch == 'y' || ch == 'Y' || ch == '\n' || ch == '\r') return 1;
        if (ch == 'n' || ch == 'N') return 0;
    }
}

static int js_prompt_cb(const char *msg, char *out, int out_size) {
    js_dialog_box("JavaScript - prompt", msg);
    int w = SCREEN_WIDTH / 2;
    if (w < 220) w = 220;
    int x = (SCREEN_WIDTH - w) / 2;
    int y = (SCREEN_HEIGHT - 5 * CHAR_HEIGHT) / 2;
    int iy = y + 4 * CHAR_HEIGHT;
    int pos = 0;
    out[0] = '\0';
    for (;;) {
        video_fill_rect(x + 8, iy, w - 16, CHAR_HEIGHT, 0);
        char buf[64];
        memcpy(buf, out, (size_t)pos);
        buf[pos] = '\0';
        int tx = x + 8;
        if (pos > 0) video_print(buf, tx, iy, 15);
        video_draw_cursor(tx + pos * CHAR_WIDTH, iy);
        key_event_t ev;
        js_wait_key(&ev);
        char ch = ev.key;
        video_restore_cursor(tx + pos * CHAR_WIDTH, iy);
        if (ch == '\n' || ch == '\r') break;
        else if (ch == '\b') {
            if (pos > 0) { pos--; out[pos] = '\0'; }
        } else if (ch >= 32 && pos < out_size - 1) {
            out[pos++] = ch;
            out[pos] = '\0';
        }
    }
    return 1;
}

/* ─── CofeuTarayici DOM/CSS/layout tabanlı renderer ─────────
   HTML -> DOM -> stil -> kutu ağacı; çizim video_*32 ile yapılır. */

static u32 web_color_to_fb(u32 rgb) {
    return ((rgb & 0xFFu) << 16) | (rgb & 0xFF00u) | ((rgb >> 16) & 0xFFu);
}

static void browser_free_page(desktop_window_t *w) {
    if (w->layout) { web_free_boxes(w->layout); w->layout = NULL; }
    if (w->css)    { web_free_css(w->css); w->css = NULL; }
    if (w->doc)    { web_free_document(w->doc); w->doc = NULL; }
    w->page_h = 0;
}

static void browser_navigate_url(desktop_window_t *w, const char *url);
static void resolve_url(const char *base, const char *href, char *out, int out_size);
static void browser_relayout_page(desktop_window_t *w);
static int browser_handle_js_navigation(desktop_window_t *w);

/* window.location = ile yonlendirme; sonsuz donguyu onle */
static int g_js_redirects;

static void browser_parse_page(desktop_window_t *w) {
    browser_free_page(w);
    if (w->page_len <= 0) return;
    w->doc = web_parse_html(w->page_buf, (unsigned int)w->page_len);
    if (!w->doc) return;
    web_node_t *st[2];
    int nst = web_get_elements_by_tag(w->doc->root, "style", st, 2);
    if (nst == 1 && st[0]->first_child)
        w->css = web_parse_css(st[0]->first_child->text,
                               (unsigned int)strlen(st[0]->first_child->text));

    /* JavaScript: <script> bloklarını sırayla çalıştır */
    js_reset();
    js_set_page(w->doc, w->css, w->url_buf);
    web_node_t *sc[8];
    int nsc = web_get_elements_by_tag(w->doc->root, "script", sc, 8);
    for (int i = 0; i < nsc; i++) {
        web_node_t *t = sc[i]->first_child;
        if (t && t->text)
            js_run(t->text, (int)strlen(t->text));
    }

    /* window.location.href atamasından doğan yönlendirme */
    const char *nav = js_get_pending_nav();
    if (nav && nav[0]) {
        js_clear_pending_nav();
        if (g_js_redirects >= 10) {
            g_js_redirects = 0;
            js_log_cb("JS: cok fazla yonlendirme, durduruldu");
        } else {
            g_js_redirects++;
            char url[160];
            resolve_url(w->page_base, nav, url, sizeof(url));
            if (url[0]) browser_navigate_url(w, url);
            return;
        }
    } else {
        g_js_redirects = 0;
    }

    browser_relayout_page(w);

    /* document.title -> pencere basligi */
    if (w->doc && w->doc->title && w->doc->title[0]) {
        strncpy(w->title, w->doc->title, sizeof(w->title) - 1);
        w->title[sizeof(w->title) - 1] = '\0';
    } else {
        strcpy(w->title, "CofeuTarayici");
    }
}

/* Dikdörtgeni content alanıyla kırpar; görünür ise 1 döner */
static int clip_to_area(int *rx, int *ry, int *rw, int *rh,
                        int ax, int ay, int aw, int ah) {
    if (*rx < ax) { *rw -= ax - *rx; *rx = ax; }
    if (*ry < ay) { *rh -= ay - *ry; *ry = ay; }
    if (*rx + *rw > ax + aw) *rw = ax + aw - *rx;
    if (*ry + *rh > ay + ah) *rh = ay + ah - *ry;
    return *rw > 0 && *rh > 0;
}

static void render_box_tree(web_box_t *b, int dx, int dy,
                            int ax, int ay, int aw, int ah) {
    web_run_t *r;
    web_box_t *c;
    if (b->has_bg) {
        int rx = b->x + dx, ry = b->y + dy, rw = b->w, rh = b->h;
        if (clip_to_area(&rx, &ry, &rw, &rh, ax, ay, aw, ah))
            video_fill_rect32(rx, ry, rw, rh, web_color_to_fb(b->bg_color));
    }
    if (b->border_w > 0) {
        int rx = b->x + dx, ry = b->y + dy, rw = b->w, rh = b->h;
        if (clip_to_area(&rx, &ry, &rw, &rh, ax, ay, aw, ah))
            video_draw_rect32(rx, ry, rw, rh, web_color_to_fb(b->border_color));
    }
    for (r = b->runs; r; r = r->next) {
        int rx = r->x + dx, ry = r->y + dy;
        if (rx + r->w < ax || rx > ax + aw || ry + r->h < ay || ry > ay + ah) continue;
        if (r->type == WEB_RUN_TEXT) {
            if (r->has_bg)
                video_fill_rect32(rx, ry, r->w, r->h, web_color_to_fb(r->bg));
            u32 col = web_color_to_fb(r->color);
            if (r->font_size == WEB_FONT_BIG)
                video_print_scaled(r->text, rx, ry, 2, col);
            else
                video_print32(r->text, rx, ry, col);
            if (r->is_link)
                video_fill_rect32(rx, ry + r->h - 2, r->w, 1, col);
        } else {
            video_fill_rect32(rx, ry, r->w, r->h, web_color_to_fb(r->bg));
            if (r->border_w > 0)
                video_draw_rect32(rx, ry, r->w, r->h, web_color_to_fb(r->border_color));
        }
    }
    for (c = b->first_child; c; c = c->next_sibling)
        render_box_tree(c, dx, dy, ax, ay, aw, ah);
}

static void browser_render_page(desktop_window_t *w, int ax, int ay, int aw, int ah) {
    if (!w->layout) return;
    int max_scroll = w->page_h - ah;
    if (max_scroll < 0) max_scroll = 0;
    if (w->page_scroll > max_scroll) w->page_scroll = max_scroll;
    if (w->page_scroll < 0) w->page_scroll = 0;
    render_box_tree(w->layout, ax + 8, ay + 6 - w->page_scroll, ax, ay, aw, ah);
}

/* Çizilen kutulardan, tıklama konumuna karşılık gelen en iç DOM elemanını bulur. */
static web_node_t *find_click_node(web_box_t *b, int dx, int dy, int sx, int sy) {
    web_node_t *hit = NULL;
    for (web_box_t *c = b->first_child; c; c = c->next_sibling) {
        web_node_t *child = find_click_node(c, dx, dy, sx, sy);
        if (child) hit = child;
    }
    for (web_run_t *r = b->runs; r; r = r->next) {
        if (sx >= r->x + dx && sx < r->x + dx + r->w &&
            sy >= r->y + dy && sy < r->y + dy + r->h)
            hit = r->node;
    }
    if (!hit && sx >= b->x + dx && sx < b->x + dx + b->w &&
        sy >= b->y + dy && sy < b->y + dy + b->h)
        hit = b->node;
    while (hit && hit->type != WEB_NODE_ELEMENT) hit = hit->parent;
    return hit;
}

static void browser_relayout_page(desktop_window_t *w) {
    if (w->layout) { web_free_boxes(w->layout); w->layout = NULL; }
    if (w->doc)
        w->page_h = web_layout(w->doc->root, w->css, w->w - 24, w->h - 56, &w->layout);
}

/* Ekran koordinatındaki tıklamanın üzerindeki <a href> değerini döner */
static const char *find_link_run(web_box_t *b, int dx, int dy, int sx, int sy) {
    web_run_t *r;
    web_box_t *c;
    for (r = b->runs; r; r = r->next) {
        if (r->is_link && sx >= r->x + dx && sx < r->x + dx + r->w &&
            sy >= r->y + dy && sy < r->y + dy + r->h) {
            if (r->node && r->node->parent)
                return web_node_attr(r->node->parent, "href");
            return NULL;
        }
    }
    for (c = b->first_child; c; c = c->next_sibling) {
        const char *h = find_link_run(c, dx, dy, sx, sy);
        if (h) return h;
    }
    return NULL;
}

/* Göreli bağlantıyı taban URL'e göre çözer */
static void resolve_url(const char *base, const char *href, char *out, int out_size) {
    if (strncmp(href, "http://", 7) == 0 || strncmp(href, "https://", 8) == 0) {
        strncpy(out, href, out_size - 1);
        out[out_size - 1] = '\0';
        return;
    }
    const char *scheme = strstr(base, "://");
    if (!scheme) {
        strncpy(out, href, out_size - 1);
        out[out_size - 1] = '\0';
        return;
    }
    const char *scheme_end = scheme + 3;
    const char *host_start = scheme_end;
    const char *host_end = host_start;
    while (*host_end && *host_end != '/') host_end++;
    int n = 0;
    const char *sp = base;
    while (sp < scheme_end && n < out_size - 1) out[n++] = *sp++;
    while (host_start < host_end && n < out_size - 1) out[n++] = *host_start++;
    while (n < out_size - 1 && *href) out[n++] = *href++;
    out[n] = '\0';
    (void)base;
}

static void browser_navigate_url(desktop_window_t *w, const char *url);

static void browser_click_page(desktop_window_t *w, int sx, int sy) {
    if (!w->layout || w->page_len <= 0) return;
    int ax = w->x + 6, ay = w->y + 52;
    web_node_t *node = find_click_node(w->layout, ax + 8, ay + 6 - w->page_scroll, sx, sy);
    if (node && js_dispatch_click(node)) {
        if (browser_handle_js_navigation(w)) return;
        browser_relayout_page(w);
    }
    const char *href = find_link_run(w->layout, ax + 8, ay + 6 - w->page_scroll, sx, sy);
    if (href) {
        char url[160];
        resolve_url(w->page_base, href, url, sizeof(url));
        if (url[0]) browser_navigate_url(w, url);
    }
}

static void browser_navigate_url(desktop_window_t *w, const char *url) {
    char host[128];
    char path[256];
    int use_https = 0;
    unsigned short port = 80;
    if (parse_http_url(url, host, sizeof(host), path, sizeof(path), &use_https, &port) != 0) {
        w->page_len = -1;
        w->page_scroll = 0;
        return;
    }
    int len = use_https ? https_get_port(host, path, w->page_buf, (int)sizeof(w->page_buf) - 1, port)
                        : http_get_port(host, path, w->page_buf, (int)sizeof(w->page_buf) - 1, port);
    if (len < 0) {
        w->page_len = -1;
        w->page_scroll = 0;
        return;
    }
    w->page_len = len;
    w->page_buf[len] = '\0';
    strncpy(w->url_buf, url, sizeof(w->url_buf) - 1);
    w->url_buf[sizeof(w->url_buf) - 1] = '\0';
    w->url_pos = (int)strlen(w->url_buf);
    strncpy(w->page_base, url, sizeof(w->page_base) - 1);
    w->page_base[sizeof(w->page_base) - 1] = '\0';
    w->page_scroll = 0;
    browser_parse_page(w);
}

static int browser_handle_js_navigation(desktop_window_t *w) {
    const char *nav = js_get_pending_nav();
    if (!nav || !nav[0]) return 0;
    char requested[160];
    strncpy(requested, nav, sizeof(requested) - 1);
    requested[sizeof(requested) - 1] = '\0';
    js_clear_pending_nav();
    char url[160];
    resolve_url(w->page_base, requested, url, sizeof(url));
    if (url[0]) browser_navigate_url(w, url);
    return 1;
}

static void browser_navigate(desktop_window_t *w) {
    w->url_buf[w->url_pos] = '\0';
    browser_navigate_url(w, w->url_buf);
}

/* ─── Hesap Makinesi ─────────────────────────────────────────────── */
static const char *calc_labels[16] = {"7","8","9","/","4","5","6","*","1","2","3","-","C","0","=","+"};

static void calc_set_disp_int(desktop_window_t *w, int v) {
    char tmp[16];
    int k = 0, n = 0;
    if (v == 0) { w->calc_disp[0] = '0'; w->calc_disp[1] = '\0'; return; }
    if (v < 0) { w->calc_disp[n++] = '-'; v = -v; }
    while (v > 0) { tmp[k++] = (char)('0' + v % 10); v /= 10; }
    while (k > 0) w->calc_disp[n++] = tmp[--k];
    w->calc_disp[n] = '\0';
}

static void calc_press(desktop_window_t *w, int idx) {
    const char *s = calc_labels[idx];
    char ch = s[0];
    int len;

    if (idx == 12) { /* C: temizle */
        w->calc_disp[0] = '0'; w->calc_disp[1] = '\0';
        w->calc_acc = 0; w->calc_op = 0; w->calc_entered = 0;
        return;
    }
    if (idx == 14) { /* = */
        int right = shell_atoi(w->calc_disp);
        int result;
        if (w->calc_op == '+')      result = w->calc_acc + right;
        else if (w->calc_op == '-') result = w->calc_acc - right;
        else if (w->calc_op == '*') result = w->calc_acc * right;
        else if (w->calc_op == '/') result = right ? w->calc_acc / right : 0;
        else                        result = right;
        w->calc_acc = 0; w->calc_op = 0;
        calc_set_disp_int(w, result);
        w->calc_entered = 1;
        return;
    }
    if (ch >= '0' && ch <= '9') {
        if (w->calc_entered) { w->calc_disp[0] = '\0'; w->calc_entered = 0; }
        len = (int)strlen(w->calc_disp);
        if (len < 14) { w->calc_disp[len] = ch; w->calc_disp[len + 1] = '\0'; }
    } else { /* + - * / */
        if (w->calc_op && !w->calc_entered) {
            int right = shell_atoi(w->calc_disp);
            if (w->calc_op == '+')      w->calc_acc += right;
            else if (w->calc_op == '-') w->calc_acc -= right;
            else if (w->calc_op == '*') w->calc_acc *= right;
            else if (w->calc_op == '/') w->calc_acc = right ? w->calc_acc / right : 0;
            calc_set_disp_int(w, w->calc_acc);
        } else {
            w->calc_acc = shell_atoi(w->calc_disp);
        }
        w->calc_op = ch;
        w->calc_entered = 1;
    }
}

static desktop_window_t g_windows[MAX_GUI_WINDOWS];
static int g_window_count = 0;
static int g_active_win = -1;
static int g_drag_win = -1;
static int g_drag_off_x = 0;
static int g_drag_off_y = 0;

static void desktop_open_program(int type) {
    for (int i = 0; i < g_window_count; i++) {
        if (g_windows[i].active && g_windows[i].type == type) {
            g_windows[i].minimized = 0;
            g_active_win = i;
            return;
        }
    }

    int slot = -1;
    for (int i = 0; i < g_window_count; i++) {
        if (!g_windows[i].active) { slot = i; break; }
    }
    if (slot == -1 && g_window_count < MAX_GUI_WINDOWS) {
        slot = g_window_count++;
    }
    if (slot == -1) return;

    desktop_window_t *w = &g_windows[slot];
    memset(w, 0, sizeof(desktop_window_t));
    w->id = slot + 1;
    w->active = 1;
    w->minimized = 0;
    w->type = type;

    if (type == WINDOW_TYPE_TERMINAL) {
        strcpy(w->title, "cofeuTerminal");
        w->x = 95; w->y = 25; w->w = 680; w->h = 420;
        w->term_cx = 0; w->term_cy = 0;
        w->input_pos = 0; w->input_buf[0] = '\0';
        g_gui_term_target = w;
        shell_print("cofeuOS GUI Terminal v3.0\n", 14);
        shell_print("Komut yazabilirsiniz (orn: ls, neofetch, ping, python)\n", 11);
        shell_print("--------------------------------------------------\n", 7);
        g_gui_term_target = NULL;
    } else if (type == WINDOW_TYPE_FILES) {
        strcpy(w->title, "Dosya Yoneticisi");
        w->x = 110; w->y = 45; w->w = 480; w->h = 290;
    } else if (type == WINDOW_TYPE_NOTES) {
        strcpy(w->title, "Not Defteri");
        w->x = 125; w->y = 65; w->w = 440; w->h = 270;
    } else if (type == WINDOW_TYPE_INFO) {
        strcpy(w->title, "Sistem Bilgisi");
        w->x = 140; w->y = 50; w->w = 400; w->h = 260;
    } else if (type == WINDOW_TYPE_CALC) {
        strcpy(w->title, "Hesap Makinesi");
        w->x = 160; w->y = 70; w->w = 340; w->h = 250;
        w->calc_disp[0] = '0'; w->calc_disp[1] = '\0';
        w->calc_acc = 0; w->calc_op = 0; w->calc_entered = 0;
    } else if (type == WINDOW_TYPE_BROWSER) {
        strcpy(w->title, "CofeuTarayici");
        w->x = 70; w->y = 30; w->w = 620; w->h = 420;
        strcpy(w->url_buf, "http://example.com");
        w->url_pos = (int)strlen(w->url_buf);
        w->page_len = 0;
        w->page_scroll = 0;
    }

    g_active_win = slot;
}

static void gui_term_print_string(desktop_window_t *w, const char *str, u8 color) {
    if (!w) return;
    for (const char *p = str; *p; p++) {
        gui_term_print_char(w, *p, color);
    }
}

/* GUI terminali pencere dışına taşırmadan aktif komut satırını çizer. */
static void gui_term_draw_prompt(desktop_window_t *w, int x, int y, int focused) {
    char line[GUI_TERM_COLS + 1];
    int max_cols = (w->w - 20) / CHAR_WIDTH;
    if (max_cols > GUI_TERM_COLS) max_cols = GUI_TERM_COLS;
    int n = 0;
    const char *user = g_shell.user;
    while (*user && n < 12 && n < max_cols - 1) line[n++] = *user++;
    const char *suffix = "@cofeu $ ";
    for (int i = 0; suffix[i] && n < max_cols - 1; i++) line[n++] = suffix[i];
    for (int i = 0; w->input_buf[i] && n < max_cols - 1; i++) line[n++] = w->input_buf[i];
    if (focused && n < max_cols) line[n++] = '_';
    line[n] = '\0';
    video_print(line, x, y, 15);
}

static void desktop_draw_single_window(desktop_window_t *w, int is_focused) {
    if (!w || !w->active || w->minimized) return;

    /* Başlık Çubuğu */
    u8 title_bg = is_focused ? 9 : 8;
    video_fill_rect(w->x, w->y, w->w, 22, title_bg);
    video_print(w->title, w->x + 10, w->y + 3, 15);

    /* Kapatma [X] Butonu */
    int btn_x = w->x + w->w - 20;
    int btn_y = w->y + 3;
    video_fill_rect(btn_x, btn_y, 16, 16, 12);
    video_draw_char('X', btn_x + 4, btn_y, 15);

    /* Küçült [–] Butonu */
    int min_x = w->x + w->w - 40;
    video_fill_rect(min_x, btn_y, 16, 16, 1);
    video_draw_rect(min_x, btn_y, 16, 16, 15);
    video_fill_rect(min_x + 3, btn_y + 11, 10, 2, 15);

    /* Gövde Alanı */
    video_fill_rect(w->x, w->y + 22, w->w, w->h - 22, 7);
    video_draw_rect(w->x, w->y, w->w, w->h, 15);

    /* İçerik */
    if (w->type == WINDOW_TYPE_TERMINAL) {
        /* Terminal İç Siyah Ekran */
        video_fill_rect(w->x + 6, w->y + 26, w->w - 12, w->h - 32, 0);

        /* Satırları fontun gerçek yüksekliğiyle çiz. */
        int start_y = w->y + 28;
        for (int r = 0; r < GUI_TERM_ROWS; r++) {
            int line_y = start_y + r * CHAR_HEIGHT;
            if (line_y + CHAR_HEIGHT > w->y + w->h - 30) break;

            for (int c = 0; c < GUI_TERM_COLS; c++) {
                char ch = w->term_screen[r][c];
                u8 col = w->term_colors[r][c] ? w->term_colors[r][c] : 15;
                if (ch && ch >= 32) {
                    video_draw_char(ch, w->x + 10 + c * CHAR_WIDTH, line_y, col);
                }
            }
        }

        /* Aktif Prompt Satırı */
        int prompt_y = start_y + w->term_cy * CHAR_HEIGHT;
        if (prompt_y + CHAR_HEIGHT <= w->y + w->h - 28) {
            gui_term_draw_prompt(w, w->x + 10, prompt_y, is_focused);
        }
    } else if (w->type == WINDOW_TYPE_FILES) {
        video_print("Dizin: ", w->x + 12, w->y + 32, 0);
        video_print(g_shell.cwd, w->x + 65, w->y + 32, 1);
        video_fill_rect(w->x + 12, w->y + 50, w->w - 24, 1, 8);

        char buf[512];
        int sz = fs_list_dir(&g_fs, g_shell.cwd, buf, sizeof(buf));
        if (sz >= 0 && buf[0]) {
            video_print(buf, w->x + 16, w->y + 60, 0);
        } else {
            video_print("(Dizin bos)", w->x + 16, w->y + 60, 8);
        }
    } else if (w->type == WINDOW_TYPE_NOTES) {
        char note[256];
        int sz = fs_read_file(&g_fs, "/home/notes.txt", note, sizeof(note) - 1);
        video_print("Dosya: /home/notes.txt", w->x + 12, w->y + 32, 1);
        video_fill_rect(w->x + 12, w->y + 50, w->w - 24, 1, 8);
        if (sz >= 0) {
            note[sz] = '\0';
            video_print(note, w->x + 16, w->y + 60, 0);
        } else {
            video_print("Not bulunamadi. Terminalden write ile ekleyebilirsiniz.", w->x + 16, w->y + 60, 0);
        }
    } else if (w->type == WINDOW_TYPE_INFO) {
        video_print("cofeuOS v3.0 Desktop Edition", w->x + 15, w->y + 32, 9);
        video_print("----------------------------", w->x + 15, w->y + 48, 8);
        video_print("Mimar: x86_64 UEFI GOP", w->x + 15, w->y + 66, 0);
        video_print("GUI: cofeuDE v2 (Mouse + Window Manager)", w->x + 15, w->y + 84, 0);
        video_print("Scripting: MicroPython Embed", w->x + 15, w->y + 102, 0);
        video_print("Bellek: 16MB Arena Allocator", w->x + 15, w->y + 120, 0);
        video_print("Ag Katmani: DHCP + TCP/IP + HTTP", w->x + 15, w->y + 138, 0);
    } else if (w->type == WINDOW_TYPE_CALC) {
        /* Ekran */
        video_fill_rect(w->x + 15, w->y + 30, w->w - 30, 34, 0);
        video_draw_rect(w->x + 15, w->y + 30, w->w - 30, 34, 8);
        video_print(w->calc_disp, w->x + 22, w->y + 38, 10);

        /* Buton Izgarası: 4x4 */
        int bw = (w->w - 38) / 4;
        int bx = w->x + 15;
        int by = w->y + 74;
        for (int i = 0; i < 16; i++) {
            int r = i / 4, c = i % 4;
            int ccx = bx + c * (bw + 2);
            int ccy = by + r * 32;
            video_fill_rect(ccx, ccy, bw, 30, 2);
            video_draw_rect(ccx, ccy, bw, 30, 15);
            video_print(calc_labels[i], ccx + 6, ccy + 6, 15);
        }
    } else if (w->type == WINDOW_TYPE_BROWSER) {
        /* Adres Çubuğu */
        video_fill_rect(w->x + 8, w->y + 26, w->w - 16, 20, 0);
        video_draw_rect(w->x + 8, w->y + 26, w->w - 16, 20, 8);
        video_print(w->url_buf, w->x + 12, w->y + 29, 15);
        if (is_focused) {
            int ux = w->x + 12 + video_text_width(w->url_buf);
            video_draw_char('_', ux, w->y + 29, 11);
        }
        /* GO Butonu */
        video_fill_rect(w->x + w->w - 60, w->y + 27, 52, 18, 3);
        video_draw_rect(w->x + w->w - 60, w->y + 27, 52, 18, 15);
        video_print("GO", w->x + w->w - 49, w->y + 29, 15);

        /* İçerik Alanı (beyaz zemin: web sayfaları koyu metin) */
        int cy = w->y + 52;
        int ch = w->h - 56;
        video_fill_rect(w->x + 6, cy, w->w - 12, ch, 15);
        video_draw_rect(w->x + 6, cy, w->w - 12, ch, 8);

        if (w->page_len < 0) {
            video_print("Yuklenemedi: URL gecersiz veya ag hatasi.", w->x + 14, cy + 6, 12);
            video_print("Ag baglantisi icin 'ping'/'wget' ile test edin.", w->x + 14, cy + 24, 12);
        } else if (w->page_len == 0) {
            video_print("Adres girip Enter'a basin veya GO'a tiklayin.", w->x + 14, cy + 6, 8);
            video_print("Ornek: http://example.com", w->x + 14, cy + 24, 8);
        } else {
            int cy = w->y + 52;
            int ch = w->h - 56;
            browser_render_page(w, w->x + 6, cy, w->w - 12, ch);
        }
    }
}

/* ─── PS/2 Tek Okuyucu (UEFI Simple Pointer yoksa) ───────────────────
   QEMU/OVMF fareyi (Simple Pointer) sağlamaz; fare PS/2'dir ve klavye
   ile AYNI 8042 çıkış tamponunu (0x60) paylaşır. Bu yüzden iki ayrı
   tüketici (OVMF ConIn + bizim fare okuyucu) çakışır: fare okumak
   klavye tuşlarını çalar. Çözüm: TEK okuyucu. Kernel 0x60'ı okur,
   Set-1 tarama kodlarını klavyeye, bit3'lü baytları fare paketine
   ayırır. Fark:
     - bit7'li bayt      -> klavye break/prefix (yok say)
     - bilinen klavye    -> klavye (Shift, Ctrl, 0x40-0x53 arası)
     - bit3'lü bayt      -> fare paket başlangıcı
     - geri kalan        -> klavye make
   Sonuç: 7/8/9/0 tuşları fare baytlarıyla birebir aynı olduğundan
   kaybolur; geri kalan her şey (harfler, 1-6, noktalama, Enter,
   backspace, Shift, ok tuşları) çalışır. */
static int g_ps2_mouse_ready = 0;
static int g_ps2_mouse_phase = 0;
static u8  g_ps2_pkt[3];
static int g_ps2_e0 = 0;
static int g_ps2_shift = 0;

static int g_ps2_kq_head = 0;
static int g_ps2_kq_tail = 0;
static key_event_t g_ps2_kq[16];

static int g_ps2_dbg_count = 0;
static void ps2_dbg_byte(u8 b) {
    if (g_ps2_dbg_count >= 60) return;
    g_ps2_dbg_count++;
    static const char hex[] = "0123456789ABCDEF";
    char tmp[4] = {' ', hex[b >> 4], hex[b & 0xF], 0};
    dbg_write(tmp);
}

static void ps2_kq_push(char key, int scan) {
    int next = (g_ps2_kq_head + 1) % 16;
    if (next == g_ps2_kq_tail) return;   /* kuyruk dolu */
    g_ps2_kq[g_ps2_kq_head].key = key;
    g_ps2_kq[g_ps2_kq_head].scan_code = scan;
    g_ps2_kq_head = next;
}

static int ps2_kq_pop(key_event_t *ev) {
    if (g_ps2_kq_tail == g_ps2_kq_head) return 0;
    *ev = g_ps2_kq[g_ps2_kq_tail];
    g_ps2_kq_tail = (g_ps2_kq_tail + 1) % 16;
    return 1;
}

/* PS/2 Set-1 tarama kodu -> ASCII (bastırmasız) */
static const char ps2_key_base[0x60] = {
    0, 0, '1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,'a','s',
    'd','f','g','h','j','k','l',';','\'','`', 0,'\\','z','x','c','v',
    'b','n','m',',','.','/', 0,'*', 0,' ', 0
};

/* Shift basılıyken */
static const char ps2_key_shift[0x60] = {
    0, 0, '!','@','#','$','%','^','&','*','(',')','_','+','\b','\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0,'A','S',
    'D','F','G','H','J','K','L',':','"','~', 0,'|','Z','X','C','V',
    'B','N','M','<','>','?', 0,'*', 0,' ', 0
};

static inline u8 ps2_inb(u16 port) {
    u8 v;
    __asm__ volatile("inb %w1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void ps2_outb(u16 port, u8 v) {
    __asm__ volatile("outb %0, %w1" : : "a"(v), "Nd"(port));
}

static void ps2_delay(void) {
    volatile u32 i;
    for (i = 0; i < 1500; i++) __asm__ volatile("nop");
}

static void ps2_wait_input_empty(void) {
    for (int i = 0; i < 5000; i++) {
        if (!(ps2_inb(0x64) & 0x02)) return;
        ps2_delay();
    }
}

static void ps2_flush_output(void) {
    for (int i = 0; i < 100; i++) {
        if (ps2_inb(0x64) & 0x01) { (void)ps2_inb(0x60); ps2_delay(); }
        else return;
    }
}

static void ps2_mouse_init(void) {
    /* 8042 aux kanalını aç (klavye kanalına dokunulmaz) */
    ps2_wait_input_empty();
    ps2_outb(0x64, 0xA8);               /* aux cihazı etkin */
    ps2_delay();
    ps2_wait_input_empty();
    ps2_outb(0x64, 0x20);               /* komut baytını iste */
    ps2_delay();
    u8 cb = ps2_inb(0x60);
    cb &= ~0x04;                        /* aux devre dışı değil */
    cb &= ~0x20;                        /* aux saat sinyali açık */
    ps2_wait_input_empty();
    ps2_outb(0x64, 0x60);               /* komut baytını yaz */
    ps2_delay();
    ps2_outb(0x60, cb);
    ps2_delay();
    ps2_flush_output();
    ps2_wait_input_empty();
    ps2_outb(0x64, 0xD4);               /* sonraki veri aux'a gider */
    ps2_delay();
    ps2_wait_input_empty();
    ps2_outb(0x60, 0xF4);               /* data raporlamayı başlat */
    ps2_delay();
    ps2_flush_output();
    g_ps2_mouse_ready = 1;
}

static void ps2_decode_mouse(u8 p0, u8 b1, u8 b2, mouse_state_t *ms) {
    int dx = b1 & 0x80 ? (int)b1 - 256 : (int)b1;
    int dy = b2 & 0x80 ? (int)b2 - 256 : (int)b2;
    if (g_ps2_dbg_count < 60) {
        char tmp[4] = {'M', (p0 & 1) ? 'L' : '-', ' ', 0};
        dbg_write(tmp);
    }
    ms->dx += dx * 2;
    ms->dy += -dy * 2;
    if (ms->dx > 60) ms->dx = 60;
    if (ms->dx < -60) ms->dx = -60;
    if (ms->dy > 60) ms->dy = 60;
    if (ms->dy < -60) ms->dy = -60;
    if (p0 & 0x01) ms->left_btn = 1;
    if (p0 & 0x02) ms->right_btn = 1;
}

static void ps2_read_both(mouse_state_t *ms, key_event_t *ev) {
    if (!g_ps2_mouse_ready) ps2_mouse_init();
    ms->dx = 0; ms->dy = 0; ms->dz = 0;
    ms->left_btn = 0; ms->right_btn = 0;
    ev->key = 0; ev->scan_code = 0;

    for (int guard = 0; guard < 48; guard++) {
        if (!(ps2_inb(0x64) & 0x01)) break;
        u8 st = ps2_inb(0x64);
        u8 b = ps2_inb(0x60);
        int is_mouse = (st & 0x20) != 0;
        ps2_dbg_byte(b);

        if (is_mouse) {
            /* 8042 status bit 0x20: bayt aux (fare) kanalından. */
            if (g_ps2_mouse_phase != 0) {
                g_ps2_pkt[g_ps2_mouse_phase++] = b;
                if (g_ps2_mouse_phase == 3) {
                    g_ps2_mouse_phase = 0;
                    ps2_decode_mouse(g_ps2_pkt[0], g_ps2_pkt[1], g_ps2_pkt[2], ms);
                }
            } else {
                g_ps2_pkt[0] = b;
                g_ps2_mouse_phase = 1;
            }
            continue;
        }

        /* Klavye baytı; paket ortasındaysak senkron bozulmuş, paketi düşür. */
        if (g_ps2_mouse_phase != 0) g_ps2_mouse_phase = 0;

        /* E0 öneki: ok tuşları */
        if (g_ps2_e0) {
            g_ps2_e0 = 0;
            if (b == 0x48) ps2_kq_push(0, 1);       /* Up */
            else if (b == 0x50) ps2_kq_push(0, 2);  /* Down */
            else if (b == 0x4D) ps2_kq_push(0, 3);  /* Right */
            else if (b == 0x4B) ps2_kq_push(0, 4);  /* Left */
            continue;
        }

        if (b == 0xE0) { g_ps2_e0 = 1; continue; }

        /* Break kodları */
        if (b & 0x80) {
            if (b == 0xAA || b == 0xB6) g_ps2_shift = 0;
            continue;
        }

        /* Bilinen klavye make kodları */
        switch (b) {
            case 0x1D:                        /* sol ctrl */
            case 0x38: continue;              /* alt */
            case 0x2A: case 0x36: g_ps2_shift = 1; continue;   /* shift */
            case 0x3A: case 0x3B: case 0x3C: case 0x3D: case 0x3E: case 0x3F:
            case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45:
            case 0x46: case 0x47: case 0x48: case 0x49: case 0x4A: case 0x4B:
            case 0x4C: case 0x4D: case 0x4E: case 0x4F: case 0x50: case 0x51:
            case 0x52: case 0x53: case 0x57: case 0x58: continue;  /* F-key/numpad */
            default: break;
        }

        /* Klavye make -> ASCII */
        char c = 0;
        if (b < 0x60) c = g_ps2_shift ? ps2_key_shift[b] : ps2_key_base[b];
        if (c) ps2_kq_push(c, 0);
    }

    ps2_kq_pop(ev);
}

static void desktop_input_read(mouse_state_t *ms, key_event_t *ev) {
    ev->key = 0; ev->scan_code = 0;
    ms->dx = 0; ms->dy = 0; ms->dz = 0;
    ms->left_btn = 0; ms->right_btn = 0;
    if (mouse_get_state(ms) == 0) return;   /* UEFI Simple Pointer varsa: klavye ConIn'dan */
    ps2_read_both(ms, ev);                  /* yoksa tek okuyucu */
}

static void desktop_draw_clock(int seconds) {
    char buf[12];
    int h = seconds / 3600;
    int m = (seconds % 3600) / 60;
    int s = seconds % 60;
    int i = 0;
    buf[i++] = (char)('0' + h / 10);
    buf[i++] = (char)('0' + h % 10);
    buf[i++] = ':';
    buf[i++] = (char)('0' + m / 10);
    buf[i++] = (char)('0' + m % 10);
    buf[i++] = ':';
    buf[i++] = (char)('0' + s / 10);
    buf[i++] = (char)('0' + s % 10);
    buf[i] = '\0';
    video_print(buf, SCREEN_WIDTH - 70, SCREEN_HEIGHT - 19, 15);
}

static int cmd_js(int argc, char** argv) {
    if (argc < 2) {
        shell_print("kullanim: js <dosya>  (JavaScript calistirir)", 12);
        shell_newline();
        return -1;
    }
    js_init();
    js_set_log_cb(js_log_cb);
    js_set_alert_cb(js_alert_cb);
    js_set_prompt_cb(js_prompt_cb);
    js_set_confirm_cb(js_confirm_cb);
    js_reset();
    char content[4096];
    int r = fs_read_file(&g_fs, argv[1], content, sizeof(content));
    if (r < 0) {
        shell_print("js: dosya bulunamadi: ", 12);
        shell_print(argv[1], 12);
        shell_newline();
        return -1;
    }
    if (js_run(content, r) != 0) {
        shell_print("js: hata!", 12);
        shell_newline();
        return -1;
    }
    return 0;
}

static int cmd_desktop(int argc, char** argv) {
    (void)argc; (void)argv;

    /* JS motoru geri çağrımlarını bağla */
    js_init();
    js_set_log_cb(js_log_cb);
    js_set_alert_cb(js_alert_cb);
    js_set_prompt_cb(js_prompt_cb);
    js_set_confirm_cb(js_confirm_cb);

    fs_create_dir(&g_fs, "/home");
    if (!fs_file_exists(&g_fs, "/home/notes.txt"))
        fs_create_file(&g_fs, "/home/notes.txt", "cofeuDE Masaustune Hosgeldiniz!", 28);

    /* Başlangıçta Terminal Penceresini Aç */
    desktop_open_program(WINDOW_TYPE_TERMINAL);

    int mx = SCREEN_WIDTH / 2;
    int my = SCREEN_HEIGHT / 2;
    int prev_mx = -1;
    int prev_my = -1;
    int start_menu_open = 0;
    int prev_left_btn = 0;
    int needs_redraw = 1;
    int clock_sec = 0;
    int clock_ms = 0;

    video_clear(1);

    while (1) {
        /* 1. Fare + Klavye Girişi (tek okuyucu).
           UEFI Simple Pointer varsa fare SimplePointer'dan, klavye
           ConIn'dan okunur. Yoksa PS/2 tek okuyucu hem fare paketlerini
           hem klavye tarama kodlarını 8042 tamponundan çevirir; OVMF
           ConIn sadece yedek olarak yoklanır. */
        mouse_state_t ms;
        key_event_t ev = {0, 0};
        desktop_input_read(&ms, &ev);
        if (ev.key == 0 && ev.scan_code == 0)
            ev = try_read_key_event();      /* ConIn yedeği */

        int moved = 0;
        if (ms.dx != 0 || ms.dy != 0) {
            mx += ms.dx;
            my += ms.dy;
            moved = 1;
        }

        /* 2. Klavye Girişi ve Ok Tuşları ile İmleç Kontrolü */
        if (ev.scan_code != 0 || ev.key != 0) {
            /* Aktif pencere tarayıcıysa ok tuşları sayfayı kaydırır */
            int br_focus = (g_active_win >= 0 && g_active_win < g_window_count &&
                            g_windows[g_active_win].active &&
                            g_windows[g_active_win].type == WINDOW_TYPE_BROWSER);
            if (br_focus && (ev.scan_code == 1 || ev.scan_code == 2)) {
                desktop_window_t *bw = &g_windows[g_active_win];
                if (ev.scan_code == 1 && bw->page_scroll > 0) { bw->page_scroll -= 18; needs_redraw = 1; }
                else if (ev.scan_code == 2) { bw->page_scroll += 18; needs_redraw = 1; }
            } else if (ev.scan_code == 1) { my -= 12; moved = 1; }      /* Up */
            else if (ev.scan_code == 2) { my += 12; moved = 1; }  /* Down */
            else if (ev.scan_code == 3) { mx += 12; moved = 1; }  /* Right */
            else if (ev.scan_code == 4) { mx -= 12; moved = 1; }  /* Left */

            /* Aktif Pencereye Klavye Girişi */
            if (ev.key != 0 && g_active_win >= 0 && g_active_win < g_window_count && g_windows[g_active_win].active) {
                desktop_window_t *w = &g_windows[g_active_win];

                if (w->type == WINDOW_TYPE_TERMINAL) {
                    if (ev.key == '\n') {
                        gui_term_print_string(w, g_shell.user, 10);
                        gui_term_print_string(w, "@cofeu # ", 14);
                        gui_term_print_string(w, w->input_buf, 15);
                        gui_term_print_char(w, '\n', 15);

                        if (w->input_pos > 0) {
                            w->input_buf[w->input_pos] = '\0';

                            /* Çıktıyı bu pencereye yönlendir */
                            g_gui_term_target = w;
                            shell_execute(w->input_buf);
                            g_gui_term_target = NULL;
                        }
                        w->input_pos = 0;
                        w->input_buf[0] = '\0';
                        needs_redraw = 1;
                    } else if (ev.key == '\b') {
                        if (w->input_pos > 0) {
                            w->input_pos--;
                            w->input_buf[w->input_pos] = '\0';
                            needs_redraw = 1;
                        }
                    } else if (ev.key >= 32 && w->input_pos < (int)sizeof(w->input_buf) - 1) {
                        w->input_buf[w->input_pos++] = ev.key;
                        w->input_buf[w->input_pos] = '\0';
                        needs_redraw = 1;
                    }
                } else if (w->type == WINDOW_TYPE_BROWSER) {
                    if (ev.key == '\n') {
                        browser_navigate(w);
                        needs_redraw = 1;
                    } else if (ev.key == '\b') {
                        if (w->url_pos > 0) {
                            w->url_pos--;
                            w->url_buf[w->url_pos] = '\0';
                            needs_redraw = 1;
                        }
                    } else if (ev.key >= 32 && w->url_pos < (int)sizeof(w->url_buf) - 1) {
                        w->url_buf[w->url_pos++] = ev.key;
                        w->url_buf[w->url_pos] = '\0';
                        needs_redraw = 1;
                    }
                }
            }
        }

        /* Ekran Sınırlarını Koruma */
        if (mx < 0) mx = 0;
        if (mx >= SCREEN_WIDTH) mx = SCREEN_WIDTH - 1;
        if (my < 0) my = 0;
        if (my >= SCREEN_HEIGHT) my = SCREEN_HEIGHT - 1;

        /* Pencere Sürükleme Mantığı */
        if (ms.left_btn && g_drag_win >= 0 && g_drag_win < g_window_count && g_windows[g_drag_win].active) {
            desktop_window_t *w = &g_windows[g_drag_win];
            w->x = mx - g_drag_off_x;
            w->y = my - g_drag_off_y;
            if (w->x < 0) w->x = 0;
            if (w->y < 0) w->y = 0;
            if (w->x + w->w > SCREEN_WIDTH) w->x = SCREEN_WIDTH - w->w;
            if (w->y + w->h > SCREEN_HEIGHT - 26) w->y = SCREEN_HEIGHT - 26 - w->h;
            needs_redraw = 1;
        } else if (!ms.left_btn) {
            g_drag_win = -1;
        }

        int click = (ms.left_btn && !prev_left_btn);
        prev_left_btn = ms.left_btn;

        if (click) {
            needs_redraw = 1;

            /* Başlat Butonu Tıklaması */
            if (mx >= 4 && mx <= 79 && my >= SCREEN_HEIGHT - 26) {
                start_menu_open = !start_menu_open;
            }
            /* Başlat Menüsü Elemanları Tıklaması */
            else if (start_menu_open && mx >= 4 && mx <= 144 && my >= SCREEN_HEIGHT - 195 && my < SCREEN_HEIGHT - 26) {
                if (my >= SCREEN_HEIGHT - 165 && my < SCREEN_HEIGHT - 145) desktop_open_program(WINDOW_TYPE_TERMINAL);
                else if (my >= SCREEN_HEIGHT - 145 && my < SCREEN_HEIGHT - 125) desktop_open_program(WINDOW_TYPE_FILES);
                else if (my >= SCREEN_HEIGHT - 125 && my < SCREEN_HEIGHT - 105) desktop_open_program(WINDOW_TYPE_NOTES);
                else if (my >= SCREEN_HEIGHT - 105 && my < SCREEN_HEIGHT - 85)  desktop_open_program(WINDOW_TYPE_INFO);
                else if (my >= SCREEN_HEIGHT - 85  && my < SCREEN_HEIGHT - 65)  desktop_open_program(WINDOW_TYPE_CALC);
                else if (my >= SCREEN_HEIGHT - 65  && my < SCREEN_HEIGHT - 45)  desktop_open_program(WINDOW_TYPE_BROWSER);
                else if (my >= SCREEN_HEIGHT - 45) {
                    video_clear(0); cursor_x = 5; cursor_y = 30;
                    shell_print("Masaustunden cikildi.\n", 10);
                    return 0;
                }
                start_menu_open = 0;
            }
            else {
                start_menu_open = 0;

                int handled = 0;

                /* Görev Çubuğu Sekmeleri: tıklanan pencereyi geri getir / odakla */
                int tab_x = 90;
                for (int i = 0; i < g_window_count; i++) {
                    if (g_windows[i].active) {
                        if (mx >= tab_x && mx < tab_x + 110 &&
                            my >= SCREEN_HEIGHT - 23 && my < SCREEN_HEIGHT - 3) {
                            g_windows[i].minimized = 0;
                            g_active_win = i;
                            handled = 1;
                            break;
                        }
                        tab_x += 115;
                    }
                }

                /* Önce Aktif Pencere Etkileşimini Kontrol Et */
                if (!handled && g_active_win >= 0 && g_active_win < g_window_count && g_windows[g_active_win].active) {
                    desktop_window_t *w = &g_windows[g_active_win];
                    int btn_x = w->x + w->w - 20;
                    int btn_y = w->y + 3;

                    /* Küçült [–] Butonuna mı Basıldı? */
                    int min_x = w->x + w->w - 40;
                    if (mx >= min_x && mx <= min_x + 16 && my >= btn_y && my <= btn_y + 16) {
                        int old = g_active_win;
                        w->minimized = 1;
                        g_active_win = -1;
                        for (int k = 0; k < g_window_count; k++) {
                            if (k != old && g_windows[k].active && !g_windows[k].minimized) {
                                g_active_win = k; break;
                            }
                        }
                        handled = 1;
                    }
                    /* Kapatma [X] Butonuna mı Basıldı? */
                    else if (mx >= btn_x && mx <= btn_x + 16 && my >= btn_y && my <= btn_y + 16) {
                        if (w->type == WINDOW_TYPE_BROWSER) browser_free_page(w);
                        w->active = 0;
                        w->minimized = 0;
                        g_active_win = -1;
                        for (int k = 0; k < g_window_count; k++) {
                            if (g_windows[k].active) { g_active_win = k; break; }
                        }
                        handled = 1;
                    }
                    /* Başlık Çubuğuna mı Basıldı? (Sürükleme Başlat) */
                    else if (mx >= w->x && mx <= w->x + w->w - 42 && my >= w->y && my <= w->y + 22) {
                        g_drag_win = g_active_win;
                        g_drag_off_x = mx - w->x;
                        g_drag_off_y = my - w->y;
                        handled = 1;
                    }
                    /* Tarayıcı GO Butonu mu? */
                    else if (w->type == WINDOW_TYPE_BROWSER &&
                             mx >= w->x + w->w - 60 && mx <= w->x + w->w - 8 &&
                             my >= w->y + 27 && my <= w->y + 45) {
                        browser_navigate(w);
                        needs_redraw = 1;
                        handled = 1;
                    }
                    /* Tarayıcı sayfa içi bağlantı tıklaması mı? */
                    else if (w->type == WINDOW_TYPE_BROWSER && w->page_len > 0 &&
                             mx >= w->x + 6 && mx <= w->x + w->w - 6 &&
                             my >= w->y + 52 && my <= w->y + w->h - 4) {
                        browser_click_page(w, mx, my);
                        needs_redraw = 1;
                        handled = 1;
                    }
                    /* Hesap Makinesi Butonları mı? */
                    else if (w->type == WINDOW_TYPE_CALC &&
                             mx >= w->x + 15 && mx <= w->x + w->w - 15 &&
                             my >= w->y + 74 && my <= w->y + 200) {
                        int c = (mx - (w->x + 15)) / ((w->w - 38) / 4 + 2);
                        int r = (my - (w->y + 74)) / 32;
                        if (c >= 0 && c < 4 && r >= 0 && r < 4) calc_press(w, r * 4 + c);
                        needs_redraw = 1;
                        handled = 1;
                    }
                    /* Pencere İçine mi Basıldı? */
                    else if (mx >= w->x && mx <= w->x + w->w && my >= w->y && my <= w->y + w->h) {
                        handled = 1;
                    }
                }

                /* Diğer Pencereler Arasında Geçiş / Odak */
                if (!handled) {
                    for (int i = g_window_count - 1; i >= 0; i--) {
                        if (g_windows[i].active) {
                            desktop_window_t *w = &g_windows[i];
                            int btn_x = w->x + w->w - 20;
                            int btn_y = w->y + 3;
                            if (mx >= btn_x && mx <= btn_x + 16 && my >= btn_y && my <= btn_y + 16) {
                                if (w->type == WINDOW_TYPE_BROWSER) browser_free_page(w);
                                w->active = 0;
                                handled = 1;
                                break;
                            } else if (mx >= w->x && mx <= w->x + w->w && my >= w->y && my <= w->y + w->h) {
                                w->minimized = 0;
                                g_active_win = i;
                                /* İçeriğe tıklamak yalnızca odağı değiştirir.
                                   Sürükleme, aktif pencerenin başlık çubuğundan başlar. */
                                handled = 1;
                                break;
                            }
                        }
                    }
                }

                /* Masaüstü İkonları Tıklaması (Sol Kenar Dikey İkonlar) */
                if (!handled && mx >= 10 && mx <= 75) {
                    if (my >= 20 && my <= 70) desktop_open_program(WINDOW_TYPE_TERMINAL);
                    else if (my >= 90 && my <= 140) desktop_open_program(WINDOW_TYPE_FILES);
                    else if (my >= 160 && my <= 210) desktop_open_program(WINDOW_TYPE_NOTES);
                    else if (my >= 230 && my <= 280) desktop_open_program(WINDOW_TYPE_INFO);
                    else if (my >= 300 && my <= 350) desktop_open_program(WINDOW_TYPE_CALC);
                    else if (my >= 370 && my <= 420) desktop_open_program(WINDOW_TYPE_BROWSER);
                    else if (my >= 440 && my <= 490) {
                        video_clear(0); cursor_x = 5; cursor_y = 30;
                        shell_print("Masaustunden cikildi.\n", 10);
                        return 0;
                    }
                }
            }
        }

        /* 3. GÖRSEL ÇİZİM (SIFIR TİTREME / ZERO FLICKER) */
        if (needs_redraw) {
            video_restore_cursor(prev_mx, prev_my);

            /* Arka Plan */
            video_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - 26, 1);
            video_print("cofeuOS Desktop Edition", SCREEN_WIDTH - 230, 10, 3);

            /* İkonlar */
            video_fill_rect(15, 20, 56, 44, 0); video_draw_rect(15, 20, 56, 44, 15); video_print(">_", 34, 32, 10); video_print("Terminal", 11, 68, 15);
            video_fill_rect(15, 90, 56, 44, 3); video_draw_rect(15, 90, 56, 44, 15); video_print("DIR", 30, 102, 14); video_print("Dosyalar", 11, 138, 15);
            video_fill_rect(15, 160, 56, 44, 5); video_draw_rect(15, 160, 56, 44, 15); video_print("TXT", 30, 172, 15); video_print("Notlar", 18, 208, 15);
            video_fill_rect(15, 230, 56, 44, 6); video_draw_rect(15, 230, 56, 44, 15); video_print("INFO", 26, 242, 15); video_print("Sistem", 18, 278, 15);
            video_fill_rect(15, 300, 56, 44, 2); video_draw_rect(15, 300, 56, 44, 15); video_print("123", 30, 312, 15); video_print("Hesap", 20, 348, 15);
            video_fill_rect(15, 370, 56, 44, 4); video_draw_rect(15, 370, 56, 44, 15); video_print("WWW", 27, 382, 15); video_print("Tarayici", 14, 418, 15);
            video_fill_rect(15, 440, 56, 44, 4); video_draw_rect(15, 440, 56, 44, 15); video_print("EXIT", 26, 452, 15); video_print("Cikis", 22, 488, 15);

            /* Pencereler */
            for (int i = 0; i < g_window_count; i++) {
                if (i != g_active_win && g_windows[i].active) {
                    desktop_draw_single_window(&g_windows[i], 0);
                }
            }
            if (g_active_win >= 0 && g_active_win < g_window_count && g_windows[g_active_win].active) {
                desktop_draw_single_window(&g_windows[g_active_win], 1);
            }

            /* Alt Görev Çubuğu */
            video_fill_rect(0, SCREEN_HEIGHT - 26, SCREEN_WIDTH, 26, 8);
            video_draw_rect(0, SCREEN_HEIGHT - 26, SCREEN_WIDTH, 26, 7);
            video_fill_rect(4, SCREEN_HEIGHT - 23, 75, 20, start_menu_open ? 9 : 3);
            video_draw_rect(4, SCREEN_HEIGHT - 23, 75, 20, 15);
            video_print("cofeuOS", 10, SCREEN_HEIGHT - 19, 15);

            int tab_x = 90;
            for (int i = 0; i < g_window_count; i++) {
                if (g_windows[i].active) {
                    int is_act = (i == g_active_win);
                    int is_min = g_windows[i].minimized;
                    video_fill_rect(tab_x, SCREEN_HEIGHT - 23, 110, 20, is_act ? 9 : (is_min ? 8 : 7));
                    video_draw_rect(tab_x, SCREEN_HEIGHT - 23, 110, 20, 15);
                    video_print(g_windows[i].title, tab_x + 6, SCREEN_HEIGHT - 19,
                                is_act ? 15 : (is_min ? 8 : 0));
                    if (is_min) {
                        video_print("[-]", tab_x + 92, SCREEN_HEIGHT - 19, 12);
                    }
                    tab_x += 115;
                }
            }
            desktop_draw_clock(clock_sec);

            /* Başlat Menüsü Pop-up */
            if (start_menu_open) {
                video_fill_rect(4, SCREEN_HEIGHT - 195, 140, 168, 8);
                video_draw_rect(4, SCREEN_HEIGHT - 195, 140, 168, 15);
                video_print("  Uygulamalar", 12, SCREEN_HEIGHT - 187, 14);
                video_fill_rect(8, SCREEN_HEIGHT - 171, 132, 1, 7);
                video_print("[>] Terminal", 12, SCREEN_HEIGHT - 165, 15);
                video_print("[>] Dosyalar", 12, SCREEN_HEIGHT - 145, 15);
                video_print("[>] Notlar",   12, SCREEN_HEIGHT - 125, 15);
                video_print("[>] Sistem",   12, SCREEN_HEIGHT - 105, 15);
                video_print("[>] Hesap",    12, SCREEN_HEIGHT - 85,  15);
                video_print("[>] Tarayici", 12, SCREEN_HEIGHT - 65,  15);
                video_print("[X] Cikis",    12, SCREEN_HEIGHT - 45,  12);
            }

            needs_redraw = 0;

            video_draw_cursor(mx, my);
            prev_mx = mx;
            prev_my = my;
        } else if (moved || mx != prev_mx || my != prev_my) {
            /* Sadece İmleç Hareketi: Tampondan Eski İmleci Temizle ve Yenisini Çiz */
            video_restore_cursor(prev_mx, prev_my);
            video_draw_cursor(mx, my);
            prev_mx = mx;
            prev_my = my;
        }

        /* Döngüyü 1ms adımla; masaüstü saati bu sayaçtan beslenir */
        pit_delay_ms(1);
        if (js_tick(1) > 0 && g_active_win >= 0 &&
            g_active_win < g_window_count) {
            desktop_window_t *bw = &g_windows[g_active_win];
            if (bw->active && bw->type == WINDOW_TYPE_BROWSER) {
                if (!browser_handle_js_navigation(bw)) browser_relayout_page(bw);
                needs_redraw = 1;
            }
        }
        clock_ms += 1;
        if (clock_ms >= 1000) {
            clock_ms -= 1000;
            clock_sec++;
            needs_redraw = 1;
        }
    }
}

static int cmd_date(int argc, char** argv)   { shell_print("Thu Jan 1 00:00:00 2025", 14); shell_newline(); return 0; }
static int cmd_uptime(int argc, char** argv) { shell_print("uptime: 0 gun", 14); shell_newline(); return 0; }
static int cmd_free(int argc, char** argv)   { shell_print("Toplam: 16MB", 14); shell_newline(); return 0; }
static int cmd_ps(int argc, char** argv)     { shell_print("PID 1 shell\nPID 2 cofeuDE", 15); shell_newline(); return 0; }
static int cmd_df(int argc, char** argv)     { shell_print("/dev/sda1 16MB %6 kullanildi", 15); shell_newline(); return 0; }

static int cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        shell_print(argv[i], 15);
        if (i < argc - 1) shell_print(" ", 15);
    }
    shell_newline();
    return 0;
}

static int cmd_env(int argc, char** argv) {
    shell_print("USER=", 15); shell_print(g_shell.user, 15);
    shell_print(" HOST=cofeu PATH=/bin SHELL=/bin/cofeush", 15);
    shell_newline();
    return 0;
}static int cmd_sudo(int argc, char** argv) { return cmd_rodo(argc, argv); }
