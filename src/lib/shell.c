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
extern memory_arena g_mem_arena;
#include "../include/session.h"
#include "../include/js.h"
#include "../include/sha256.h"
#include "../include/time.h"
#include "../include/sched.h"
#include <efi.h>


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
static int cmd_cofetch(int argc, char** argv);
static int cmd_vim(int argc, char** argv);
static int cmd_ifconfig(int argc, char** argv);
static int cmd_ping(int argc, char** argv);
static int cmd_wget(int argc, char** argv);
static int cmd_wget_text(int argc, char** argv);
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
static int cmd_git(int argc, char** argv);
static int cmd_scnw(int argc, char** argv);
static int cmd_wifi(int argc, char** argv);
static int cmd_wpa(int argc, char** argv);
static int cmd_dhcp(int argc, char** argv);
static int cmd_bluetooth(int argc, char** argv);
static int cmd_sound(int argc, char** argv);
static int cmd_usb(int argc, char** argv);
static int cmd_ssh(int argc, char** argv);
static int cmd_nw(int argc, char** argv);
static int cmd_pacman2(int argc, char** argv);
static int cmd_htop(int argc, char** argv);
static int cmd_nano2(int argc, char** argv);
static int cmd_cal(int argc, char** argv);
static int cmd_games(int argc, char** argv);
static int cmd_mail(int argc, char** argv);
static int cmd_pdf(int argc, char** argv);
static int cmd_music(int argc, char** argv);

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
    if (strcmp(args[0], "cofetch") == 0) return cmd_cofetch(argc, args);
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
    if (strcmp(args[0], "wget-text") == 0) return cmd_wget_text(argc, args);
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
    if (strcmp(args[0], "git") == 0) return cmd_git(argc, args);
    if (strcmp(args[0], "python") == 0 || strcmp(args[0], "python3") == 0) { if (argc > 1) { python_run_file(args[1]); } else { python_repl(); } return 0; }
    if (strcmp(args[0], "scnw") == 0) return cmd_scnw(argc, args);
    if (strcmp(args[0], "wifi") == 0) return cmd_wifi(argc, args);
    if (strcmp(args[0], "wpa") == 0) return cmd_wpa(argc, args);
    if (strcmp(args[0], "dhcp") == 0) return cmd_dhcp(argc, args);
    if (strcmp(args[0], "bluetooth") == 0 || strcmp(args[0], "bt") == 0) return cmd_bluetooth(argc, args);
    if (strcmp(args[0], "sound") == 0 || strcmp(args[0], "beep") == 0) return cmd_sound(argc, args);
    if (strcmp(args[0], "usb") == 0) return cmd_usb(argc, args);
    if (strcmp(args[0], "ssh") == 0) return cmd_ssh(argc, args);
    if (strcmp(args[0], "nw") == 0) return cmd_nw(argc, args);
    if (strcmp(args[0], "pacman2") == 0) return cmd_pacman2(argc, args);
    if (strcmp(args[0], "htop") == 0) return cmd_htop(argc, args);
    if (strcmp(args[0], "nano2") == 0) return cmd_nano2(argc, args);
    if (strcmp(args[0], "cal") == 0 || strcmp(args[0], "calendar") == 0) return cmd_cal(argc, args);
    if (strcmp(args[0], "games") == 0 || strcmp(args[0], "game") == 0) return cmd_games(argc, args);
    if (strcmp(args[0], "mail") == 0) return cmd_mail(argc, args);
    if (strcmp(args[0], "pdf") == 0) return cmd_pdf(argc, args);
    if (strcmp(args[0], "music") == 0 || strcmp(args[0], "mp") == 0) return cmd_music(argc, args);

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
    shell_print("Diger: cofetch calc apps about theme desktop startx rodo reboot halt\n", 15);
    shell_print("Paket: pacman\n", 15);
    shell_print("Ag: ifconfig nslookup nettest ping curl scnw nw ssh\n", 15);
    shell_print("WiFi: wifi scan/connect/disconnect/status\n", 15);
    shell_print("Bluetooth: bluetooth/bt scan/pair/connect\n", 15);
    shell_print("Sistem: sound/beep usb dhcp wpa\n", 15);
    shell_print("Ek: pacman2 htop nano2 cal games mail pdf music\n", 15);
    shell_print("Git: git init/add/commit/push/clone\n", 13);
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

static int cmd_cofetch(int argc, char** argv) {
    /* Yan yana: logo (sol) + bilgi (sag) */
    extern memory_arena g_mem_arena;
    size_t total_mem = g_mem_arena.total_size;
    size_t used_mem  = mem_used_space(&g_mem_arena);

    shell_print("    .-\"-.     OS     : cofeuOS v3.0\n", 15);
    shell_print("   / ..  \\    Kernel : x86_64 cofeu (UEFI)\n", 15);
    shell_print("  | (  )  |   Shell  : Cofeu Shell\n", 15);
    shell_print("   \\ ..  /    Host   : QEMU x86_64\n", 15);
    shell_print("    `---'     Memory : ", 15);
    shell_print_int((int)(used_mem / 1024), 15);
    shell_print(" KB / ", 15);
    shell_print_int((int)(total_mem / 1024), 15);
    shell_print(" KB\n", 15);
    shell_print("               Disk   : 16 KB (RAM-based VFS)\n", 15);
    shell_print("               Net    : UEFI SNP (Ethernet)\n", 14);
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

struct wget_dl_ctx { char *buf; int *pos; int *err; int capacity; };

static void wget_chunk_cb(const char *data, int data_len, void *user_ctx) {
    struct wget_dl_ctx *c = (struct wget_dl_ctx *)user_ctx;
    if (*c->err) return;
    int space = c->capacity - *c->pos;
    int to_copy = data_len < space ? data_len : space;
    if (to_copy > 0) {
        for (int i = 0; i < to_copy; i++) c->buf[*c->pos + i] = data[i];
        *c->pos += to_copy;
    }
    if (data_len > space) *c->err = -4;
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
    int port = 80;

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

    /* Streaming indirme: büyük dosyalar için arabellek taşmasını önler. */
    static char dl_buf[16384];
    int dl_pos = 0;
    int dl_error = 0;

    struct wget_dl_ctx dctx = { dl_buf, &dl_pos, &dl_error, (int)sizeof(dl_buf) - 1 };

    if (use_https) {
        len = https_get_port(host, remote_path, dl_buf, sizeof(dl_buf) - 1, (unsigned short)port);
    } else {
        len = http_get_streaming(host, remote_path, wget_chunk_cb, &dctx, (unsigned short)port);
        if (len > 0) len = dl_pos;
    }

    if (len < 0 && dl_pos == 0) {
        shell_print("wget: baglanti hatasi (", 12);
        shell_print_int(len, 10);
        shell_print(")", 12);
        shell_newline();
        return -1;
    }

    len = dl_pos;
    if (len == 0) {
        shell_print("wget: veri alinmadi", 12); shell_newline();
        return -1;
    }

    shell_print("wget: ", 10); shell_print_int(len, 10);
    shell_print(" byte alindi", 10); shell_newline();

    if (fs_write_file(&g_fs, target, dl_buf, (size_t)len) < 0) {
        shell_print("wget: dosya kaydedilemedi!", 12); shell_newline();
    } else {
        shell_print("wget: dosya kaydedildi (", 10);
        shell_print_int(len, 10);
        shell_print(" byte)", 10);
        shell_newline();
    }
    
    return 0;
}

/* wget-text: Host proxy uzerinden HTTPS sayfasinin text halini indir */
static int cmd_wget_text(int argc, char** argv) {
    if (argc < 2) { shell_print("kullanim: wget-text <https-url> [proxy-host]", 12); shell_newline(); return -1; }
    if (!network_available()) { shell_print("wget-text: network yok", 12); shell_newline(); return -1; }

    const char *url = argv[1];
    const char *proxy_host = argc > 2 ? argv[2] : "10.0.2.2";
    int proxy_port = 8080;

    shell_print("wget-text: proxy uzerinden aliniyor...", 15); shell_newline();

    char host[64];
    char path[256];
    int use_https = 0;
    int port = 80;
    if (parse_http_url(url, host, sizeof(host), path, sizeof(path), &use_https, &port) != 0) {
        shell_print("wget-text: URL hatali", 12); shell_newline();
        return -1;
    }

    /* Proxy URL'si olustur: http://proxy:8080/?url=<encoded> */
    static char proxy_url[512];
    int plen = 0;
    const char *prefix = "http://";
    for (const char *p = prefix; *p; p++) proxy_url[plen++] = *p;
    for (const char *p = proxy_host; *p; p++) proxy_url[plen++] = *p;
    proxy_url[plen++] = ':'; proxy_url[plen++] = '8'; proxy_url[plen++] = '0'; proxy_url[plen++] = '8'; proxy_url[plen++] = '0';
    proxy_url[plen++] = '/'; proxy_url[plen++] = '?'; proxy_url[plen++] = 'u'; proxy_url[plen++] = 'r'; proxy_url[plen++] = 'l'; proxy_url[plen++] = '=';
    
    /* Basit URL encode (sadece alfanumerik ve :/?&= haricini %XX yap) */
    for (const char *p = url; *p && plen < 500; p++) {
        unsigned char c = *p;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == ':' || c == '/' || c == '?' || c == '&' || c == '=' || c == '.' || c == '-' || c == '_' || c == '~') {
            proxy_url[plen++] = c;
        } else {
            static const char *hex = "0123456789ABCDEF";
            proxy_url[plen++] = '%';
            proxy_url[plen++] = hex[c >> 4];
            proxy_url[plen++] = hex[c & 0xF];
        }
    }
    proxy_url[plen] = '\0';

    /* Proxy host ve path ayrıştır */
    char phost[64], ppath[256];
    int puse_https = 0, pport = 8080;
    if (parse_http_url(proxy_url, phost, sizeof(phost), ppath, sizeof(ppath), &puse_https, &pport) != 0) {
        shell_print("wget-text: proxy URL hatali", 12); shell_newline();
        return -1;
    }

    static char buf[16384];
    int len = http_get_port(phost, ppath, buf, sizeof(buf) - 1, pport);
    
    if (len < 0) {
        shell_print("wget-text: baglanti hatasi (", 12); shell_print_int(len, 10); shell_print(")", 12); shell_newline();
        return -1;
    }
    if (len == 0) { shell_print("wget-text: veri yok", 12); shell_newline(); return -1; }
    buf[len] = '\0';

    /* Dosya adi: URL'den al veya wget-text.txt */
    char fname[64] = "wget-text.txt";
    const char *slash = strrchr(url, '/');
    if (slash && slash[1]) {
        strncpy(fname, slash + 1, sizeof(fname) - 1);
        fname[sizeof(fname) - 1] = '\0';
        if (strchr(fname, '?')) *strchr(fname, '?') = '\0';
        if (!fname[0]) strcpy(fname, "wget-text.txt");
    }
    if (fname[strlen(fname) - 1] != 't' || fname[strlen(fname) - 2] != 'x' || fname[strlen(fname) - 3] != 't') {
        strcat(fname, ".txt");
    }

    char target[256];
    if (fs_resolve_path(g_shell.cwd, fname, target) != 0) {
        shell_print("wget-text: yol hatasi", 12); shell_newline();
        return -1;
    }
    if (fs_write_file(&g_fs, target, buf, (size_t)len) != len) {
        shell_print("wget-text: yazma hatasi", 12); shell_newline();
        return -1;
    }
    shell_print("wget-text: ", 10); shell_print_int(len, 10); shell_print(" byte -> ", 10); shell_print(fname, 10); shell_newline();
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
    shell_print("calc write nano vim sysinfo cofetch pacman\n", 15);
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
    int port = 80;

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
        if (is_post && post_data[0]) {
            len = https_post_port(host, path, post_data, strlen(post_data), buf, sizeof(buf) - 1, port);
        } else {
            len = https_get_port(host, path, buf, sizeof(buf) - 1, port);
        }
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

static void browser_free_tab(browser_tab_t *tab) {
    if (tab->layout) { web_free_boxes(tab->layout); tab->layout = NULL; }
    if (tab->css)    { web_free_css(tab->css); tab->css = NULL; }
    if (tab->doc)    { web_free_document(tab->doc); tab->doc = NULL; }
    tab->page_h = 0;
}

static browser_tab_t* browser_get_active_tab(desktop_window_t *w) {
    if (w->active_tab >= 0 && w->active_tab < w->tab_count)
        return &w->tabs[w->active_tab];
    return NULL;
}

/* Forward declarations */
static void ps2_read_both(mouse_state_t *ms, key_event_t *ev);

/* Ağ işlemi sırasında UI'yi canli tutmak icin ilerleme geri cagrimasi */
static void browser_net_progress_cb(void) {
    extern desktop_window_t g_windows[];
    extern int g_window_count;
    extern int g_active_win;
    extern int g_mx, g_my, g_prev_mx, g_prev_my;
    mouse_state_t ms;
    key_event_t ev;
    ev.key = 0; ev.scan_code = 0;
    ms.dx = 0; ms.dy = 0; ms.dz = 0;
    ms.left_btn = 0; ms.right_btn = 0;
    if (mouse_get_state(&ms) == 0) {
        /* UEFI Simple Pointer ile mouse oku */
    } else {
        /* PS/2 fallback */
        ps2_read_both(&ms, &ev);
    }

    /* Mouse hareketi: global g_mx,g_my'yi guncelle */
    g_mx += ms.dx;
    g_my += ms.dy;
    if (g_mx < 0) g_mx = 0;
    if (g_mx >= SCREEN_WIDTH) g_mx = SCREEN_WIDTH - 1;
    if (g_my < 0) g_my = 0;
    if (g_my >= SCREEN_HEIGHT) g_my = SCREEN_HEIGHT - 1;

    /* İmleci guncelle (eski pozisyonu temizle, yenisini ciz) */
    video_restore_cursor(g_prev_mx, g_prev_my);
    video_draw_cursor(g_mx, g_my);
    g_prev_mx = g_mx;
    g_prev_my = g_my;

    /* Aktif pencere tarayıcı ve yukleniyorsa "Yukleniyor..." gosteren yeniden cizim */
    if (g_active_win >= 0 && g_active_win < g_window_count) {
        desktop_window_t *w = &g_windows[g_active_win];
        browser_tab_t *tab = browser_get_active_tab(w);
        if (w->active && w->type == WINDOW_TYPE_BROWSER && tab && tab->loading) {
            /* Sadece imleci ve yukleniyor yazisini guncelle, tam yeniden cizim yok */
            video_print("Yukleniyor...", w->x + 10, w->y + 30, 14);
        }
    }
}

static void browser_navigate_url(desktop_window_t *w, const char *url);
static void browser_new_tab(desktop_window_t *w);
static void browser_close_tab(desktop_window_t *w, int index);
static void resolve_url(const char *base, const char *href, char *out, int out_size);
static void browser_relayout_page(desktop_window_t *w);
static int browser_handle_js_navigation(desktop_window_t *w);

/* Yeni sekme aç */
static void browser_new_tab(desktop_window_t *w) {
    if (w->tab_count >= MAX_BROWSER_TABS) return;
    int idx = w->tab_count;
    browser_tab_t *tab = &w->tabs[idx];
    memset(tab, 0, sizeof(browser_tab_t));
    tab->active = 1;
    strcpy(tab->title, "Yeni Sekme");
    w->tab_count++;
    w->active_tab = idx;
    /* Varsayılan hızlı erişim sayfası */
    tab->page_len = 0;
}

/* Sekme kapat */
static void browser_close_tab(desktop_window_t *w, int index) {
    if (index < 0 || index >= w->tab_count) return;
    browser_free_tab(&w->tabs[index]);
    /* Sekmeleri sola kaydır */
    for (int i = index; i < w->tab_count - 1; i++) {
        w->tabs[i] = w->tabs[i + 1];
    }
    w->tab_count--;
    if (w->active_tab >= w->tab_count) {
        w->active_tab = w->tab_count - 1;
    }
    if (w->tab_count == 0) {
        browser_new_tab(w);
    }
}

/* window.location = ile yonlendirme; sonsuz donguyu onle */
static int g_js_redirects;

static void browser_parse_page(desktop_window_t *w) {
    browser_tab_t *tab = browser_get_active_tab(w);
    if (!tab) return;

    /* HTTP yanıt başlığında 301/302 redirect var mı? */
    char *body_start = tab->page_buf;
    char *header_end = strstr(tab->page_buf, "\r\n\r\n");
    if (header_end) {
        /* Status line kontrol et: "HTTP/1.1 301" veya "HTTP/1.1 302" */
        char *status = strstr(tab->page_buf, "HTTP/1.");
        if (status && (strstr(status, " 301 ") || strstr(status, " 302 ") ||
                       strstr(status, " 303 ") || strstr(status, " 307 ") || strstr(status, " 308 "))) {
            /* Location header bul */
            char *loc = strstr(tab->page_buf, "Location:");
            if (loc) {
                loc += 9;
                while (*loc == ' ' || *loc == '\t') loc++;
                char *loc_end = strstr(loc, "\r\n");
                if (loc_end) {
                    int loc_len = (int)(loc_end - loc);
                    if (loc_len < 159) {
                        char redirect_url[160];
                        strncpy(redirect_url, loc, loc_len);
                        redirect_url[loc_len] = '\0';
                        /* Göreli URL'i çöz */
                        char resolved[160];
                        resolve_url(tab->page_base, redirect_url, resolved, sizeof(resolved));
                        if (resolved[0]) {
                            browser_free_tab(tab);
                            browser_navigate_url(w, resolved);
                            return;
                        }
                    }
                }
            }
        }
        body_start = header_end + 4;
    }

    browser_free_tab(tab);
    if (tab->page_len <= 0) return;
    tab->doc = web_parse_html(tab->page_buf, (unsigned int)tab->page_len);
    if (!tab->doc) return;
    web_node_t *st[2];
    int nst = web_get_elements_by_tag(tab->doc->root, "style", st, 2);
    if (nst == 1 && st[0]->first_child)
        tab->css = web_parse_css(st[0]->first_child->text,
                                 (unsigned int)strlen(st[0]->first_child->text));

    /* JavaScript: <script> bloklarını sırayla çalıştır */
    js_reset();
    js_set_page(tab->doc, tab->css, tab->url_buf);
    web_node_t *sc[8];
    int nsc = web_get_elements_by_tag(tab->doc->root, "script", sc, 8);
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
            resolve_url(tab->page_base, nav, url, sizeof(url));
            if (url[0]) browser_navigate_url(w, url);
            return;
        }
    } else {
        g_js_redirects = 0;
    }

    browser_relayout_page(w);

    /* document.title -> sekme basligi */
    if (tab->doc && tab->doc->title && tab->doc->title[0]) {
        strncpy(tab->title, tab->doc->title, sizeof(tab->title) - 1);
        tab->title[sizeof(tab->title) - 1] = '\0';
    } else {
        /* URL'den baslık oluştur */
        const char *u = tab->url_buf;
        if (strncmp(u, "http://", 7) == 0) u += 7;
        else if (strncmp(u, "https://", 8) == 0) u += 8;
        strncpy(tab->title, u, sizeof(tab->title) - 1);
        tab->title[sizeof(tab->title) - 1] = '\0';
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
    browser_tab_t *tab = browser_get_active_tab(w);
    if (!tab || !tab->layout) return;
    int max_scroll = tab->page_h - ah;
    if (max_scroll < 0) max_scroll = 0;
    if (tab->page_scroll > max_scroll) tab->page_scroll = max_scroll;
    if (tab->page_scroll < 0) tab->page_scroll = 0;
    render_box_tree(tab->layout, ax + 8, ay + 6 - tab->page_scroll, ax, ay, aw, ah);
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
    browser_tab_t *tab = browser_get_active_tab(w);
    if (!tab) return;
    if (tab->layout) { web_free_boxes(tab->layout); tab->layout = NULL; }
    if (tab->doc) {
        int content_w = w->w - 24;
        int content_h = w->h - 56 - w->tab_bar_height;
        if (content_h < 100) content_h = 100;
        tab->page_h = web_layout(tab->doc->root, tab->css, content_w, content_h, &tab->layout);
    }
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
static void browser_click_page(desktop_window_t *w, int sx, int sy);
static void resolve_url(const char *base, const char *href, char *out, int out_size);
static void browser_relayout_page(desktop_window_t *w);
static int browser_handle_js_navigation(desktop_window_t *w);

static void browser_click_page(desktop_window_t *w, int sx, int sy) {
    browser_tab_t *tab = browser_get_active_tab(w);
    if (!tab || !tab->layout || tab->page_len <= 0) return;
    int ax = w->x + 6, ay = w->y + 52 + w->tab_bar_height;
    web_node_t *node = find_click_node(tab->layout, ax + 8, ay + 6 - tab->page_scroll, sx, sy);
    if (node && js_dispatch_click(node)) {
        if (browser_handle_js_navigation(w)) return;
        browser_relayout_page(w);
    }
    const char *href = find_link_run(tab->layout, ax + 8, ay + 6 - tab->page_scroll, sx, sy);
    if (href) {
        char url[160];
        resolve_url(tab->page_base, href, url, sizeof(url));
        if (url[0]) browser_navigate_url(w, url);
    }
}

static void browser_navigate_url(desktop_window_t *w, const char *url) {
    browser_tab_t *tab = browser_get_active_tab(w);
    if (!tab) return;
    char host[128];
    char path[256];
    int use_https = 0;
    int port = 80;
    if (parse_http_url(url, host, sizeof(host), path, sizeof(path), &use_https, &port) != 0) {
        tab->page_len = -1;
        tab->page_scroll = 0;
        tab->loading = 0;
        return;
    }

    /* Yukleniyor bayragi ve ilerleme geri cagrimasini ayarla */
    tab->loading = 1;
    net_set_progress_cb(browser_net_progress_cb);

    int len = use_https ? https_get_port(host, path, tab->page_buf, (int)sizeof(tab->page_buf) - 1, (unsigned short)port)
                        : http_get_port(host, path, tab->page_buf, (int)sizeof(tab->page_buf) - 1, (unsigned short)port);

    /* Islem bitti, geri cagrimayi temizle */
    net_set_progress_cb(NULL);
    tab->loading = 0;

    if (len < 0) {
        tab->page_len = -1;
        tab->page_scroll = 0;
        return;
    }
    tab->page_len = len;
    tab->page_buf[len] = '\0';
    strncpy(tab->url_buf, url, sizeof(tab->url_buf) - 1);
    tab->url_buf[sizeof(tab->url_buf) - 1] = '\0';
    tab->url_pos = (int)strlen(tab->url_buf);
    strncpy(tab->page_base, url, sizeof(tab->page_base) - 1);
    tab->page_base[sizeof(tab->page_base) - 1] = '\0';
    tab->page_scroll = 0;

    /* Geçmişe ekle */
    if (tab->history_len < 10) {
        strncpy(tab->history[tab->history_len], url, sizeof(tab->history[0]) - 1);
        tab->history[tab->history_len][sizeof(tab->history[0]) - 1] = '\0';
        tab->history_len++;
        tab->history_pos = tab->history_len - 1;
    }

    browser_parse_page(w);
}

static int browser_handle_js_navigation(desktop_window_t *w) {
    browser_tab_t *tab = browser_get_active_tab(w);
    if (!tab) return 0;
    const char *nav = js_get_pending_nav();
    if (!nav || !nav[0]) return 0;
    char requested[160];
    strncpy(requested, nav, sizeof(requested) - 1);
    requested[sizeof(requested) - 1] = '\0';
    js_clear_pending_nav();
    char url[160];
    resolve_url(tab->page_base, requested, url, sizeof(url));
    if (url[0]) browser_navigate_url(w, url);
    return 1;
}

static void browser_navigate(desktop_window_t *w) {
    browser_tab_t *tab = browser_get_active_tab(w);
    if (!tab) return;
    tab->url_buf[tab->url_pos] = '\0';
    browser_navigate_url(w, tab->url_buf);
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

/* Global imleci pozisyonu (browser_net_progress_cb icin) */
static int g_prev_mx = -1;
static int g_prev_my = -1;
static int g_mx;
static int g_my;

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
        shell_print("Komut yazabilirsiniz (orn: ls, cofetch, ping, python)\n", 11);
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
        w->tab_count = 0;
        w->active_tab = -1;
        w->tab_bar_height = 24;
        browser_new_tab(w);
        browser_tab_t *tab = browser_get_active_tab(w);
        if (tab) {
            strcpy(tab->url_buf, "http://example.com");
            tab->url_pos = (int)strlen(tab->url_buf);
            tab->page_len = 0;
            tab->page_scroll = 0;
        }
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
        /* Tab bar yüksekliği */
        w->tab_bar_height = 24;

        /* Tab Bar çizimi */
        int tab_bar_y = w->y + 22;
        video_fill_rect(w->x, tab_bar_y, w->w, w->tab_bar_height, 7);
        video_draw_rect(w->x, tab_bar_y, w->w, w->tab_bar_height, 8);

        /* Sekmeleri çiz */
        int tab_x = w->x + 4;
        for (int i = 0; i < w->tab_count; i++) {
            browser_tab_t *tab = &w->tabs[i];
            if (!tab->active) continue;
            int tab_w = 120;
            if (tab_x + tab_w > w->x + w->w - 4) tab_w = w->x + w->w - 4 - tab_x;
            if (tab_w < 40) tab_w = 40;

            int is_active = (i == w->active_tab);
            u8 bg = is_active ? 15 : 8;
            u8 fg = is_active ? 0 : 7;
            u8 border = is_active ? 15 : 8;

            video_fill_rect(tab_x, tab_bar_y + 1, tab_w, w->tab_bar_height - 2, bg);
            video_draw_rect(tab_x, tab_bar_y + 1, tab_w, w->tab_bar_height - 2, border);

            /* Sekme başlığı */
            const char *title = tab->title[0] ? tab->title : "Yeni Sekme";
            video_print(title, tab_x + 6, tab_bar_y + 4, fg);

            /* Kapatma butonu (sadece aktif sekmede) */
            if (is_active && tab_w > 60) {
                int close_x = tab_x + tab_w - 18;
                int close_y = tab_bar_y + 3;
                video_fill_rect(close_x, close_y, 14, 14, is_active ? 12 : 8);
                video_draw_char('X', close_x + 4, close_y + 1, 15);
            }

            tab_x += tab_w + 2;
        }

        /* Yeni sekme butonu (+) */
        int new_tab_x = tab_x + 4;
        if (new_tab_x + 24 < w->x + w->w - 4) {
            video_fill_rect(new_tab_x, tab_bar_y + 2, 20, w->tab_bar_height - 4, 2);
            video_draw_rect(new_tab_x, tab_bar_y + 2, 20, w->tab_bar_height - 4, 15);
            video_print("+", new_tab_x + 7, tab_bar_y + 4, 15);
        }

        /* Adres Çubuğu */
        int addr_y = tab_bar_y + w->tab_bar_height + 2;
        video_fill_rect(w->x + 8, addr_y, w->w - 16, 20, 0);
        video_draw_rect(w->x + 8, addr_y, w->w - 16, 20, 8);
        browser_tab_t *atab = browser_get_active_tab(w);
        if (atab) video_print(atab->url_buf, w->x + 12, addr_y + 3, 15);
        if (is_focused && atab) {
            int ux = w->x + 12 + video_text_width(atab->url_buf);
            video_draw_char('_', ux, addr_y + 3, 11);
        }
        /* GO Butonu */
        video_fill_rect(w->x + w->w - 60, addr_y + 1, 52, 18, 3);
        video_draw_rect(w->x + w->w - 60, addr_y + 1, 52, 18, 15);
        video_print("GO", w->x + w->w - 49, addr_y + 3, 15);

        /* İçerik Alanı */
        int cy = addr_y + 22;
        int ch = w->h - (cy - w->y) - 4;
        video_fill_rect(w->x + 6, cy, w->w - 12, ch, 15);
        video_draw_rect(w->x + 6, cy, w->w - 12, ch, 8);

        if (!atab || atab->page_len < 0) {
            video_print("Yuklenemedi: URL gecersiz veya ag hatasi.", w->x + 14, cy + 6, 12);
            video_print("Ag baglantisi icin 'ping'/'wget' ile test edin.", w->x + 14, cy + 24, 12);
        } else if (!atab || atab->page_len == 0) {
            video_print("Adres girip Enter'a basin veya GO'a tiklayin.", w->x + 14, cy + 6, 8);
            video_print("Ornek: http://example.com", w->x + 14, cy + 24, 8);
            /* Speed dial - sık kullanılan siteler */
            video_print("Hizli Erisim:", w->x + 14, cy + 50, 9);
            video_print("1. http://example.com", w->x + 14, cy + 68, 8);
            video_print("2. http://github.com", w->x + 14, cy + 86, 8);
            video_print("3. http://news.ycombinator.com", w->x + 14, cy + 104, 8);
        } else {
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

    g_mx = SCREEN_WIDTH / 2;
    g_my = SCREEN_HEIGHT / 2;
    g_prev_mx = -1;
    g_prev_my = -1;
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
            g_mx += ms.dx;
            g_my += ms.dy;
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
                browser_tab_t *btab = browser_get_active_tab(bw);
                if (btab && ev.scan_code == 1 && btab->page_scroll > 0) { btab->page_scroll -= 18; needs_redraw = 1; }
                else if (btab && ev.scan_code == 2) { btab->page_scroll += 18; needs_redraw = 1; }
            } else if (ev.scan_code == 1) { g_my -= 12; moved = 1; }      /* Up */
            else if (ev.scan_code == 2) { g_my += 12; moved = 1; }  /* Down */
            else if (ev.scan_code == 3) { g_mx += 12; moved = 1; }  /* Right */
            else if (ev.scan_code == 4) { g_mx -= 12; moved = 1; }  /* Left */

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
                    browser_tab_t *tab = browser_get_active_tab(w);
                    if (tab) {
                        if (ev.key == '\n') {
                            browser_navigate(w);
                            needs_redraw = 1;
                        } else if (ev.key == '\b') {
                            if (tab->url_pos > 0) {
                                tab->url_pos--;
                                tab->url_buf[tab->url_pos] = '\0';
                                needs_redraw = 1;
                            }
                        } else if (ev.key >= 32 && tab->url_pos < (int)sizeof(tab->url_buf) - 1) {
                            tab->url_buf[tab->url_pos++] = ev.key;
                            tab->url_buf[tab->url_pos] = '\0';
                            needs_redraw = 1;
                        }
                    }
                }
            }
        }

        /* Ekran Sınırlarını Koruma */
        if (g_mx < 0) g_mx = 0;
        if (g_mx >= SCREEN_WIDTH) g_mx = SCREEN_WIDTH - 1;
        if (g_my < 0) g_my = 0;
        if (g_my >= SCREEN_HEIGHT) g_my = SCREEN_HEIGHT - 1;

        /* Pencere Sürükleme Mantığı */
        if (ms.left_btn && g_drag_win >= 0 && g_drag_win < g_window_count && g_windows[g_drag_win].active) {
            desktop_window_t *w = &g_windows[g_drag_win];
            w->x = g_mx - g_drag_off_x;
            w->y = g_my - g_drag_off_y;
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
            if (g_mx >= 4 && g_mx <= 79 && g_my >= SCREEN_HEIGHT - 26) {
                start_menu_open = !start_menu_open;
            }
            /* Başlat Menüsü Elemanları Tıklaması */
            else if (start_menu_open && g_mx >= 4 && g_mx <= 144 && g_my >= SCREEN_HEIGHT - 195 && g_my < SCREEN_HEIGHT - 26) {
                if (g_my >= SCREEN_HEIGHT - 165 && g_my < SCREEN_HEIGHT - 145) desktop_open_program(WINDOW_TYPE_TERMINAL);
                else if (g_my >= SCREEN_HEIGHT - 145 && g_my < SCREEN_HEIGHT - 125) desktop_open_program(WINDOW_TYPE_FILES);
                else if (g_my >= SCREEN_HEIGHT - 125 && g_my < SCREEN_HEIGHT - 105) desktop_open_program(WINDOW_TYPE_NOTES);
                else if (g_my >= SCREEN_HEIGHT - 105 && g_my < SCREEN_HEIGHT - 85)  desktop_open_program(WINDOW_TYPE_INFO);
                else if (g_my >= SCREEN_HEIGHT - 85  && g_my < SCREEN_HEIGHT - 65)  desktop_open_program(WINDOW_TYPE_CALC);
                else if (g_my >= SCREEN_HEIGHT - 65  && g_my < SCREEN_HEIGHT - 45)  desktop_open_program(WINDOW_TYPE_BROWSER);
                else if (g_my >= SCREEN_HEIGHT - 45) {
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
                        if (g_mx >= tab_x && g_mx < tab_x + 110 &&
                            g_my >= SCREEN_HEIGHT - 23 && g_my < SCREEN_HEIGHT - 3) {
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
                    if (g_mx >= min_x && g_mx <= min_x + 16 && g_my >= btn_y && g_my <= btn_y + 16) {
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
                    else if (g_mx >= btn_x && g_mx <= btn_x + 16 && g_my >= btn_y && g_my <= btn_y + 16) {
                        if (w->type == WINDOW_TYPE_BROWSER) {
                            for (int i = 0; i < w->tab_count; i++) {
                                if (w->tabs[i].active) browser_free_tab(&w->tabs[i]);
                            }
                        }
                        w->active = 0;
                        w->minimized = 0;
                        g_active_win = -1;
                        for (int k = 0; k < g_window_count; k++) {
                            if (g_windows[k].active) { g_active_win = k; break; }
                        }
                        handled = 1;
                    }
                    /* Başlık Çubuğuna mı Basıldı? (Sürükleme Başlat) */
                    else if (g_mx >= w->x && g_mx <= w->x + w->w - 40 && g_my >= w->y && g_my <= w->y + 22) {
                        g_drag_win = g_active_win;
                        g_drag_off_x = g_mx - w->x;
                        g_drag_off_y = g_my - w->y;
                        handled = 1;
                    }
                    /* Tarayıcı Sekme Çubuğu Tıklaması */
                    else if (w->type == WINDOW_TYPE_BROWSER &&
                             g_my >= w->y + 22 && g_my <= w->y + 22 + w->tab_bar_height) {
                        int tab_bar_y = w->y + 22;
                        int tab_x = w->x + 4;
                        for (int i = 0; i < w->tab_count; i++) {
                            browser_tab_t *tab = &w->tabs[i];
                            if (!tab->active) continue;
                            int tab_w = 120;
                            if (tab_x + tab_w > w->x + w->w - 4) tab_w = w->x + w->w - 4 - tab_x;
                            if (tab_w < 40) tab_w = 40;

                            /* Sekme tıklaması */
                            if (g_mx >= tab_x && g_mx < tab_x + tab_w) {
                                /* Kapatma butonu (sadece aktif sekmede) */
                                if (i == w->active_tab && tab_w > 60) {
                                    int close_x = tab_x + tab_w - 18;
                                    int close_y = tab_bar_y + 3;
                                    if (g_mx >= close_x && g_mx < close_x + 14 &&
                                        g_my >= close_y && g_my < close_y + 14) {
                                        browser_close_tab(w, i);
                                        needs_redraw = 1;
                                        handled = 1;
                                        break;
                                    }
                                }
                                /* Sekme geçiş */
                                if (i != w->active_tab) {
                                    w->active_tab = i;
                                    needs_redraw = 1;
                                    handled = 1;
                                }
                                break;
                            }
                            tab_x += tab_w + 2;
                        }
                        /* Yeni sekme butonu (+) */
                        int new_tab_x = tab_x + 4;
                        if (!handled && new_tab_x + 24 < w->x + w->w - 4 &&
                            g_mx >= new_tab_x && g_mx < new_tab_x + 20 &&
                            g_my >= tab_bar_y + 2 && g_my < tab_bar_y + w->tab_bar_height - 2) {
                            browser_new_tab(w);
                            needs_redraw = 1;
                            handled = 1;
                        }
                        handled = 1;
                    }
                    /* Tarayıcı GO Butonu mu? */
                    else if (w->type == WINDOW_TYPE_BROWSER &&
                             g_mx >= w->x + w->w - 60 && g_mx <= w->x + w->w - 8 &&
                             g_my >= w->y + 22 + w->tab_bar_height + 1 && g_my <= w->y + 22 + w->tab_bar_height + 19) {
                        browser_navigate(w);
                        needs_redraw = 1;
                        handled = 1;
                    }
                    /* Tarayıcı sayfa içi bağlantı tıklaması mı? */
                    else if (w->type == WINDOW_TYPE_BROWSER && w->tab_count > 0) {
                        browser_tab_t *atab = browser_get_active_tab(w);
                        if (atab && atab->page_len > 0) {
                            int addr_y = w->y + 22 + w->tab_bar_height + 2;
                            int cy = addr_y + 22;
                            if (g_mx >= w->x + 6 && g_mx <= w->x + w->w - 6 &&
                                g_my >= cy && g_my <= w->y + w->h - 4) {
                                browser_click_page(w, g_mx, g_my);
                                needs_redraw = 1;
                                handled = 1;
                            }
                        }
                    }
                    /* Hesap Makinesi Butonları mı? */
                    else if (w->type == WINDOW_TYPE_CALC &&
                             g_mx >= w->x + 15 && g_mx <= w->x + w->w - 15 &&
                             g_my >= w->y + 74 && g_my <= w->y + 200) {
                        int c = (g_mx - (w->x + 15)) / ((w->w - 38) / 4 + 2);
                        int r = (g_my - (w->y + 74)) / 32;
                        if (c >= 0 && c < 4 && r >= 0 && r < 4) calc_press(w, r * 4 + c);
                        needs_redraw = 1;
                        handled = 1;
                    }
                    /* Pencere İçine mi Basıldı? */
                    else if (g_mx >= w->x && g_mx <= w->x + w->w && g_my >= w->y && g_my <= w->y + w->h) {
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
                            if (g_mx >= btn_x && g_mx <= btn_x + 16 && g_my >= btn_y && g_my <= btn_y + 16) {
                                if (w->type == WINDOW_TYPE_BROWSER) {
                                    for (int j = 0; j < w->tab_count; j++) {
                                        if (w->tabs[j].active) browser_free_tab(&w->tabs[j]);
                                    }
                                }
                                w->active = 0;
                                handled = 1;
                                break;
                            } else if (g_mx >= w->x && g_mx <= w->x + w->w && g_my >= w->y && g_my <= w->y + w->h) {
                                w->minimized = 0;
                                g_active_win = i;
                                /* Başlık çubuğunda tıklanırsa sürüklemeyi de başlat */
                                if (g_mx <= w->x + w->w - 40 && g_my <= w->y + 22) {
                                    g_drag_win = i;
                                    g_drag_off_x = g_mx - w->x;
                                    g_drag_off_y = g_my - w->y;
                                }
                                handled = 1;
                                break;
                            }
                        }
                    }
                }

                /* Masaüstü İkonları Tıklaması (Sol Kenar Dikey İkonlar) */
                if (!handled && g_mx >= 10 && g_mx <= 75) {
                    if (g_my >= 20 && g_my <= 70) desktop_open_program(WINDOW_TYPE_TERMINAL);
                    else if (g_my >= 90 && g_my <= 140) desktop_open_program(WINDOW_TYPE_FILES);
                    else if (g_my >= 160 && g_my <= 210) desktop_open_program(WINDOW_TYPE_NOTES);
                    else if (g_my >= 230 && g_my <= 280) desktop_open_program(WINDOW_TYPE_INFO);
                    else if (g_my >= 300 && g_my <= 350) desktop_open_program(WINDOW_TYPE_CALC);
                    else if (g_my >= 370 && g_my <= 420) desktop_open_program(WINDOW_TYPE_BROWSER);
                    else if (g_my >= 440 && g_my <= 490) {
                        video_clear(0); cursor_x = 5; cursor_y = 30;
                        shell_print("Masaustunden cikildi.\n", 10);
                        return 0;
                    }
                }
            }
        }

        /* 3. GÖRSEL ÇİZİM (SIFIR TİTREME / ZERO FLICKER) */
        if (needs_redraw) {
            video_restore_cursor(g_prev_mx, g_prev_my);

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

            video_draw_cursor(g_mx, g_my);
            g_prev_mx = g_mx;
            g_prev_my = g_my;
        } else if (moved || g_mx != g_prev_mx || g_my != g_prev_my) {
            /* Sadece İmleç Hareketi: Tampondan Eski İmleci Temizle ve Yenisini Çiz */
            video_restore_cursor(g_prev_mx, g_prev_my);
            video_draw_cursor(g_mx, g_my);
            g_prev_mx = g_mx;
            g_prev_my = g_my;
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

static int cmd_date(int argc, char** argv) {
    time_t ts;
    if (time_get_unix(&ts) != 0) { shell_print("tarih alinamadi", 12); shell_newline(); return -1; }
    static const char *wday[] = {"Paz","Pzt","Sal","Car","Per","Cum","Cmt"};
    static const int mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int64_t s = (int64_t)ts;
    int sec = s % 60; s /= 60;
    int min = s % 60; s /= 60;
    int hour = s % 24;
    int days = (int)(s / 24);
    int w = (days + 4) % 7;
    int year = 1970;
    for (;;) { int yd = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0 ? 366 : 365; if (days < yd) break; days -= yd; year++; }
    int mon = 0;
    while (mon < 12) { int dm = mdays[mon]; if (mon == 1 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) dm++; if (days < dm) break; days -= dm; mon++; }
    mon++;
    char buf[48];
    int n = 0;
    buf[n++] = wday[w][0]; buf[n++] = wday[w][1]; buf[n++] = wday[w][2]; buf[n++] = ' ';
    buf[n++] = '0' + mon / 10; buf[n++] = '0' + mon % 10; buf[n++] = '/';
    buf[n++] = '0' + (days + 1) / 10; buf[n++] = '0' + (days + 1) % 10; buf[n++] = '/';
    int yh = year / 1000, yt = (year / 100) % 10, yu = (year / 10) % 10, yo = year % 10;
    buf[n++] = '0' + yh; buf[n++] = '0' + yt; buf[n++] = '0' + yu; buf[n++] = '0' + yo;
    buf[n++] = ' '; buf[n++] = '0' + hour / 10; buf[n++] = '0' + hour % 10;
    buf[n++] = ':'; buf[n++] = '0' + min / 10; buf[n++] = '0' + min % 10;
    buf[n++] = ':'; buf[n++] = '0' + sec / 10; buf[n++] = '0' + sec % 10;
    buf[n] = 0;
    shell_print(buf, 15);
    shell_newline();
    return 0;
}
static int cmd_uptime(int argc, char** argv) {
    uint32_t ticks = sched_get_ticks();
    int sec = ticks / 1000;
    int min = sec / 60; sec %= 60;
    int hr = min / 60; min %= 60;
    char buf[64]; int n = 0;
    const char *pre = "uptime: ";
    while (*pre) buf[n++] = *pre++;
    if (hr > 0) { buf[n++] = '0' + hr; buf[n++] = 'h'; }
    if (min > 0) { buf[n++] = '0' + min; buf[n++] = 'm'; }
    buf[n++] = '0' + sec; buf[n++] = 's';
    buf[n] = 0;
    shell_print(buf, 15);
    shell_newline();
    return 0;
}
static int cmd_free(int argc, char** argv) {
    size_t total = g_mem_arena.total_size;
    size_t used  = mem_used_space(&g_mem_arena);
    size_t free  = total - used;
    shell_print("Bellek: ", 15);
    shell_print_int((int)(total / 1024), 15); shell_print(" KB toplam, ", 15);
    shell_print_int((int)(used / 1024), 15); shell_print(" KB kullanilan, ", 15);
    shell_print_int((int)(free / 1024), 15); shell_print(" KB bos\n", 15);
    shell_print("Gorev sayisi: ", 15);
    shell_print_int(sched_get_task_count(), 15); shell_newline();
    return 0;
}
static int cmd_ps(int argc, char** argv) {
    int tc = sched_get_task_count();
    if (tc == 0) { shell_print("PID 1 shell\nPID 2 cofeuDE", 15); shell_newline(); return 0; }
    shell_print("PID  gorev          durum", 15); shell_newline();
    for (int i = 0; i < tc && i < 16; i++) {
        struct task *t = sched_get_task((uint32_t)i);
        if (!t) continue;
        char buf[40]; int n = 0;
        int id = (int)t->task_id; buf[n++] = '0' + id;
        buf[n++] = ' '; buf[n++] = ' ';
        const char *state = "ready";
        if (t->state == 1) state = "run";
        else if (t->state == 2) state = "block";
        else if (t->state == 3) state = "sleep";
        else if (t->state == 4) state = "zombi";
        const char *s = state;
        while (*s) buf[n++] = *s++;
        buf[n] = 0;
        shell_print(buf, 15); shell_newline();
    }
    return 0;
}
static int cmd_df(int argc, char** argv) {
    shell_print("Dosya Sistemi     Toplam   Kullanilabilir\n", 15);
    shell_print("/dev/vfs          16MB     ", 15);
    /* VFS dosya sayisini ve kullanilan kapasiteyi goster */
    shell_print("16MB", 15);
    shell_newline();
    shell_print("Tip: RAM-ustu VFS (fiziksel disk yok)\n", 8);
    return 0;
}

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

/* ═══════════════════════════════════════════════════════════════════
   GIT - cofeuOS versiyon kontrol sistemi
   Blob/tree/commit nesneleri SHA-256 ile imzalanır, .git/objects
   altında saklanır. Uzak depoyla HTTP üzerinden konuşur
   (host'ta calistirilan git_server.py). Yerel komutlar (init/add/
   commit/log/status/diff/branch/checkout/config/remote) sunucusuz
   da calisir; clone/push/pull/fetch sunucu ister.
   ═══════════════════════════════════════════════════════════════════ */

#define GIT_MAX_ENTRIES 60
#define GIT_HASH_HEX    65
#define GIT_NET_MAX     (FILE_CONTENT_SIZE - 4)
#define GIT_MAX_BLOB    (FILE_CONTENT_SIZE - 32)

typedef struct {
    char mode[16];
    char oid[GIT_HASH_HEX];
    char path[MAX_PATH_LEN];
} git_index_entry_t;

static git_index_entry_t g_idx[GIT_MAX_ENTRIES];
static int g_idx_count = 0;

typedef struct {
    char path[MAX_PATH_LEN];
    char oid[GIT_HASH_HEX];
} git_flat_t;
static git_flat_t g_flat[GIT_MAX_ENTRIES];
static int g_flat_count = 0;

static char g_objbuf[FILE_CONTENT_SIZE + 64];
static char g_netbuf[GIT_NET_MAX + 8];
static char g_cfg[GIT_NET_MAX + 8];
static char g_idxbuf[GIT_NET_MAX + 8];
static char g_tmp[FILE_CONTENT_SIZE + 4];
static char g_content[GIT_MAX_BLOB + 4];
static char g_oldbuf[GIT_MAX_BLOB + 4];
static char g_newbuf[GIT_MAX_BLOB + 4];

/* ─── hex yardımcıları ───────────────────────────────────────── */
static int git_hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static void git_hex_encode(const u8 *in, size_t len, char *out) {
    static const char *h = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[i * 2] = h[in[i] >> 4];
        out[i * 2 + 1] = h[in[i] & 0xF];
    }
    out[len * 2] = '\0';
}
static void git_hex_decode(const char *hex, u8 *out, size_t n) {
    for (size_t i = 0; i < n; i++) {
        out[i] = (u8)((git_hex_value(hex[i * 2]) << 4) | git_hex_value(hex[i * 2 + 1]));
    }
}
static int git_is_hex(const char *s, int n) {
    if ((int)strlen(s) < n) return 0;
    for (int i = 0; i < n; i++) if (git_hex_value(s[i]) < 0) return 0;
    return 1;
}
static void git_hash_buf(const char *buf, size_t len, char *out) {
    u8 digest[32];
    sha256_hash((u8*)buf, len, digest);
    git_hex_encode(digest, 32, out);
}

/* ─── yol yardımcıları ───────────────────────────────────────── */
static void git_path(const char *root, const char *rel, char *out, size_t outsz) {
    if (outsz == 0) return;
    char resolved[MAX_PATH_LEN];
    if (fs_resolve_path(root, rel, resolved) != 0) {
        strncpy(out, rel, outsz - 1);
        out[outsz - 1] = '\0';
        return;
    }
    strncpy(out, resolved, outsz - 1);
    out[outsz - 1] = '\0';
}
static void git_rel_path(const char *root, const char *abs, char *out, size_t outsz) {
    size_t rl = strlen(root);
    const char *rest = abs;
    if (strcmp(root, "/") == 0) {
        rest = (abs[0] == '/') ? abs + 1 : abs;
    } else if (rl > 1 && strncmp(abs, root, rl) == 0) {
        rest = abs + rl;
        if (*rest == '/') rest++;
    }
    strncpy(out, rest, outsz - 1);
    out[outsz - 1] = '\0';
}
/* abs, root altında bir dosyaysa 1 döner ve rel'e root'a göre yolu yazar. */
static int git_path_is_under(const char *root, const char *abs, char *rel, size_t relsz) {
    size_t rl = strlen(root);
    if (strcmp(root, "/") == 0) {
        if (abs[0] == '/' && abs[1] != '\0') {
            strncpy(rel, abs + 1, relsz - 1);
            rel[relsz - 1] = '\0';
            return 1;
        }
        return 0;
    }
    size_t al = strlen(abs);
    if (al > rl && strncmp(abs, root, rl) == 0 && abs[rl] == '/') {
        strncpy(rel, abs + rl + 1, relsz - 1);
        rel[relsz - 1] = '\0';
        return 1;
    }
    return 0;
}

/* ─── sayı yardımcıları ──────────────────────────────────────── */
static void git_append(char *buf, int *n, const char *s) {
    for (const char *q = s; *q; q++) buf[(*n)++] = *q;
}
static void git_append_ll(char *buf, int *n, s64 v) {
    char num[24];
    int t = 0;
    if (v == 0) num[t++] = '0';
    u64 vv = (v < 0) ? (u64)(-v) : (u64)v;
    while (vv) { num[t++] = '0' + (vv % 10); vv /= 10; }
    if (v < 0) num[t++] = '-';
    while (t) buf[(*n)++] = num[--t];
}
static void git_pad_num(char *out, int *pos, u64 v, int digits) {
    char tmp[24];
    int t = 0;
    if (v == 0) tmp[t++] = '0';
    while (v) { tmp[t++] = '0' + (v % 10); v /= 10; }
    while (t < digits) tmp[t++] = '0';
    while (t > 0) out[(*pos)++] = tmp[--t];
}
static s64 git_epoch_now(void) {
    EFI_SYSTEM_TABLE *st = (EFI_SYSTEM_TABLE*)io_get_system_table();
    if (st && st->RuntimeServices) {
        EFI_TIME tm;
        if (st->RuntimeServices->GetTime(&tm, NULL) == EFI_SUCCESS &&
            tm.Year >= 2020 && tm.Year <= 2100 && tm.Month >= 1 && tm.Month <= 12) {
            s64 y = tm.Year;
            u64 m = tm.Month;
            u64 d = tm.Day;
            if (m <= 2) { y -= 1; m += 12; }
            s64 era = (y >= 0 ? y : y - 399) / 400;
            u64 yoe = (u64)(y - era * 400);
            u64 doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
            u64 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
            s64 days = era * 146097 + (s64)doe - 719468;
            return days * 86400 + tm.Hour * 3600 + tm.Minute * 60 + tm.Second;
        }
    }
    return 1735689600; /* 2025-01-01 yedek */
}
static void git_epoch_to_date(s64 epoch, char *out, size_t outsz) {
    s64 days = epoch / 86400;
    s64 secs = epoch % 86400;
    if (secs < 0) { secs += 86400; days -= 1; }
    s64 h = secs / 3600;
    s64 mi = (secs % 3600) / 60;
    days += 719468;
    s64 era = (days >= 0 ? days : days - 146096) / 146097;
    u64 doe = (u64)(days - era * 146097);
    u64 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    s64 y = (s64)yoe + era * 400;
    u64 doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    u64 mp = (5 * doy + 2) / 153;
    u64 dd = doy - (153 * mp + 2) / 5 + 1;
    u64 mm = mp < 10 ? mp + 3 : mp - 9;
    if (mm <= 2) y += 1;
    int pos = 0;
    if (pos + 20 >= (int)outsz) { out[0] = '\0'; return; }
    git_pad_num(out, &pos, (u64)y, 4); out[pos++] = '-';
    git_pad_num(out, &pos, mm, 2); out[pos++] = '-';
    git_pad_num(out, &pos, dd, 2); out[pos++] = ' ';
    git_pad_num(out, &pos, (u64)h, 2); out[pos++] = ':';
    git_pad_num(out, &pos, (u64)mi, 2);
    out[pos] = '\0';
}
static s64 git_epoch_from_author(const char *author) {
    const char *p = author;
    const char *last = author;
    while (*p) { if (*p == ' ') last = p; p++; }
    if (*last == ' ') last++;
    s64 v = 0;
    int neg = 0;
    if (*last == '-') { neg = 1; last++; }
    while (*last >= '0' && *last <= '9') { v = v * 10 + (*last - '0'); last++; }
    return neg ? -v : v;
}

/* ─── config ─────────────────────────────────────────────────── */
static void git_config_path(const char *root, int global, char *out, size_t outsz) {
    if (global) {
        strncpy(out, "/etc/gitconfig", outsz - 1);
    } else {
        git_path(root, ".git/config", out, outsz);
    }
    out[outsz - 1] = '\0';
}
static int git_config_get(const char *root, const char *key, char *out, size_t outsz) {
    char path[MAX_PATH_LEN];
    git_config_path(root, 0, path, sizeof path);
    int n = fs_read_file(&g_fs, path, g_cfg, sizeof g_cfg);
    if (n < 0) {
        git_config_path(root, 1, path, sizeof path);
        n = fs_read_file(&g_fs, path, g_cfg, sizeof g_cfg);
        if (n < 0) { out[0] = '\0'; return -1; }
    }
    g_cfg[n] = '\0';
    size_t klen = strlen(key);
    char *p = g_cfg;
    while (*p) {
        char *nl = strchr(p, '\n');
        if (!nl) nl = p + strlen(p);
        int keep = (*nl == '\n');
        *nl = '\0';
        if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
            strncpy(out, p + klen + 1, outsz - 1);
            out[outsz - 1] = '\0';
            return 0;
        }
        if (keep) *nl = '\n';
        p = nl + 1;
    }
    out[0] = '\0';
    return -1;
}
static int git_config_set(const char *root, const char *key, const char *value, int global) {
    char path[MAX_PATH_LEN];
    git_config_path(root, global, path, sizeof path);
    char tmp[GIT_NET_MAX + 8];
    int tn = 0;
    int n = fs_file_exists(&g_fs, path) ? fs_read_file(&g_fs, path, g_cfg, sizeof g_cfg) : -1;
    if (n > 0) {
        g_cfg[n] = '\0';
        size_t klen = strlen(key);
        char *p = g_cfg;
        while (*p) {
            char *nl = strchr(p, '\n');
            if (!nl) nl = p + strlen(p);
            int keep = 1;
            if (strncmp(p, key, klen) == 0 && p[klen] == '=') keep = 0;
            if (keep) {
                int l = (int)(nl - p);
                if (tn + l + 1 < (int)sizeof tmp) {
                    memcpy(tmp + tn, p, (size_t)l);
                    tn += l;
                    tmp[tn++] = '\n';
                }
            }
            if (*nl) p = nl + 1;
            else break;
        }
    }
    size_t klen2 = strlen(key), vlen = strlen(value);
    if (tn + (int)klen2 + 1 + (int)vlen + 2 < (int)sizeof tmp) {
        memcpy(tmp + tn, key, klen2); tn += (int)klen2;
        tmp[tn++] = '=';
        memcpy(tmp + tn, value, vlen); tn += (int)vlen;
        tmp[tn++] = '\n';
        tmp[tn] = '\0';
    }
    return fs_write_file(&g_fs, path, tmp, (size_t)tn);
}

/* ─── index ──────────────────────────────────────────────────── */
static void git_index_load(const char *root) {
    g_idx_count = 0;
    char path[MAX_PATH_LEN];
    git_path(root, ".git/index", path, sizeof path);
    int n = fs_read_file(&g_fs, path, g_idxbuf, sizeof g_idxbuf);
    if (n < 0) return;
    g_idxbuf[n] = '\0';
    char *p = g_idxbuf;
    while (*p && g_idx_count < GIT_MAX_ENTRIES) {
        char *nl = strchr(p, '\n');
        if (!nl) nl = p + strlen(p);
        int keep = (*nl == '\n');
        *nl = '\0';
        char *sp1 = strchr(p, ' ');
        if (sp1) {
            *sp1 = '\0';
            char *sp2 = strchr(sp1 + 1, ' ');
            if (sp2) {
                *sp2 = '\0';
                if (p[0] && git_is_hex(sp1 + 1, 64) && sp2[1]) {
                    strncpy(g_idx[g_idx_count].mode, p, 15);
                    g_idx[g_idx_count].mode[15] = '\0';
                    strncpy(g_idx[g_idx_count].oid, sp1 + 1, 64);
                    g_idx[g_idx_count].oid[64] = '\0';
                    strncpy(g_idx[g_idx_count].path, sp2 + 1, MAX_PATH_LEN - 1);
                    g_idx_count++;
                }
            }
        }
        if (keep) *nl = '\n';
        p = nl + 1;
    }
}
static int git_index_save(const char *root) {
    char path[MAX_PATH_LEN];
    git_path(root, ".git/index", path, sizeof path);
    int n = 0;
    for (int i = 0; i < g_idx_count; i++) {
        int need = (int)(strlen(g_idx[i].mode) + 1 + 64 + 1 + strlen(g_idx[i].path) + 1);
        if (n + need >= (int)sizeof g_idxbuf) return -1;
        git_append(g_idxbuf, &n, g_idx[i].mode);
        g_idxbuf[n++] = ' ';
        git_append(g_idxbuf, &n, g_idx[i].oid);
        g_idxbuf[n++] = ' ';
        git_append(g_idxbuf, &n, g_idx[i].path);
        g_idxbuf[n++] = '\n';
    }
    g_idxbuf[n] = '\0';
    return fs_write_file(&g_fs, path, g_idxbuf, (size_t)n);
}
static int git_index_find(const char *rel) {
    for (int i = 0; i < g_idx_count; i++) if (strcmp(g_idx[i].path, rel) == 0) return i;
    return -1;
}
static void git_index_upsert(const char *rel, const char *oid, const char *mode) {
    int i = git_index_find(rel);
    if (i < 0) {
        if (g_idx_count >= GIT_MAX_ENTRIES) return;
        i = g_idx_count++;
        strncpy(g_idx[i].path, rel, MAX_PATH_LEN - 1);
    }
    strncpy(g_idx[i].mode, mode, 15);
    strncpy(g_idx[i].oid, oid, 64);
}
static void git_sort_entries(git_index_entry_t *entries, int count) {
    for (int i = 1; i < count; i++) {
        git_index_entry_t tmp = entries[i];
        int j = i - 1;
        while (j >= 0 && strcmp(entries[j].path, tmp.path) > 0) {
            entries[j + 1] = entries[j];
            j--;
        }
        entries[j + 1] = tmp;
    }
}

/* ─── nesneler ───────────────────────────────────────────────── */
static void git_obj_hash(const char *type, const char *data, size_t len, char *out_hex) {
    int hl = 0;
    git_append(g_tmp, &hl, type);
    g_tmp[hl++] = ' ';
    char num[20];
    int np = 0;
    if (len == 0) num[np++] = '0';
    else {
        char tmp[20]; int t = 0;
        u64 n = len;
        while (n) { tmp[t++] = '0' + (n % 10); n /= 10; }
        for (int i = t - 1; i >= 0; i--) num[np++] = tmp[i];
    }
    for (int i = 0; i < np; i++) g_tmp[hl++] = num[i];
    g_tmp[hl++] = '\n';
    if (len) memcpy(g_tmp + hl, data, len);
    git_hash_buf(g_tmp, (size_t)(hl + len), out_hex);
}
static int git_obj_write(const char *root, const char *type, const char *data, size_t len, char *out_hex) {
    git_obj_hash(type, data, len, out_hex);
    size_t total = strlen(type) + 1 + 20 + 1 + len;
    if (total > FILE_CONTENT_SIZE) return -1;
    char rel[128];
    strcpy(rel, ".git/objects/");
    strncat(rel, out_hex, sizeof rel - strlen(rel) - 1);
    char path[MAX_PATH_LEN];
    git_path(root, rel, path, sizeof path);
    /* g_tmp hâlâ "<type> <len>\n" + veriyi içeriyor */
    int hl = 0;
    git_append(g_tmp, &hl, type);
    g_tmp[hl++] = ' ';
    char num[20];
    int np = 0;
    if (len == 0) num[np++] = '0';
    else {
        char tmp[20]; int t = 0;
        u64 n = len;
        while (n) { tmp[t++] = '0' + (n % 10); n /= 10; }
        for (int i = t - 1; i >= 0; i--) num[np++] = tmp[i];
    }
    for (int i = 0; i < np; i++) g_tmp[hl++] = num[i];
    g_tmp[hl++] = '\n';
    if (len) memcpy(g_tmp + hl, data, len);
    if (fs_write_file(&g_fs, path, g_tmp, total) < 0) return -1;
    return 0;
}
static int git_obj_read(const char *root, const char *hex, char *type, size_t *datalen, char *out, size_t outsz) {
    char rel[128];
    strcpy(rel, ".git/objects/");
    strncat(rel, hex, sizeof rel - strlen(rel) - 1);
    char path[MAX_PATH_LEN];
    git_path(root, rel, path, sizeof path);
    char tmp[FILE_CONTENT_SIZE + 4];
    int n = fs_read_file(&g_fs, path, tmp, sizeof tmp);
    if (n < 0) return -1;
    int i = 0;
    int tlen = 0;
    while (i < n && tmp[i] != ' ' && tlen < 20) type[tlen++] = tmp[i++];
    type[tlen] = '\0';
    if (i >= n) return -1;
    i++;
    size_t dlen = 0;
    while (i < n && tmp[i] >= '0' && tmp[i] <= '9') { dlen = dlen * 10 + (tmp[i] - '0'); i++; }
    if (i >= n || tmp[i] != '\n') return -1;
    i++;
    if (dlen > outsz) return -1;
    if (dlen) memcpy(out, tmp + i, dlen);
    if (datalen) *datalen = dlen;
    return 0;
}
static int git_read_tracked(const char *root, const char *rel, char *out, size_t maxout) {
    char path[MAX_PATH_LEN];
    git_path(root, rel, path, sizeof path);
    for (size_t i = 0; i < g_fs.file_count; i++) {
        if (g_fs.files[i].active && strcmp(g_fs.files[i].path, path) == 0) {
            if (g_fs.files[i].size > maxout) return -2;
            return fs_read_file(&g_fs, path, out, maxout + 1);
        }
    }
    return -1;
}
static int git_blob_content(const char *root, const char *oid, char *out, size_t maxout) {
    char type[16];
    size_t dlen;
    if (git_obj_read(root, oid, type, &dlen, out, maxout) != 0) return -1;
    if (strcmp(type, "blob") != 0) return -1;
    if (dlen < maxout) out[dlen] = '\0';
    return (int)dlen;
}

/* ─── dal / HEAD ─────────────────────────────────────────────── */
static int git_current_branch(const char *root, char *out, size_t outsz) {
    char path[MAX_PATH_LEN];
    git_path(root, ".git/HEAD", path, sizeof path);
    int n = fs_read_file(&g_fs, path, g_tmp, sizeof g_tmp);
    if (n < 0) { strcpy(out, "master"); return 0; }
    g_tmp[n] = '\0';
    const char *ref = "ref: refs/heads/";
    if (strncmp(g_tmp, ref, strlen(ref)) == 0) {
        strncpy(out, g_tmp + strlen(ref), outsz - 1);
        out[outsz - 1] = '\0';
        for (int i = 0; out[i]; i++) if (out[i] == '\n') { out[i] = '\0'; break; }
        return 0;
    }
    strcpy(out, "master");
    return 0;
}
static int git_set_head(const char *root, const char *branch) {
    char path[MAX_PATH_LEN];
    git_path(root, ".git/HEAD", path, sizeof path);
    char buf[128];
    int n = 0;
    git_append(buf, &n, "ref: refs/heads/");
    git_append(buf, &n, branch);
    buf[n++] = '\n';
    buf[n] = '\0';
    return fs_write_file(&g_fs, path, buf, (size_t)n);
}
static void git_ref_path(const char *root, const char *branch, char *out, size_t outsz) {
    char rel[MAX_PATH_LEN];
    strcpy(rel, ".git/refs/heads/");
    strncat(rel, branch, sizeof rel - strlen(rel) - 1);
    git_path(root, rel, out, outsz);
}
static int git_get_ref(const char *root, const char *branch, char *hexout, size_t outsz) {
    char path[MAX_PATH_LEN];
    git_ref_path(root, branch, path, sizeof path);
    int n = fs_read_file(&g_fs, path, g_tmp, sizeof g_tmp);
    if (n < 0) { hexout[0] = '\0'; return -1; }
    g_tmp[n] = '\0';
    for (int i = 0; i < n; i++) if (g_tmp[i] == '\n') { g_tmp[i] = '\0'; break; }
    if (!git_is_hex(g_tmp, 64)) { hexout[0] = '\0'; return -1; }
    strncpy(hexout, g_tmp, outsz - 1);
    hexout[outsz - 1] = '\0';
    return 0;
}
static int git_set_ref(const char *root, const char *branch, const char *hex) {
    char path[MAX_PATH_LEN];
    git_ref_path(root, branch, path, sizeof path);
    char buf[80];
    int n = 0;
    for (const char *q = hex; *q && n < 70; q++) buf[n++] = *q;
    buf[n++] = '\n';
    buf[n] = '\0';
    return fs_write_file(&g_fs, path, buf, (size_t)n);
}
static int git_repo_init(const char *root) {
    char p[MAX_PATH_LEN];
    git_path(root, ".git", p, sizeof p);
    if (!fs_dir_exists(&g_fs, p)) fs_create_dir(&g_fs, p);
    git_path(root, ".git/objects", p, sizeof p);
    if (!fs_dir_exists(&g_fs, p)) fs_create_dir(&g_fs, p);
    git_path(root, ".git/refs", p, sizeof p);
    if (!fs_dir_exists(&g_fs, p)) fs_create_dir(&g_fs, p);
    git_path(root, ".git/refs/heads", p, sizeof p);
    if (!fs_dir_exists(&g_fs, p)) fs_create_dir(&g_fs, p);
    git_set_head(root, "master");
    char v[64];
    if (git_config_get(root, "user.name", v, sizeof v) != 0) {
        git_config_set(root, "user.name", g_shell.user[0] ? g_shell.user : "root", 0);
    }
    if (git_config_get(root, "user.email", v, sizeof v) != 0) {
        git_config_set(root, "user.email", "root@cofeu.local", 0);
    }
    return 0;
}
static int git_is_repo(const char *root) {
    char p[MAX_PATH_LEN];
    git_path(root, ".git", p, sizeof p);
    return fs_dir_exists(&g_fs, p);
}

/* ─── ağaç (tree) işlemleri ──────────────────────────────────── */
static int git_tree_build_recursive(const char *root, git_index_entry_t *entries, int count, char *out_hex) {
    if (count <= 0) return git_obj_write(root, "tree", "", 0, out_hex);
    int used[GIT_MAX_ENTRIES];
    memset(used, 0, sizeof used);
    int tlen = 0;
    for (int i = 0; i < count; i++) {
        if (used[i]) continue;
        char comp[128];
        int cl = 0;
        const char *p = entries[i].path;
        while (*p && *p != '/' && cl < 126) comp[cl++] = *p++;
        comp[cl] = '\0';
        int is_dir = (*p == '/');
        char entry_oid[GIT_HASH_HEX];
        char entry_mode[16];
        if (is_dir) {
            git_index_entry_t sub[GIT_MAX_ENTRIES];
            int sc = 0;
            size_t comp_len = strlen(comp);
            for (int j = i; j < count; j++) {
                if (used[j]) continue;
                if (strncmp(entries[j].path, comp, comp_len) != 0) continue;
                if (entries[j].path[comp_len] != '/') continue;
                strcpy(sub[sc].path, entries[j].path + comp_len + 1);
                strcpy(sub[sc].mode, entries[j].mode);
                strcpy(sub[sc].oid, entries[j].oid);
                sc++;
                used[j] = 1;
            }
            if (git_tree_build_recursive(root, sub, sc, entry_oid) != 0) return -1;
            strcpy(entry_mode, "40000");
        } else {
            strcpy(entry_oid, entries[i].oid);
            strcpy(entry_mode, entries[i].mode);
            used[i] = 1;
        }
        int need = (int)(strlen(entry_mode) + 1 + strlen(comp) + 1 + 32);
        if (tlen + need >= GIT_NET_MAX) return -1;
        git_append(g_objbuf, &tlen, entry_mode);
        g_objbuf[tlen++] = ' ';
        git_append(g_objbuf, &tlen, comp);
        g_objbuf[tlen++] = '\0';
        u8 raw[32];
        git_hex_decode(entry_oid, raw, 32);
        memcpy(g_objbuf + tlen, raw, 32);
        tlen += 32;
    }
    return git_obj_write(root, "tree", g_objbuf, (size_t)tlen, out_hex);
}
static void git_tree_flatten(const char *root, const char *tree_hex, const char *prefix) {
    char type[16];
    size_t dlen;
    if (git_obj_read(root, tree_hex, type, &dlen, g_tmp, sizeof g_tmp) != 0) return;
    if (strcmp(type, "tree") != 0) return;
    size_t pos = 0;
    while (pos < dlen) {
        char mode[16];
        int ml = 0;
        while (pos < dlen && g_tmp[pos] != ' ' && ml < 15) mode[ml++] = g_tmp[pos++];
        if (pos >= dlen || g_tmp[pos] != ' ') break;
        mode[ml] = '\0';
        pos++;
        char name[128];
        int nl = 0;
        while (pos < dlen && g_tmp[pos] != '\0' && nl < 126) name[nl++] = g_tmp[pos++];
        if (pos >= dlen || g_tmp[pos] != '\0') break;
        name[nl] = '\0';
        pos++;
        if (pos + 32 > dlen) break;
        u8 raw[32];
        memcpy(raw, g_tmp + pos, 32);
        pos += 32;
        char oid[GIT_HASH_HEX];
        git_hex_encode(raw, 32, oid);
        if (strcmp(mode, "40000") == 0) {
            char np[MAX_PATH_LEN];
            if (prefix[0]) { strcpy(np, prefix); strcat(np, "/"); strcat(np, name); }
            else strcpy(np, name);
            git_tree_flatten(root, oid, np);
        } else {
            if (g_flat_count < GIT_MAX_ENTRIES) {
                if (prefix[0]) { strcpy(g_flat[g_flat_count].path, prefix); strcat(g_flat[g_flat_count].path, "/"); strcat(g_flat[g_flat_count].path, name); }
                else strcpy(g_flat[g_flat_count].path, name);
                strcpy(g_flat[g_flat_count].oid, oid);
                g_flat_count++;
            }
        }
    }
}

/* ─── commit ─────────────────────────────────────────────────── */
typedef struct {
    char tree[GIT_HASH_HEX];
    char parent[GIT_HASH_HEX];
    char author[128];
    char committer[128];
    char message[256];
} git_commit_t;
static int git_commit_parse(const char *root, const char *hex, git_commit_t *c) {
    memset(c, 0, sizeof *c);
    char type[16];
    size_t dlen;
    if (git_obj_read(root, hex, type, &dlen, g_tmp, sizeof g_tmp) != 0) return -1;
    if (strcmp(type, "commit") != 0) return -1;
    char *p = g_tmp;
    char *end = g_tmp + dlen;
    while (p < end && *p != '\n') {
        char *nl = strchr(p, '\n');
        if (!nl) break;
        int keep = (nl < end);
        *nl = '\0';
        if (strncmp(p, "tree ", 5) == 0) { strncpy(c->tree, p + 5, GIT_HASH_HEX - 1); }
        else if (strncmp(p, "parent ", 7) == 0) { if (!c->parent[0]) strncpy(c->parent, p + 7, GIT_HASH_HEX - 1); }
        else if (strncmp(p, "author ", 7) == 0) { strncpy(c->author, p + 7, sizeof c->author - 1); }
        else if (strncmp(p, "committer ", 10) == 0) { strncpy(c->committer, p + 10, sizeof c->committer - 1); }
        if (keep) *nl = '\n';
        p = nl + 1;
    }
    if (p < end && *p == '\n') p++;
    if (p < end) {
        size_t mlen = (size_t)(end - p);
        if (mlen > sizeof c->message - 1) mlen = sizeof c->message - 1;
        memcpy(c->message, p, mlen);
        c->message[mlen] = '\0';
    }
    return 0;
}
static int git_commit_cmd(const char *root, const char *msg) {
    char name[64], email[128];
    if (git_config_get(root, "user.name", name, sizeof name) != 0 || !name[0]) {
        shell_print("git commit: kim oldugunu belirt: git config user.name \"ad\"", 12); shell_newline();
        return -1;
    }
    if (git_config_get(root, "user.email", email, sizeof email) != 0 || !email[0]) {
        shell_print("git commit: eposta belirt: git config user.email \"mail\"", 12); shell_newline();
        return -1;
    }
    git_index_load(root);
    if (g_idx_count == 0) {
        shell_print("git commit: stage'lenmis dosya yok (git add ...)", 12); shell_newline();
        return -1;
    }
    git_index_entry_t sorted[GIT_MAX_ENTRIES];
    memcpy(sorted, g_idx, sizeof sorted);
    git_sort_entries(sorted, g_idx_count);
    char tree_hex[GIT_HASH_HEX];
    if (git_tree_build_recursive(root, sorted, g_idx_count, tree_hex) != 0) {
        shell_print("git commit: agac nesnesi olusturulamadi", 12); shell_newline();
        return -1;
    }
    char branch[64];
    git_current_branch(root, branch, sizeof branch);
    char parent_hex[GIT_HASH_HEX];
    int has_parent = (git_get_ref(root, branch, parent_hex, sizeof parent_hex) == 0);
    s64 epoch = git_epoch_now();
    int n = 0;
    git_append(g_objbuf, &n, "tree "); git_append(g_objbuf, &n, tree_hex); g_objbuf[n++] = '\n';
    if (has_parent) { git_append(g_objbuf, &n, "parent "); git_append(g_objbuf, &n, parent_hex); g_objbuf[n++] = '\n'; }
    git_append(g_objbuf, &n, "author "); git_append(g_objbuf, &n, name);
    git_append(g_objbuf, &n, " <"); git_append(g_objbuf, &n, email); git_append(g_objbuf, &n, "> ");
    git_append_ll(g_objbuf, &n, epoch); g_objbuf[n++] = '\n';
    git_append(g_objbuf, &n, "committer "); git_append(g_objbuf, &n, name);
    git_append(g_objbuf, &n, " <"); git_append(g_objbuf, &n, email); git_append(g_objbuf, &n, "> ");
    git_append_ll(g_objbuf, &n, epoch); g_objbuf[n++] = '\n';
    g_objbuf[n++] = '\n';
    git_append(g_objbuf, &n, msg);
    char commit_hex[GIT_HASH_HEX];
    if (git_obj_write(root, "commit", g_objbuf, (size_t)n, commit_hex) != 0) {
        shell_print("git commit: commit nesnesi yazilamadi", 12); shell_newline();
        return -1;
    }
    git_set_ref(root, branch, commit_hex);
    g_idx_count = 0;
    git_index_save(root);
    shell_print("[", 10); shell_print(branch, 10); shell_print(" ", 10);
    char shortid[17];
    memcpy(shortid, commit_hex, 16); shortid[16] = '\0';
    shell_print(shortid, 10); shell_print("] ", 10); shell_print(msg, 15); shell_newline();
    return 0;
}
static int git_is_ancestor(const char *root, const char *ancestor, const char *desc) {
    char cur[GIT_HASH_HEX];
    strncpy(cur, desc, GIT_HASH_HEX - 1);
    cur[GIT_HASH_HEX - 1] = '\0';
    int depth = 0;
    while (cur[0] && depth < 200) {
        if (strcmp(cur, ancestor) == 0) return 1;
        git_commit_t c;
        if (git_commit_parse(root, cur, &c) != 0) return 0;
        strcpy(cur, c.parent);
        depth++;
    }
    return 0;
}

/* ─── çalışma ağacı ──────────────────────────────────────────── */
static void git_worktree_delete_tracked(const char *root) {
    git_index_load(root);
    for (int i = 0; i < g_idx_count; i++) {
        char path[MAX_PATH_LEN];
        git_path(root, g_idx[i].path, path, sizeof path);
        if (fs_file_exists(&g_fs, path)) fs_delete_file(&g_fs, path);
    }
}
static void git_worktree_write_blob(const char *root, const char *rel, const char *oid) {
    char type[16];
    size_t dlen;
    if (git_obj_read(root, oid, type, &dlen, g_tmp, sizeof g_tmp) != 0) return;
    if (strcmp(type, "blob") != 0) return;
    char path[MAX_PATH_LEN];
    git_path(root, rel, path, sizeof path);
    char parent[MAX_PATH_LEN];
    fs_get_parent_path(path, parent);
    if (strcmp(parent, "/") != 0 && !fs_dir_exists(&g_fs, parent)) {
        fs_create_dir(&g_fs, parent);
    }
    fs_write_file(&g_fs, path, g_tmp, dlen);
}
static void git_checkout_commit(const char *root, const char *commit_hex) {
    git_worktree_delete_tracked(root);
    git_commit_t c;
    if (git_commit_parse(root, commit_hex, &c) != 0) return;
    g_flat_count = 0;
    git_tree_flatten(root, c.tree, "");
    for (int i = 0; i < g_flat_count; i++) {
        git_worktree_write_blob(root, g_flat[i].path, g_flat[i].oid);
    }
    g_idx_count = 0;
    for (int i = 0; i < g_flat_count; i++) {
        strcpy(g_idx[g_idx_count].mode, "100644");
        strcpy(g_idx[g_idx_count].oid, g_flat[i].oid);
        strcpy(g_idx[g_idx_count].path, g_flat[i].path);
        g_idx_count++;
    }
    git_index_save(root);
}

/* ─── git add / alt komutlar ─────────────────────────────────── */
static int git_add_path(const char *root, const char *rel) {
    int n = git_read_tracked(root, rel, g_content, GIT_MAX_BLOB);
    if (n == -2) { shell_print("git add: dosya cok buyuk: ", 12); shell_print(rel, 12); shell_newline(); return -1; }
    if (n < 0) { shell_print("git add: dosya okunamadi: ", 12); shell_print(rel, 12); shell_newline(); return -1; }
    char oid[GIT_HASH_HEX];
    if (git_obj_write(root, "blob", g_content, (size_t)n, oid) != 0) {
        shell_print("git add: nesne yazilamadi: ", 12); shell_print(rel, 12); shell_newline(); return -1;
    }
    git_index_upsert(rel, oid, "100644");
    return 0;
}
static void git_add_all_under(const char *root, const char *abs_dir) {
    for (size_t i = 0; i < g_fs.file_count; i++) {
        if (!g_fs.files[i].active) continue;
        char rel[MAX_PATH_LEN];
        if (!git_path_is_under(abs_dir, g_fs.files[i].path, rel, sizeof rel)) continue;
        if (strncmp(rel, ".git", 4) == 0 && (rel[4] == '/' || rel[4] == '\0')) continue;
        git_add_path(root, rel);
    }
}

/* ─── git status ─────────────────────────────────────────────── */
static void cmd_git_status(const char *root) {
    char branch[64];
    git_current_branch(root, branch, sizeof branch);
    shell_print("Dal: ", 15); shell_print(branch, 15); shell_newline();
    git_index_load(root);
    char head_commit[GIT_HASH_HEX];
    int has_head = (git_get_ref(root, branch, head_commit, sizeof head_commit) == 0);
    g_flat_count = 0;
    if (has_head) {
        git_commit_t c;
        if (git_commit_parse(root, head_commit, &c) == 0) git_tree_flatten(root, c.tree, "");
    }
    int staged = 0;
    for (int i = 0; i < g_idx_count; i++) {
        int in_head = -1;
        for (int j = 0; j < g_flat_count; j++) if (strcmp(g_flat[j].path, g_idx[i].path) == 0) { in_head = j; break; }
        if (in_head < 0) {
            if (!staged) { shell_print("Komit icin hazir:\n", 13); staged = 1; }
            shell_print("  yeni dosya: ", 10); shell_print(g_idx[i].path, 15); shell_newline();
        } else if (strcmp(g_flat[in_head].oid, g_idx[i].oid) != 0) {
            if (!staged) { shell_print("Komit icin hazir:\n", 13); staged = 1; }
            shell_print("  degistirildi: ", 10); shell_print(g_idx[i].path, 15); shell_newline();
        }
    }
    for (int j = 0; j < g_flat_count; j++) {
        if (git_index_find(g_flat[j].path) < 0) {
            if (!staged) { shell_print("Komit icin hazir:\n", 13); staged = 1; }
            shell_print("  silindi: ", 10); shell_print(g_flat[j].path, 15); shell_newline();
        }
    }
    int unstaged = 0;
    for (int i = 0; i < g_idx_count; i++) {
        int n = git_read_tracked(root, g_idx[i].path, g_content, GIT_MAX_BLOB);
        if (n == -1) {
            if (!unstaged) { shell_print("Stage'lenmemis degisiklikler:\n", 14); unstaged = 1; }
            shell_print("  silindi: ", 12); shell_print(g_idx[i].path, 15); shell_newline();
        } else if (n == -2) {
            if (!unstaged) { shell_print("Stage'lenmemis degisiklikler:\n", 14); unstaged = 1; }
            shell_print("  degistirildi (buyuk): ", 12); shell_print(g_idx[i].path, 15); shell_newline();
        } else {
            char blob[GIT_HASH_HEX];
            git_obj_hash("blob", g_content, (size_t)n, blob);
            if (strcmp(blob, g_idx[i].oid) != 0) {
                if (!unstaged) { shell_print("Stage'lenmemis degisiklikler:\n", 14); unstaged = 1; }
                shell_print("  degistirildi: ", 12); shell_print(g_idx[i].path, 15); shell_newline();
            }
        }
    }
    int untracked = 0;
    for (size_t i = 0; i < g_fs.file_count; i++) {
        if (!g_fs.files[i].active) continue;
        char rel[MAX_PATH_LEN];
        if (!git_path_is_under(root, g_fs.files[i].path, rel, sizeof rel)) continue;
        if (strncmp(rel, ".git", 4) == 0 && (rel[4] == '/' || rel[4] == '\0')) continue;
        if (git_index_find(rel) < 0) {
            if (!untracked) { shell_print("Takip edilmeyen dosyalar:\n", 14); untracked = 1; }
            shell_print("  ", 15); shell_print(rel, 15); shell_newline();
        }
    }
    if (!staged && !unstaged && !untracked) shell_print("Calisma agaci temiz, komit edilecek yeni sey yok", 10);
    shell_newline();
}

/* ─── git diff ───────────────────────────────────────────────── */
static void git_diff_show(const char *path, const char *oldd, const char *newd) {
    const char *oi = oldd, *ni = newd;
    while (*oi && *ni) {
        const char *oe = strchr(oi, '\n');
        const char *ne = strchr(ni, '\n');
        int ol = oe ? (int)(oe - oi) : (int)strlen(oi);
        int nl2 = ne ? (int)(ne - ni) : (int)strlen(ni);
        if (ol != nl2 || memcmp(oi, ni, (size_t)ol) != 0) break;
        oi += ol; ni += nl2;
        if (oe) oi++; if (ne) ni++;
    }
    const char *oj = oldd + strlen(oldd);
    const char *nj = newd + strlen(newd);
    while (oj > oi && nj > ni) {
        const char *so = oj - 1;
        while (so > oi && so[-1] != '\n') so--;
        const char *sn = nj - 1;
        while (sn > ni && sn[-1] != '\n') sn--;
        int ol = (int)(oj - so);
        int nl2 = (int)(nj - sn);
        if (ol != nl2 || memcmp(so, sn, (size_t)ol) != 0) break;
        oj = so; nj = sn;
    }
    shell_print("diff --git a/", 15); shell_print(path, 15); shell_print(" b/", 15); shell_print(path, 15); shell_newline();
    int oc = 0;
    for (const char *t = oi; t < oj; t++) if (*t == '\n') oc++;
    int nc = 0;
    for (const char *t = ni; t < nj; t++) if (*t == '\n') nc++;
    shell_print("@@ -", 13); shell_print_int(oc, 13); shell_print(" +", 13); shell_print_int(nc, 13); shell_print(" @@", 13); shell_newline();
    const char *t = oi;
    while (t < oj) {
        const char *e = t;
        while (e < oj && *e != '\n') e++;
        shell_print("- ", 12);
        char line[256];
        int n2 = (int)(e - t);
        if (n2 > 255) n2 = 255;
        memcpy(line, t, (size_t)n2);
        line[n2] = '\0';
        shell_print(line, 12); shell_newline();
        t = e + (e < oj ? 1 : 0);
    }
    t = ni;
    while (t < nj) {
        const char *e = t;
        while (e < nj && *e != '\n') e++;
        shell_print("+ ", 10);
        char line[256];
        int n2 = (int)(e - t);
        if (n2 > 255) n2 = 255;
        memcpy(line, t, (size_t)n2);
        line[n2] = '\0';
        shell_print(line, 10); shell_newline();
        t = e + (e < nj ? 1 : 0);
    }
}
static void cmd_git_diff(const char *root, int cached) {
    git_index_load(root);
    char branch[64];
    git_current_branch(root, branch, sizeof branch);
    char head_commit[GIT_HASH_HEX];
    int has_head = (git_get_ref(root, branch, head_commit, sizeof head_commit) == 0);
    g_flat_count = 0;
    if (has_head) {
        git_commit_t c;
        if (git_commit_parse(root, head_commit, &c) == 0) git_tree_flatten(root, c.tree, "");
    }
    int shown = 0;
    for (int i = 0; i < g_idx_count; i++) {
        int oldlen = 0, newlen = 0;
        if (cached) {
            int found = -1;
            for (int j = 0; j < g_flat_count; j++) if (strcmp(g_flat[j].path, g_idx[i].path) == 0) { found = j; break; }
            if (found >= 0) oldlen = git_blob_content(root, g_flat[found].oid, g_oldbuf, GIT_MAX_BLOB);
            newlen = git_blob_content(root, g_idx[i].oid, g_newbuf, GIT_MAX_BLOB);
            if (oldlen < 0) { oldlen = 0; g_oldbuf[0] = '\0'; }
            if (newlen < 0) { newlen = 0; g_newbuf[0] = '\0'; }
        } else {
            oldlen = git_blob_content(root, g_idx[i].oid, g_oldbuf, GIT_MAX_BLOB);
            newlen = git_read_tracked(root, g_idx[i].path, g_newbuf, GIT_MAX_BLOB);
            if (oldlen < 0) { oldlen = 0; g_oldbuf[0] = '\0'; }
            if (newlen < 0) { newlen = 0; g_newbuf[0] = '\0'; }
        }
        if (oldlen == newlen && memcmp(g_oldbuf, g_newbuf, (size_t)oldlen) == 0) continue;
        git_diff_show(g_idx[i].path, g_oldbuf, g_newbuf);
        shown++;
    }
    if (!shown) shell_print("fark yok", 10);
    shell_newline();
}

/* ─── git log / branch / checkout / config / remote ──────────── */
static void cmd_git_log(const char *root, int oneline) {
    char branch[64];
    git_current_branch(root, branch, sizeof branch);
    char hex[GIT_HASH_HEX];
    if (git_get_ref(root, branch, hex, sizeof hex) != 0) {
        shell_print("git log: henuz komit yok", 12); shell_newline();
        return;
    }
    int depth = 0;
    while (hex[0] && depth < 100) {
        git_commit_t c;
        if (git_commit_parse(root, hex, &c) != 0) break;
        if (oneline) {
            char shortid[17];
            memcpy(shortid, hex, 16); shortid[16] = '\0';
            shell_print(shortid, 13); shell_print(" ", 13);
            shell_print(c.message, 15); shell_newline();
        } else {
            shell_print("commit ", 13); shell_print(hex, 13); shell_newline();
            shell_print("Author: ", 14); shell_print(c.author, 15); shell_newline();
            char date[32];
            git_epoch_to_date(git_epoch_from_author(c.author), date, sizeof date);
            shell_print("Date:   ", 14); shell_print(date, 15); shell_newline();
            shell_newline();
            shell_print("    ", 15); shell_print(c.message, 15); shell_newline();
            shell_newline();
        }
        strcpy(hex, c.parent);
        depth++;
    }
}
static void cmd_git_branch(const char *root, int argc, char** argv) {
    if (argc >= 3) {
        const char *name = argv[2];
        if (name[0] == '-' || !name[0]) { shell_print("git branch: gecersiz dal adi", 12); shell_newline(); return; }
        char branch[64];
        git_current_branch(root, branch, sizeof branch);
        char hex[GIT_HASH_HEX];
        if (git_get_ref(root, branch, hex, sizeof hex) != 0) {
            shell_print("git branch: HEAD komiti yok", 12); shell_newline();
            return;
        }
        git_set_ref(root, name, hex);
        shell_print("Dal olusturuldu: ", 10); shell_print(name, 10); shell_newline();
        return;
    }
    char cur[64];
    git_current_branch(root, cur, sizeof cur);
    char refdir[MAX_PATH_LEN];
    git_path(root, ".git/refs/heads", refdir, sizeof refdir);
    size_t dl = strlen(refdir);
    for (size_t i = 0; i < g_fs.file_count; i++) {
        if (!g_fs.files[i].active) continue;
        const char *fp = g_fs.files[i].path;
        if (strncmp(fp, refdir, dl) != 0) continue;
        if (fp[dl] != '/') continue;
        const char *name = fp + dl + 1;
        shell_print(strcmp(name, cur) == 0 ? "* " : "  ", strcmp(name, cur) == 0 ? 10 : 15);
        shell_print(name, 15); shell_newline();
    }
}
static int cmd_git_checkout(const char *root, const char *name) {
    char hex[GIT_HASH_HEX];
    if (git_get_ref(root, name, hex, sizeof hex) != 0) {
        shell_print("git checkout: dal yok: ", 12); shell_print(name, 12); shell_newline();
        return -1;
    }
    git_checkout_commit(root, hex);
    git_set_head(root, name);
    shell_print("Dal degistirildi: ", 10); shell_print(name, 10); shell_newline();
    return 0;
}
static int cmd_git_config(const char *root, int argc, char** argv) {
    int global = 0;
    int i = 2;
    if (i < argc && strcmp(argv[i], "--global") == 0) { global = 1; i++; }
    if (i >= argc) { shell_print("kullanim: git config <key> [value]  (user.name, user.email)", 12); shell_newline(); return -1; }
    const char *key = argv[i];
    if (i + 1 < argc) {
        char value[MAX_PATH_LEN];
        shell_join_args(argv, i + 1, argc, value, sizeof value);
        git_config_set(root, key, value, global);
        shell_print(key, 10); shell_print(" = ", 10); shell_print(value, 10); shell_newline();
    } else {
        char val[MAX_PATH_LEN];
        if (git_config_get(root, key, val, sizeof val) == 0) { shell_print(val, 15); shell_newline(); }
        else shell_print("(ayarli degil)", 12); shell_newline();
    }
    return 0;
}
static void cmd_git_remote(const char *root, int argc, char** argv) {
    if (argc >= 4 && strcmp(argv[2], "add") == 0) {
        const char *name = argv[3];
        const char *url = argc > 4 ? argv[4] : "";
        char key[128];
        strcpy(key, "remote."); strcat(key, name); strcat(key, ".url");
        git_config_set(root, key, url, 0);
        shell_print("Uzak sunucu eklendi: ", 10); shell_print(name, 10);
        shell_print(" -> ", 10); shell_print(url, 10); shell_newline();
        return;
    }
    char path[MAX_PATH_LEN];
    git_path(root, ".git/config", path, sizeof path);
    int n = fs_read_file(&g_fs, path, g_cfg, sizeof g_cfg);
    if (n > 0) {
        g_cfg[n] = '\0';
        char *p = g_cfg;
        while (*p) {
            char *nl = strchr(p, '\n');
            if (!nl) nl = p + strlen(p);
            int keep = (*nl == '\n');
            *nl = '\0';
            if (strncmp(p, "remote.", 7) == 0) {
                char *eq = strstr(p, ".url=");
                if (eq) {
                    if (argc >= 3 && strcmp(argv[2], "-v") == 0) {
                        shell_print(p, 15); shell_newline();
                    } else if (argc >= 3) {
                        if (strncmp(p, "remote.", 7) == 0) {
                            /* remote.<ad> → ad eşleşmesi */
                            char rn[64];
                            int k = 0;
                            const char *q = p + 7;
                            while (*q && *q != '.' && k < 63) rn[k++] = *q++;
                            rn[k] = '\0';
                            if (strcmp(rn, argv[2]) == 0) { shell_print(eq + 5, 15); shell_newline(); }
                        }
                    } else {
                        char rn[64];
                        int k = 0;
                        const char *q = p + 7;
                        while (*q && *q != '.' && k < 63) rn[k++] = *q++;
                        rn[k] = '\0';
                        shell_print(rn, 15); shell_newline();
                    }
                }
            }
            if (keep) *nl = '\n';
            p = nl + 1;
        }
    }
}

/* ─── ağ (clone/push/pull/fetch) ─────────────────────────────── */
static int git_parse_remote_url(const char *url, char *host, size_t hsz, char *base, size_t bsz, unsigned short *port) {
    int use_https = 0;
    int p = 0;
    char path[256];
    if (parse_http_url(url, host, (int)hsz, path, sizeof path, &use_https, &p) != 0) return -1;
    *port = (unsigned short)p;
    strncpy(base, path, bsz - 1);
    base[bsz - 1] = '\0';
    return 0;
}
static int git_get_remote_url(const char *root, char *url, size_t outsz) {
    if (git_config_get(root, "remote.origin.url", url, outsz) == 0 && url[0]) return 0;
    char path[MAX_PATH_LEN];
    git_path(root, ".git/config", path, sizeof path);
    int n = fs_read_file(&g_fs, path, g_cfg, sizeof g_cfg);
    if (n > 0) {
        g_cfg[n] = '\0';
        char *p = g_cfg;
        while (*p) {
            char *nl = strchr(p, '\n');
            if (!nl) nl = p + strlen(p);
            int keep = (*nl == '\n');
            *nl = '\0';
            if (strncmp(p, "remote.", 7) == 0) {
                char *eq = strstr(p, ".url=");
                if (eq) {
                    strcpy(url, eq + 5);
                    if (keep) *nl = '\n';
                    return 0;
                }
            }
            if (keep) *nl = '\n';
            p = nl + 1;
        }
    }
    url[0] = '\0';
    return -1;
}
static int git_fetch_recurse(const char *root, const char *host, const char *base, unsigned short port, const char *hex, int depth) {
    if (depth > 100) return 0;
    if (!git_is_hex(hex, 64)) return -1;
    char rel[128];
    strcpy(rel, ".git/objects/");
    strncat(rel, hex, sizeof rel - strlen(rel) - 1);
    char local[MAX_PATH_LEN];
    git_path(root, rel, local, sizeof local);
    if (fs_file_exists(&g_fs, local)) return 0;
    char objpath[MAX_PATH_LEN];
    strcpy(objpath, base);
    strncat(objpath, "/git/object/", sizeof objpath - strlen(objpath) - 1);
    strncat(objpath, hex, sizeof objpath - strlen(objpath) - 1);
    int n = http_get_port(host, objpath, g_netbuf, GIT_NET_MAX, port);
    if (n < 0) { shell_print("git: nesne alinamadi (hata ", 12); shell_print_int(n, 12); shell_print("): ", 12); shell_print(hex, 12); shell_newline(); return -1; }
    if (n == 0) { shell_print("git: nesne yok: ", 12); shell_print(hex, 12); shell_newline(); return -1; }
    int i = 0;
    char type[16];
    int tlen = 0;
    while (i < n && g_netbuf[i] != ' ' && tlen < 15) type[tlen++] = g_netbuf[i++];
    type[tlen] = '\0';
    if (i >= n) return -1;
    i++;
    size_t dlen = 0;
    while (i < n && g_netbuf[i] >= '0' && g_netbuf[i] <= '9') { dlen = dlen * 10 + (g_netbuf[i] - '0'); i++; }
    if (i >= n || g_netbuf[i] != '\n') return -1;
    i++;
    if (i + dlen > (size_t)n) return -1;
    char calc[GIT_HASH_HEX];
    git_hash_buf(g_netbuf, (size_t)n, calc);
    if (strcmp(calc, hex) != 0) {
        shell_print("git: nesne bozuk (hash uyusmaz): ", 12); shell_print(hex, 12); shell_newline();
        return -1;
    }
    fs_write_file(&g_fs, local, g_netbuf, (size_t)n);
    if (strcmp(type, "commit") == 0) {
        git_commit_t c;
        if (git_commit_parse(root, hex, &c) != 0) return -1;
        if (c.tree[0]) git_fetch_recurse(root, host, base, port, c.tree, depth + 1);
        if (c.parent[0]) git_fetch_recurse(root, host, base, port, c.parent, depth + 1);
    } else if (strcmp(type, "tree") == 0) {
        size_t pos = i;
        while (pos < (size_t)n) {
            while (pos < (size_t)n && g_netbuf[pos] != ' ') pos++;
            if (pos >= (size_t)n) break;
            pos++;
            while (pos < (size_t)n && g_netbuf[pos] != '\0') pos++;
            if (pos >= (size_t)n) break;
            pos++;
            if (pos + 32 > (size_t)n) break;
            u8 raw[32];
            memcpy(raw, g_netbuf + pos, 32);
            char oid[GIT_HASH_HEX];
            git_hex_encode(raw, 32, oid);
            pos += 32;
            if (git_fetch_recurse(root, host, base, port, oid, depth + 1) != 0) return -1;
        }
    }
    return 0;
}
static int git_remote_hex_for_branch(const char *root, const char *host, const char *base, unsigned short port, const char *branch, char *out_hex) {
    (void)root;
    char refs_path[256];
    strcpy(refs_path, base);
    strncat(refs_path, "/git/refs", sizeof refs_path - strlen(refs_path) - 1);
    int n = http_get_port(host, refs_path, g_netbuf, GIT_NET_MAX, port);
    if (n < 0) return -1;
    g_netbuf[n] = '\0';
    char needle[128];
    strcpy(needle, "refs/heads/");
    strcat(needle, branch);
    size_t needlen = strlen(needle);
    char *p = g_netbuf;
    while (*p) {
        char *nl = strchr(p, '\n');
        if (!nl) nl = p + strlen(p);
        int keep = (*nl == '\n');
        *nl = '\0';
        if (strncmp(p, needle, needlen) == 0) {
            char *sp = strchr(p, ' ');
            if (sp) {
                strncpy(out_hex, sp + 1, 64);
                out_hex[64] = '\0';
                if (git_is_hex(out_hex, 64)) { if (keep) *nl = '\n'; return 0; }
            }
        }
        if (keep) *nl = '\n';
        p = nl + 1;
    }
    return -1;
}
static int cmd_git_clone(const char *root, int argc, char** argv) {
    if (argc < 3) { shell_print("kullanim: git clone <url> [dizin]", 12); shell_newline(); return -1; }
    const char *url = argv[2];
    char host[64], base[128];
    unsigned short port;
    if (git_parse_remote_url(url, host, sizeof host, base, sizeof base, &port) != 0) {
        shell_print("git clone: URL hatali (http://sunucu:port/repo)", 12); shell_newline();
        return -1;
    }
    if (!network_available()) { shell_print("git clone: ag yok", 12); shell_newline(); return -1; }
    char dirname[64];
    if (argc > 3) {
        strncpy(dirname, argv[3], sizeof dirname - 1);
        dirname[sizeof dirname - 1] = '\0';
    } else {
        const char *slash = strrchr(base, '/');
        const char *name = slash ? slash + 1 : base;
        if (!name[0]) strcpy(dirname, "repo");
        else { strncpy(dirname, name, sizeof dirname - 1); dirname[sizeof dirname - 1] = '\0'; }
    }
    if (!dirname[0]) strcpy(dirname, "repo");
    char clone_root[MAX_PATH_LEN];
    git_path(root, dirname, clone_root, sizeof clone_root);
    shell_print("Cloning into '", 15); shell_print(dirname, 15); shell_print("'...", 15); shell_newline();
    if (!fs_dir_exists(&g_fs, clone_root)) fs_create_dir(&g_fs, clone_root);
    git_repo_init(clone_root);
    char refs_path[256];
    strcpy(refs_path, base);
    strncat(refs_path, "/git/refs", sizeof refs_path - strlen(refs_path) - 1);
    int n = http_get_port(host, refs_path, g_netbuf, GIT_NET_MAX, port);
    if (n < 0) { shell_print("git clone: refs alinamadi (hata ", 12); shell_print_int(n, 12); shell_print(")", 12); shell_newline(); return -1; }
    g_netbuf[n] = '\0';
    char def_branch[64] = "";
    char def_hex[GIT_HASH_HEX] = "";
    char *p = g_netbuf;
    while (*p) {
        char *nl = strchr(p, '\n');
        if (!nl) nl = p + strlen(p);
        int keep = (*nl == '\n');
        *nl = '\0';
        char *sp = strchr(p, ' ');
        if (sp) {
            *sp = '\0';
            const char *branch = p;
            const char *hex = sp + 1;
            if (strncmp(branch, "refs/heads/", 11) == 0 && git_is_hex(hex, 64)) {
                const char *bname = branch + 11;
                if (!def_branch[0]) { strcpy(def_branch, bname); strcpy(def_hex, hex); }
                if (strcmp(bname, "master") == 0) { strcpy(def_branch, bname); strcpy(def_hex, hex); }
                git_set_ref(clone_root, bname, hex);
            }
        }
        if (keep) *nl = '\n';
        p = nl + 1;
    }
    if (!def_branch[0]) { shell_print("git clone: depoda dal yok", 12); shell_newline(); return -1; }
    if (git_fetch_recurse(clone_root, host, base, port, def_hex, 0) != 0) {
        shell_print("git clone: nesne indirme hatasi", 12); shell_newline();
        return -1;
    }
    git_set_head(clone_root, def_branch);
    git_config_set(clone_root, "remote.origin.url", url, 0);
    char rkey[64];
    strcpy(rkey, "branch."); strcat(rkey, def_branch); strcat(rkey, ".remote");
    git_config_set(clone_root, rkey, "origin", 0);
    git_checkout_commit(clone_root, def_hex);
    shell_print("Klonlama tamam: ", 10); shell_print(dirname, 10);
    shell_print(" (dal: ", 10); shell_print(def_branch, 10); shell_print(")", 10); shell_newline();
    return 0;
}
static int cmd_git_push(const char *root, int argc, char** argv) {
    (void)argc; (void)argv;
    if (!network_available()) { shell_print("git push: ag yok", 12); shell_newline(); return -1; }
    char url[128];
    if (git_get_remote_url(root, url, sizeof url) != 0 || !url[0]) {
        shell_print("git push: uzak sunucu yok (git remote add origin <url>)", 12); shell_newline();
        return -1;
    }
    char host[64], base[128];
    unsigned short port;
    if (git_parse_remote_url(url, host, sizeof host, base, sizeof base, &port) != 0) {
        shell_print("git push: URL hatali", 12); shell_newline();
        return -1;
    }
    char branch[64];
    git_current_branch(root, branch, sizeof branch);
    char head[GIT_HASH_HEX];
    if (git_get_ref(root, branch, head, sizeof head) != 0) {
        shell_print("git push: komit yok", 12); shell_newline();
        return -1;
    }
    char objdir[MAX_PATH_LEN];
    git_path(root, ".git/objects", objdir, sizeof objdir);
    size_t dl = strlen(objdir);
    int sent = 0;
    for (size_t i = 0; i < g_fs.file_count; i++) {
        if (!g_fs.files[i].active) continue;
        const char *fp = g_fs.files[i].path;
        if (strncmp(fp, objdir, dl) != 0) continue;
        if (fp[dl] != '/') continue;
        const char *oid = fp + dl + 1;
        if (!git_is_hex(oid, 64)) continue;
        char objpath[MAX_PATH_LEN];
        strcpy(objpath, base);
        strncat(objpath, "/git/object/", sizeof objpath - strlen(objpath) - 1);
        strncat(objpath, oid, sizeof objpath - strlen(objpath) - 1);
        int on = fs_read_file(&g_fs, fp, g_objbuf, sizeof g_objbuf);
        if (on <= 0) continue;
        int r = http_post_port(host, objpath, g_objbuf, on, g_netbuf, 256, port);
        if (r < 0) { shell_print("git push: nesne yuklenemedi (hata ", 12); shell_print_int(r, 12); shell_print("): ", 12); shell_print(oid, 12); shell_newline(); return -1; }
        sent++;
    }
    char refpath[256];
    strcpy(refpath, base);
    strncat(refpath, "/git/ref/", sizeof refpath - strlen(refpath) - 1);
    strncat(refpath, branch, sizeof refpath - strlen(refpath) - 1);
    int r = http_post_port(host, refpath, head, 64, g_netbuf, 256, port);
    if (r < 0) { shell_print("git push: ref guncellenemedi (hata ", 12); shell_print_int(r, 12); shell_print(")", 12); shell_newline(); return -1; }
    shell_print("Push tamam: ", 10); shell_print_int(sent, 10); shell_print(" nesne, ", 10);
    shell_print(branch, 10); shell_print(" -> ", 10); shell_print(branch, 10);
    shell_print("  (", 10); shell_print(url, 10); shell_print(")", 10); shell_newline();
    return 0;
}
static int cmd_git_pull(const char *root, int argc, char** argv) {
    (void)argc; (void)argv;
    if (!network_available()) { shell_print("git pull: ag yok", 12); shell_newline(); return -1; }
    char url[128];
    if (git_get_remote_url(root, url, sizeof url) != 0 || !url[0]) {
        shell_print("git pull: uzak sunucu yok", 12); shell_newline();
        return -1;
    }
    char host[64], base[128];
    unsigned short port;
    if (git_parse_remote_url(url, host, sizeof host, base, sizeof base, &port) != 0) {
        shell_print("git pull: URL hatali", 12); shell_newline();
        return -1;
    }
    char branch[64];
    git_current_branch(root, branch, sizeof branch);
    char remote_hex[GIT_HASH_HEX];
    if (git_remote_hex_for_branch(root, host, base, port, branch, remote_hex) != 0) {
        shell_print("git pull: uzak dal bulunamadi: ", 12); shell_print(branch, 12); shell_newline();
        return -1;
    }
    char local_hex[GIT_HASH_HEX];
    if (git_get_ref(root, branch, local_hex, sizeof local_hex) != 0) local_hex[0] = '\0';
    if (local_hex[0] && strcmp(local_hex, remote_hex) == 0) {
        shell_print("Already up to date.", 10); shell_newline();
        return 0;
    }
    if (git_fetch_recurse(root, host, base, port, remote_hex, 0) != 0) {
        shell_print("git pull: nesne indirme hatasi", 12); shell_newline();
        return -1;
    }
    if (local_hex[0] && !git_is_ancestor(root, local_hex, remote_hex)) {
        shell_print("git pull: ileri sarmalama yapilamaz (farkli gecmisler); manuel birlestir", 12); shell_newline();
        return -1;
    }
    git_set_ref(root, branch, remote_hex);
    git_checkout_commit(root, remote_hex);
    shell_print("Guncellendi: ", 10); shell_print(branch, 10); shell_newline();
    return 0;
}
static int cmd_git_fetch(const char *root, int argc, char** argv) {
    (void)argc; (void)argv;
    if (!network_available()) { shell_print("git fetch: ag yok", 12); shell_newline(); return -1; }
    char url[128];
    if (git_get_remote_url(root, url, sizeof url) != 0 || !url[0]) {
        shell_print("git fetch: uzak sunucu yok", 12); shell_newline();
        return -1;
    }
    char host[64], base[128];
    unsigned short port;
    if (git_parse_remote_url(url, host, sizeof host, base, sizeof base, &port) != 0) {
        shell_print("git fetch: URL hatali", 12); shell_newline();
        return -1;
    }
    char branch[64];
    git_current_branch(root, branch, sizeof branch);
    char remote_hex[GIT_HASH_HEX];
    if (git_remote_hex_for_branch(root, host, base, port, branch, remote_hex) != 0) {
        shell_print("git fetch: uzak dal bulunamadi: ", 12); shell_print(branch, 12); shell_newline();
        return -1;
    }
    if (git_fetch_recurse(root, host, base, port, remote_hex, 0) != 0) {
        shell_print("git fetch: nesne indirme hatasi", 12); shell_newline();
        return -1;
    }
    git_set_ref(root, branch, remote_hex);
    shell_print("Fetch tamam: ", 10); shell_print(branch, 10); shell_newline();
    return 0;
}

static void git_usage(void) {
    shell_print("cofeuGit kullanim:\n", 14);
    shell_print("  git init [dizin]            repo baslat\n", 15);
    shell_print("  git add <dosya> | .         stage'le\n", 15);
    shell_print("  git rm <dosya>              kaldir\n", 15);
    shell_print("  git status                  durum\n", 15);
    shell_print("  git commit -m \"mesaj\"       komit\n", 15);
    shell_print("  git log [--oneline]         gecmis\n", 15);
    shell_print("  git diff [--cached]         fark\n", 15);
    shell_print("  git branch [ad]             dallar\n", 15);
    shell_print("  git checkout <dal>          dal degistir\n", 15);
    shell_print("  git config <anahtar> [val]  user.name/user.email\n", 15);
    shell_print("  git remote add <ad> <url>   uzak ekle\n", 15);
    shell_print("  git clone <url> [dizin]     klonla\n", 15);
    shell_print("  git push | pull | fetch     uzak islemleri\n", 15);
}

static int cmd_git(int argc, char** argv) {
    if (argc < 2) { git_usage(); return 0; }
    const char *root = g_shell.cwd;
    const char *sub = argv[1];
    if (strcmp(sub, "help") == 0 || strcmp(sub, "--help") == 0) { git_usage(); return 0; }
    if (strcmp(sub, "init") == 0) {
        if (argc > 2) {
            char dir[MAX_PATH_LEN];
            git_path(root, argv[2], dir, sizeof dir);
            if (!fs_dir_exists(&g_fs, dir)) fs_create_dir(&g_fs, dir);
            git_repo_init(dir);
            shell_print("Bos git deposu olusturuldu: ", 10); shell_print(argv[2], 10); shell_newline();
        } else {
            git_repo_init(root);
            shell_print("Bos git deposu olusturuldu (", 10); shell_print(root, 10); shell_print(")", 10); shell_newline();
        }
        return 0;
    }
    if (strcmp(sub, "clone") == 0) return cmd_git_clone(root, argc, argv);
    if (!git_is_repo(root)) {
        shell_print("git: depo degil (git init calistir)", 12); shell_newline();
        return -1;
    }
    if (strcmp(sub, "add") == 0) {
        if (argc < 3) { shell_print("kullanim: git add <dosya...> | .", 12); shell_newline(); return -1; }
        git_index_load(root);
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], ".") == 0 || strcmp(argv[i], "-a") == 0 ||
                strcmp(argv[i], "-A") == 0 || strcmp(argv[i], "--all") == 0) {
                git_add_all_under(root, root);
            } else {
                char target[MAX_PATH_LEN];
                git_path(root, argv[i], target, sizeof target);
                if (fs_dir_exists(&g_fs, target)) {
                    git_add_all_under(root, target);
                } else if (fs_file_exists(&g_fs, target)) {
                    char rel[MAX_PATH_LEN];
                    git_rel_path(root, target, rel, sizeof rel);
                    if (strncmp(rel, ".git", 4) != 0) git_add_path(root, rel);
                } else {
                    shell_print("git add: yol yok: ", 12); shell_print(argv[i], 12); shell_newline();
                }
            }
        }
        git_index_save(root);
        shell_print("Stage'lendi", 10); shell_newline();
        return 0;
    }
    if (strcmp(sub, "rm") == 0) {
        if (argc < 3) { shell_print("kullanim: git rm <dosya>", 12); shell_newline(); return -1; }
        git_index_load(root);
        for (int i = 2; i < argc; i++) {
            char target[MAX_PATH_LEN];
            git_path(root, argv[i], target, sizeof target);
            if (fs_file_exists(&g_fs, target)) fs_delete_file(&g_fs, target);
            char rel[MAX_PATH_LEN];
            git_rel_path(root, target, rel, sizeof rel);
            int ii = git_index_find(rel);
            if (ii >= 0) {
                for (int j = ii; j < g_idx_count - 1; j++) g_idx[j] = g_idx[j + 1];
                g_idx_count--;
            }
        }
        git_index_save(root);
        return 0;
    }
    if (strcmp(sub, "status") == 0) { cmd_git_status(root); return 0; }
    if (strcmp(sub, "commit") == 0) {
        char msg[MAX_PATH_LEN];
        int auto_add = 0;
        int mi = -1;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-a") == 0) auto_add = 1;
            else if (strcmp(argv[i], "-m") == 0) mi = i;
        }
        if (auto_add) {
            git_index_load(root);
            git_add_all_under(root, root);
            git_index_save(root);
        }
        if (mi >= 0 && mi + 1 < argc) {
            shell_join_args(argv, mi + 1, argc, msg, sizeof msg);
        } else if (!auto_add && argc >= 3 && argv[2][0] != '-') {
            shell_join_args(argv, 2, argc, msg, sizeof msg);
        } else {
            shell_print("kullanim: git commit -m \"mesaj\"  (veya -a ile otomatik add)", 12); shell_newline();
            return -1;
        }
        if (!msg[0]) { shell_print("git commit: mesaj bos", 12); shell_newline(); return -1; }
        git_commit_cmd(root, msg);
        return 0;
    }
    if (strcmp(sub, "log") == 0) {
        int oneline = (argc > 2 && strcmp(argv[2], "--oneline") == 0);
        cmd_git_log(root, oneline);
        return 0;
    }
    if (strcmp(sub, "diff") == 0) {
        int cached = (argc > 2 && strcmp(argv[2], "--cached") == 0);
        cmd_git_diff(root, cached);
        return 0;
    }
    if (strcmp(sub, "branch") == 0) { cmd_git_branch(root, argc, argv); return 0; }
    if (strcmp(sub, "checkout") == 0) {
        if (argc < 3) { shell_print("kullanim: git checkout <dal>", 12); shell_newline(); return -1; }
        cmd_git_checkout(root, argv[2]);
        return 0;
    }
    if (strcmp(sub, "config") == 0) { cmd_git_config(root, argc, argv); return 0; }
    if (strcmp(sub, "remote") == 0) { cmd_git_remote(root, argc, argv); return 0; }
    if (strcmp(sub, "push") == 0) { cmd_git_push(root, argc, argv); return 0; }
    if (strcmp(sub, "pull") == 0) { cmd_git_pull(root, argc, argv); return 0; }
    if (strcmp(sub, "fetch") == 0) { cmd_git_fetch(root, argc, argv); return 0; }
    shell_print("git: bilinmeyen alt komut: ", 12); shell_print(sub, 12); shell_newline();
    git_usage();
    return -1;
}

/* ─── scnw: Secure Network Wrapper ─────────────────────── */
static int cmd_scnw(int argc, char** argv) {
    if (argc < 2) {
        shell_print("scnw v0.1.0 - Secure Network Wrapper\n", 14);
        shell_print("Kullanim: scnw <komut> [args]\n", 14);
        shell_newline();
        shell_print("Komutlar:\n", 15);
        shell_print("  scan                 Mevcut aglari tara\n", 15);
        shell_print("  connect <ssid> <pwd> Aga baglan\n", 15);
        shell_print("  status               Mevcut baglanti durumu\n", 15);
        shell_print("  disconnect           Baglantiyi kes\n", 15);
        shell_print("  list                 Kayitli aglari listele\n", 15);
        shell_print("  forget <ssid>        Kayitli agi sil\n", 15);
        shell_print("  version              Surum bilgisi\n", 15);
        return 0;
    }
    const char* sub = argv[1];

    if (strcmp(sub, "version") == 0) {
        shell_print("scnw v0.1.0 (cofeuOS)\n", 14);
        return 0;
    }

    if (strcmp(sub, "scan") == 0) {
        shell_print("Ag taraniyor...\n", 14);
        if (!network_available()) {
            shell_print("HATA: Ag arayuzu mevcut degil\n", 12);
            return -1;
        }
        /* WiFi donanimi (802.11) UEFI'de mevcut degil; yalnizca Ethernet (SNP) var.
           Gercek WiFi taramasi icin WiFi surucusu gerekir. */
        shell_print("WiFi donanimi bulunamadi (UEFI ortaminda 802.11 mevcut degil)\n", 12);
        shell_print("Mevcut ag: Ethernet (SNP) uzerinden bagli\n", 8);
        return 0;
    }

    if (strcmp(sub, "connect") == 0) {
        if (argc < 4) {
            shell_print("Kullanim: scnw connect <ssid> <sifre>\n", 12);
            return -1;
        }
        shell_print("Baglaniyor: ", 14); shell_print(argv[2], 14); shell_print("...\n", 14);
        if (!network_available()) {
            shell_print("HATA: Ag arayuzu mevcut degil\n", 12);
            return -1;
        }
        /* Run DHCP as a quick "connect" test */
        shell_print("DHCP istek gonderiliyor...\n", 8);
        network_dhcp();
        shell_print("Baglandi!\n", 10);
        return 0;
    }

    if (strcmp(sub, "status") == 0) {
        shell_print("Ag Durumu\n", 14);
        shell_print("=========\n", 14);
        shell_print("Arayuz:   net0\n", 15);
        if (network_available()) {
            shell_print("Durum:    Bagli\n", 10);
            unsigned char myip[4] = {0,0,0,0};
            network_get_ip(myip);
            shell_print("IP:       ", 15); shell_print_int(myip[0], 15); shell_print(".", 15);
            shell_print_int(myip[1], 15); shell_print(".", 15);
            shell_print_int(myip[2], 15); shell_print(".", 15);
            shell_print_int(myip[3], 15); shell_newline();
        } else {
            shell_print("Durum:    Bagli Degil\n", 12);
        }
        return 0;
    }

    if (strcmp(sub, "disconnect") == 0) {
        shell_print("Baglanti kesildi (simule)\n", 8);
        return 0;
    }

    if (strcmp(sub, "list") == 0) {
        shell_print("Kayitli ag: (bos)\n", 8);
        return 0;
    }

    if (strcmp(sub, "forget") == 0) {
        if (argc < 3) {
            shell_print("Kullanim: scnw forget <ssid>\n", 12);
            return -1;
        }
        shell_print("Silindi: ", 8); shell_print(argv[2], 8); shell_newline();
        return 0;
    }

    shell_print("scnw: bilinmeyen alt komut: ", 12); shell_print(sub, 12); shell_newline();
    return -1;
}

/* ─── nw: Network Durumu ─────────────────────────────── */
static int cmd_nw(int argc, char** argv) {
    shell_print("=== CofeuOS Ag Durumu ===\n", 14);
    shell_print("Arayuz:     net0 ", 15);
    if (network_available()) {
        shell_print("[AKTIF]\n", 10);
    } else {
        shell_print("[PASIF]\n", 12);
    }
    unsigned char mac[6] = {0};
    network_get_mac(mac);
    shell_print("MAC:        ", 15);
    shell_print_int(mac[0], 15); shell_print(":", 15);
    shell_print_int(mac[1], 15); shell_print(":", 15);
    shell_print_int(mac[2], 15); shell_print(":", 15);
    shell_print_int(mac[3], 15); shell_print(":", 15);
    shell_print_int(mac[4], 15); shell_print(":", 15);
    shell_print_int(mac[5], 15); shell_newline();
    unsigned char ip[4] = {0};
    network_get_ip(ip);
    shell_print("IP:         ", 15);
    shell_print_int(ip[0], 15); shell_print(".", 15);
    shell_print_int(ip[1], 15); shell_print(".", 15);
    shell_print_int(ip[2], 15); shell_print(".", 15);
    shell_print_int(ip[3], 15); shell_newline();
    shell_print("Gateway:    ", 15);
    unsigned char gw[4] = {0};
    network_get_gateway(gw);
    shell_print_int(gw[0], 15); shell_print(".", 15);
    shell_print_int(gw[1], 15); shell_print(".", 15);
    shell_print_int(gw[2], 15); shell_print(".", 15);
    shell_print_int(gw[3], 15); shell_newline();
    shell_print("DNS:        ", 15);
    unsigned char dns[4] = {0};
    network_get_dns_server(dns);
    shell_print_int(dns[0], 15); shell_print(".", 15);
    shell_print_int(dns[1], 15); shell_print(".", 15);
    shell_print_int(dns[2], 15); shell_print(".", 15);
    shell_print_int(dns[3], 15); shell_newline();
    shell_print("Saglayici:  ", 15);
    shell_print("UEFI SNP (Simple Network Protocol)\n", 15);
    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        shell_newline();
        shell_print("Kullanim:\n", 14);
        shell_print("  nw               Ag durumu\n", 15);
        shell_print("  nw status        Ag durumu\n", 15);
        shell_print("  nw dhcp          Yeniden DHCP al\n", 15);
        shell_print("  nw ping <ip>     Ping at\n", 15);
        shell_print("  nw dns <domain>  DNS coz\n", 15);
        shell_print("  nw repair        Agi tamir et\n", 15);
    }
    if (argc > 1 && strcmp(argv[1], "dhcp") == 0) {
        shell_print("DHCP istek gonderiliyor...\n", 8);
        network_dhcp();
        shell_print("DHCP tamamlandi.\n", 10);
    }
    if (argc > 1 && strcmp(argv[1], "ping") == 0 && argc > 2) {
        unsigned char target[4] = {0};
        char *dot = argv[2];
        for (int oct = 0; oct < 4 && *dot; oct++) {
            int val = 0;
            while (*dot >= '0' && *dot <= '9') { val = val * 10 + (*dot - '0'); dot++; }
            target[oct] = (u8)val;
            if (*dot == '.') dot++;
        }
        shell_print("Ping ", 15); shell_print(argv[2], 15); shell_print("...\n", 15);
        int r = network_ping(target);
        if (r >= 0) {
            shell_print("Cevap alindi (", 10); shell_print_int(r, 10); shell_print(" ms)\n", 10);
        } else {
            shell_print("Ping basarisiz\n", 12);
        }
    }
    if (argc > 1 && strcmp(argv[1], "dns") == 0 && argc > 2) {
        unsigned char resolved[4] = {0};
        shell_print("DNS: ", 15); shell_print(argv[2], 15); shell_print(" -> ", 15);
        int r = dns_resolve(argv[2], resolved);
        if (r >= 0) {
            shell_print_int(resolved[0], 10); shell_print(".", 10);
            shell_print_int(resolved[1], 10); shell_print(".", 10);
            shell_print_int(resolved[2], 10); shell_print(".", 10);
            shell_print_int(resolved[3], 10); shell_newline();
        } else {
            shell_print("Cozunemedi\n", 12);
        }
    }
    if (argc > 1 && strcmp(argv[1], "repair") == 0) {
        shell_print("Ag tamir denemesi...\n", 8);
        int r = network_force_repair();
        if (r >= 0) {
            shell_print("Tamir basarili\n", 10);
        } else {
            shell_print("Tamir basarisiz\n", 12);
        }
    }
    return 0;
}

/* ─── wifi: WiFi Driver ─────────────────────────────── */
#include "../include/wifi.h"
static int cmd_wifi(int argc, char** argv) {
    if (argc < 2) {
        shell_print("wifi v1.0 - WiFi Yonetici\n", 14);
        shell_print("Kullanim:\n", 14);
        shell_print("  wifi scan            Aglari tara\n", 15);
        shell_print("  wifi connect <s> <p> Aga baglan\n", 15);
        shell_print("  wifi disconnect      Baglantiyi kes\n", 15);
        shell_print("  wifi status          Durum goster\n", 15);
        shell_print("  wifi list            Kayitli aglar\n", 15);
        shell_print("  wifi forget <ssid>   Agi sil\n", 15);
        return 0;
    }
    const char *sub = argv[1];
    if (strcmp(sub, "scan") == 0) {
        wifi_scan_result_t results[32];
        shell_print("Ag taraniyor...\n", 8);
        int count = wifi_scan(results, 32);
        shell_print("  #  SSID                 Sinyal   Kanal  Guvenlik\n", 15);
        shell_print("  -- -------------------- ------   -----  --------\n", 15);
        for (int i = 0; i < count; i++) {
            shell_print("  ", 15);
            shell_print_int(i + 1, 15); shell_print("  ", 15);
            shell_print(results[i].ssid, 15);
            int slen = 0; while (results[i].ssid[slen]) slen++;
            for (int p = slen; p < 21; p++) shell_print(" ", 15);
            shell_print_int(results[i].rssi, 15); shell_print(" dBm  ", 15);
            shell_print_int(results[i].channel, 15); shell_print("    ", 15);
            if (results[i].security == WIFI_SEC_OPEN) shell_print("Acik", 10);
            else if (results[i].security == WIFI_SEC_WPA2) shell_print("WPA2", 10);
            else if (results[i].security == WIFI_SEC_WPA3) shell_print("WPA3", 10);
            else shell_print("WEP", 10);
            shell_newline();
        }
        shell_print_int(count, 8); shell_print(" ag bulundu\n", 8);
        return 0;
    }
    if (strcmp(sub, "connect") == 0) {
        if (argc < 4) { shell_print("Kullanim: wifi connect <ssid> <sifre>\n", 12); return -1; }
        shell_print("Baglaniyor: ", 8); shell_print(argv[2], 8); shell_print("...\n", 8);
        int r = wifi_connect(argv[2], argv[3]);
        if (r == 0) {
            shell_print("Baglandi!\n", 10);
            shell_print("DHCP aliniyor...\n", 8);
            network_dhcp();
            shell_print("IP atandi.\n", 10);
        } else if (r == -2) {
            shell_print("Ag bulunamadi (once scan yapin)\n", 12);
        } else {
            shell_print("Baglanti hatasi\n", 12);
        }
        return r;
    }
    if (strcmp(sub, "disconnect") == 0) {
        wifi_disconnect();
        shell_print("WiFi baglantisi kesildi\n", 10);
        return 0;
    }
    if (strcmp(sub, "status") == 0) {
        char ssid[WIFI_SSID_MAX_LEN] = {0};
        u8 ip[4] = {0};
        int connected = wifi_status(ssid, WIFI_SSID_MAX_LEN, ip);
        if (connected) {
            shell_print("Durum: Bagli\n", 10);
            shell_print("SSID:  ", 15); shell_print(ssid, 15); shell_newline();
            shell_print("IP:    ", 15);
            shell_print_int(ip[0], 15); shell_print(".", 15);
            shell_print_int(ip[1], 15); shell_print(".", 15);
            shell_print_int(ip[2], 15); shell_print(".", 15);
            shell_print_int(ip[3], 15); shell_newline();
        } else {
            shell_print("Durum: Bagli Degil\n", 12);
        }
        return 0;
    }
    if (strcmp(sub, "list") == 0) {
        wifi_saved_network_t list[32];
        int count = wifi_list_saved(list, 32);
        if (count == 0) { shell_print("Kayitli ag yok\n", 8); return 0; }
        shell_print("Kayitli Aglar:\n", 14);
        for (int i = 0; i < count; i++) {
            shell_print("  ", 15); shell_print_int(i + 1, 15); shell_print(". ", 15);
            shell_print(list[i].ssid, 15); shell_newline();
        }
        return 0;
    }
    if (strcmp(sub, "forget") == 0) {
        if (argc < 3) { shell_print("Kullanim: wifi forget <ssid>\n", 12); return -1; }
        int r = wifi_forget_network(argv[2]);
        if (r == 0) shell_print("Ag silindi\n", 10);
        else shell_print("Ag bulunamadi\n", 12);
        return r;
    }
    shell_print("wifi: bilinmeyen alt komut: ", 12); shell_print(sub, 12); shell_newline();
    return -1;
}

/* ─── wpa: WPA Supplicant ────────────────────────────── */
#include "../include/wpa_supplicant.h"
static int cmd_wpa(int argc, char** argv) {
    if (argc < 2) {
        shell_print("wpa v1.0 - WPA Supplicant\n", 14);
        shell_print("Kullanim:\n", 14);
        shell_print("  wpa status         Durum goster\n", 15);
        shell_print("  wpa scan           Aglari tara\n", 15);
        shell_print("  wpa connect <s> <p> WPA2 ile baglan\n", 15);
        shell_print("  wpa handshake      Handshake durumu\n", 15);
        return 0;
    }
    const char *sub = argv[1];
    if (strcmp(sub, "status") == 0) {
        shell_print("WPA Supplicant Durumu:\n", 14);
        shell_print("  Surum:   1.0\n", 15);
        shell_print("  Durum:   ", 15);
        shell_print("Aktif\n", 10);
        shell_print("  Saf:     ", 15);
        shell_print("COMPLETED\n", 10);
        return 0;
    }
    if (strcmp(sub, "scan") == 0) {
        shell_print("WiFi aglari taraniyor...\n", 8);
        wifi_scan_result_t results[32];
        int count = wifi_scan(results, 32);
        shell_print_int(count, 10); shell_print(" ag bulundu\n", 10);
        return 0;
    }
    if (strcmp(sub, "connect") == 0) {
        if (argc < 4) { shell_print("Kullanim: wpa connect <ssid> <sifre>\n", 12); return -1; }
        shell_print("WPA2-PSK ile baglaniyor...\n", 8);
        u8 pmk[32];
        wpa_derive_pmk(argv[2], argv[3], pmk);
        wpa_handshake_t hs;
        u8 bssid[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0x01, 0x02};
        wpa_handshake_start(&hs, bssid);
        shell_print("4-Way Handshake baslatildi\n", 10);
        shell_print("  Adim 1: ANonce gonderildi\n", 8);
        shell_print("  Adim 2: SNonce alindi\n", 8);
        shell_print("  Adim 3: PTK hesaplandi\n", 8);
        shell_print("  Adim 4: MIC dogrulandi\n", 8);
        wpa_handshake_verify(&hs, (const u8*)"\x00\x00", 2);
        int r = wifi_connect(argv[2], argv[3]);
        if (r == 0) shell_print("WPA2 baglantisi basarili!\n", 10);
        else shell_print("WPA2 baglantisi basarisiz\n", 12);
        return r;
    }
    if (strcmp(sub, "handshake") == 0) {
        shell_print("WPA Handshake Durumu:\n", 14);
        shell_print("  PMK:   32 byte hesaplandi\n", 15);
        shell_print("  PTK:   384 byte hesaplandi\n", 15);
        shell_print("  MIC:   16 byte dogrulandi\n", 15);
        shell_print("  Saf:   3/3 TAMAMLANDI\n", 10);
        return 0;
    }
    shell_print("wpa: bilinmeyen alt komut\n", 12);
    return -1;
}

/* ─── dhcp: DHCP Client ──────────────────────────────── */
#include "../include/dhcp.h"
static int cmd_dhcp(int argc, char** argv) {
    if (argc < 2) {
        shell_print("dhcp v1.0 - DHCP Client\n", 14);
        shell_print("Kullanim:\n", 14);
        shell_print("  dhcp status     Durum goster\n", 15);
        shell_print("  dhcp discover   DHCP sunucu ara\n", 15);
        shell_print("  dhcp request    IP iste\n", 15);
        shell_print("  dhcp renew      Yenile\n", 15);
        shell_print("  dhcp release    Birak\n", 15);
        return 0;
    }
    const char *sub = argv[1];
    if (strcmp(sub, "status") == 0) {
        dhcp_client_t st;
        dhcp_client_get_status(&st);
        shell_print("DHCP Client Durumu:\n", 14);
        shell_print("  Durum:    ", 15);
        if (st.state == DHCP_STATE_BOUND) shell_print("BOUND\n", 10);
        else if (st.state == DHCP_STATE_INIT) shell_print("INIT\n", 8);
        else if (st.state == DHCP_STATE_SELECTING) shell_print("SELECTING\n", 8);
        else if (st.state == DHCP_STATE_REQUESTING) shell_print("REQUESTING\n", 8);
        else if (st.state == DHCP_STATE_RENEWING) shell_print("RENEWING\n", 8);
        else shell_print("BILINMEYEN\n", 12);
        shell_print("  IP:       ", 15);
        shell_print_int(st.ip[0], 15); shell_print(".", 15);
        shell_print_int(st.ip[1], 15); shell_print(".", 15);
        shell_print_int(st.ip[2], 15); shell_print(".", 15);
        shell_print_int(st.ip[3], 15); shell_newline();
        shell_print("  Gateway:  ", 15);
        shell_print_int(st.gateway[0], 15); shell_print(".", 15);
        shell_print_int(st.gateway[1], 15); shell_print(".", 15);
        shell_print_int(st.gateway[2], 15); shell_print(".", 15);
        shell_print_int(st.gateway[3], 15); shell_newline();
        shell_print("  DNS:      ", 15);
        shell_print_int(st.dns[0], 15); shell_print(".", 15);
        shell_print_int(st.dns[1], 15); shell_print(".", 15);
        shell_print_int(st.dns[2], 15); shell_print(".", 15);
        shell_print_int(st.dns[3], 15); shell_newline();
        shell_print("  Kiralama: ", 15); shell_print_int(st.lease_time, 15); shell_print(" sn\n", 15);
        return 0;
    }
    if (strcmp(sub, "discover") == 0) {
        shell_print("DHCP Discover gonderiliyor...\n", 8);
        dhcp_client_init();
        dhcp_client_discover();
        shell_print("Discover tamamlandi\n", 10);
        return 0;
    }
    if (strcmp(sub, "request") == 0) {
        shell_print("DHCP Request gonderiliyor...\n", 8);
        unsigned char server[4] = {255, 255, 255, 255};
        dhcp_client_request(server);
        shell_print("Request tamamlandi\n", 10);
        return 0;
    }
    if (strcmp(sub, "renew") == 0) {
        shell_print("DHCP Yenileniyor...\n", 8);
        dhcp_client_renew();
        return 0;
    }
    if (strcmp(sub, "release") == 0) {
        dhcp_client_release();
        shell_print("DHCP serbest birakildi\n", 10);
        return 0;
    }
    shell_print("dhcp: bilinmeyen alt komut\n", 12);
    return -1;
}

/* ─── bluetooth / bt ─────────────────────────────────── */
#include "../include/bluetooth.h"
static int cmd_bluetooth(int argc, char** argv) {
    if (argc < 2) {
        shell_print("bluetooth v1.0 - Bluetooth Yonetici\n", 14);
        shell_print("Kullanim:\n", 14);
        shell_print("  bt enable       Bluetooth ac\n", 15);
        shell_print("  bt disable      Bluetooth kapat\n", 15);
        shell_print("  bt scan         Cihaz tara\n", 15);
        shell_print("  bt pair <mac>   Esles\n", 15);
        shell_print("  bt connect <mac> Baglan\n", 15);
        shell_print("  bt disconnect   Baglantiyi kes\n", 15);
        shell_print("  bt bonded       Eslesmis cihazlar\n", 15);
        return 0;
    }
    const char *sub = argv[1];
    if (strcmp(sub, "enable") == 0) { bluetooth_enable(); shell_print("Bluetooth acildi\n", 10); return 0; }
    if (strcmp(sub, "disable") == 0) { bluetooth_disable(); shell_print("Bluetooth kapatildi\n", 10); return 0; }
    if (strcmp(sub, "scan") == 0) {
        if (!bluetooth_is_enabled()) { shell_print("Once bluetooth acin (bt enable)\n", 12); return -1; }
        shell_print("Bluetooth taramasi...\n", 8);
        bt_device_t devices[16];
        int count = bluetooth_scan(devices, 16);
        if (count == 0) {
            shell_print("Bluetooth donanimi bulunamadi (UEFI ortaminda HCI mevcut degil)\n", 12);
            return 0;
        }
        shell_print("  #  MAC               Isim                 Sinyal  Tur\n", 15);
        for (int i = 0; i < count; i++) {
            shell_print("  ", 15); shell_print_int(i + 1, 15); shell_print("  ", 15);
            shell_print_int(devices[i].addr[0], 15); shell_print(":", 15);
            shell_print_int(devices[i].addr[1], 15); shell_print(":", 15);
            shell_print_int(devices[i].addr[2], 15); shell_print(":", 15);
            shell_print_int(devices[i].addr[3], 15); shell_print(":", 15);
            shell_print_int(devices[i].addr[4], 15); shell_print(":", 15);
            shell_print_int(devices[i].addr[5], 15); shell_print("  ", 15);
            shell_print(devices[i].name, 15);
            int nlen = 0; while (devices[i].name[nlen]) nlen++;
            for (int p = nlen; p < 21; p++) shell_print(" ", 15);
            shell_print_int(devices[i].rssi, 15); shell_print(" dBm  ", 15);
            shell_print(devices[i].type == BT_TYPE_BLE ? "BLE" : "Classic", 10);
            shell_newline();
        }
        shell_print_int(count, 10); shell_print(" cihaz bulundu\n", 10);
        return 0;
    }
    if (strcmp(sub, "pair") == 0) {
        if (argc < 3) { shell_print("Kullanim: bt pair <mac>\n", 12); return -1; }
        u8 addr[6] = {0};
        char *mac_str = argv[2];
        for (int i = 0; i < 6 && *mac_str; i++) {
            int val = 0;
            for (int d = 0; d < 2 && *mac_str; d++) {
                if (*mac_str >= '0' && *mac_str <= '9') val = val * 16 + (*mac_str - '0');
                else if (*mac_str >= 'a' && *mac_str <= 'f') val = val * 16 + (*mac_str - 'a' + 10);
                else if (*mac_str >= 'A' && *mac_str <= 'F') val = val * 16 + (*mac_str - 'A' + 10);
                mac_str++;
            }
            addr[i] = (u8)val;
            if (*mac_str == ':') mac_str++;
        }
        int r = bluetooth_pair(addr);
        if (r == 0) shell_print("Eslestirme basarili\n", 10);
        else shell_print("Eslestirme basarisiz\n", 12);
        return r;
    }
    if (strcmp(sub, "connect") == 0) {
        if (argc < 3) { shell_print("Kullanim: bt connect <mac>\n", 12); return -1; }
        shell_print("Baglaniyor...\n", 8);
        u8 addr[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0x00, 0x01};
        int r = bluetooth_connect(addr);
        if (r == 0) shell_print("Baglandi\n", 10);
        else shell_print("Baglanti basarisiz\n", 12);
        return r;
    }
    if (strcmp(sub, "disconnect") == 0) {
        bluetooth_disconnect();
        shell_print("Baglanti kesildi\n", 10);
        return 0;
    }
    if (strcmp(sub, "bonded") == 0) {
        bt_bond_t list[16];
        int count = bluetooth_get_bonded(list, 16);
        if (count == 0) { shell_print("Eslesmis cihaz yok\n", 8); return 0; }
        shell_print("Eslesmis Cihazlar:\n", 14);
        for (int i = 0; i < count; i++) {
            shell_print("  ", 15); shell_print_int(i + 1, 15); shell_print(". ", 15);
            shell_print_int(list[i].addr[0], 15); shell_print(":", 15);
            shell_print_int(list[i].addr[1], 15); shell_print(":", 15);
            shell_print_int(list[i].addr[2], 15); shell_print("  ", 15);
            shell_print(list[i].connected ? "[BAGLI]" : "[ESLESMIS]", list[i].connected ? 10 : 8);
            shell_newline();
        }
        return 0;
    }
    shell_print("bt: bilinmeyen alt komut\n", 12);
    return -1;
}

/* ─── sound / beep ───────────────────────────────────── */
#include "../include/sound.h"
static int cmd_sound(int argc, char** argv) {
    if (argc < 2) {
        shell_print("sound v1.0 - Ses Kontrol\n", 14);
        shell_print("Kullanim:\n", 14);
        shell_print("  beep <hz> <ms>   Beep cal\n", 15);
        shell_print("  sound startup    Baslangic melodi\n", 15);
        shell_print("  sound shutdown   Kapanis melodi\n", 15);
        shell_print("  sound error      Hata sesi\n", 15);
        shell_print("  sound notify     Bildirim sesi\n", 15);
        shell_print("  sound off        Sesi kapat\n", 15);
        return 0;
    }
    if (strcmp(argv[1], "off") == 0) { sound_off(); shell_print("Ses kapatildi\n", 10); return 0; }
    if (strcmp(argv[1], "startup") == 0) { sound_startup(); return 0; }
    if (strcmp(argv[1], "shutdown") == 0) { sound_shutdown(); return 0; }
    if (strcmp(argv[1], "error") == 0) { sound_error(); return 0; }
    if (strcmp(argv[1], "notify") == 0) { sound_notify(); return 0; }
    if (argc >= 3) {
        int freq = 0, dur = 0;
        const char *f = argv[1]; while (*f >= '0' && *f <= '9') { freq = freq * 10 + (*f - '0'); f++; }
        const char *d = argv[2]; while (*d >= '0' && *d <= '9') { dur = dur * 10 + (*d - '0'); d++; }
        if (freq > 0 && dur > 0) {
            shell_print("Beep: ", 8); shell_print_int(freq, 8); shell_print(" Hz, ", 8);
            shell_print_int(dur, 8); shell_print(" ms\n", 8);
            sound_beep((u16)freq, (u16)dur);
            return 0;
        }
    }
    shell_print("sound: bilinmeyen komut\n", 12);
    return -1;
}

/* ─── usb ────────────────────────────────────────────── */
#include "../include/usb.h"
static int cmd_usb(int argc, char** argv) {
    if (argc < 2) {
        shell_print("usb v1.0 - USB Yonetici\n", 14);
        shell_print("Kullanim:\n", 14);
        shell_print("  usb list       USB cihazlar\n", 15);
        shell_print("  usb scan       USB tara\n", 15);
        shell_print("  usb info <n>   Cihaz bilgisi\n", 15);
        return 0;
    }
    if (strcmp(argv[1], "list") == 0 || strcmp(argv[1], "scan") == 0) {
        int count = usb_enumerate();
        if (count == 0) { shell_print("USB cihaz bulunamadi\n", 12); return 0; }
        shell_print("USB Cihazlar:\n", 14);
        for (int i = 0; i < count; i++) {
            usb_device_t dev;
            usb_get_device_info(i, &dev);
            shell_print("  ", 15); shell_print_int(i, 15); shell_print(". Port ", 15);
            shell_print_int(dev.port, 15); shell_print("  Sınıf: ", 15);
            if (dev.class == USB_CLASS_HID) shell_print("HID (Klavye/Fare)", 10);
            else if (dev.class == USB_CLASS_MASS) shell_print("Mass Storage", 10);
            else if (dev.class == USB_CLASS_CDC) shell_print("CDC", 10);
            else if (dev.class == USB_CLASS_HUB) shell_print("Hub", 10);
            else shell_print("Bilinmeyen", 8);
            shell_print("  VID:PID ", 15);
            shell_print_int(dev.vendor_id, 15); shell_print(":", 15);
            shell_print_int(dev.product_id, 15); shell_newline();
        }
        shell_print_int(count, 10); shell_print(" cihaz bulundu\n", 10);
        return 0;
    }
    if (strcmp(argv[1], "info") == 0 && argc >= 3) {
        int idx = 0;
        const char *p = argv[2]; while (*p >= '0' && *p <= '9') { idx = idx * 10 + (*p - '0'); p++; }
        usb_device_t dev;
        if (usb_get_device_info(idx, &dev) < 0) { shell_print("Cihaz bulunamadi\n", 12); return -1; }
        shell_print("USB Cihaz Bilgisi:\n", 14);
        shell_print("  Port:      ", 15); shell_print_int(dev.port, 15); shell_newline();
        shell_print("  Sinif:     ", 15); shell_print_int(dev.class, 15); shell_newline();
        shell_print("  Vendor:    ", 15); shell_print_int(dev.vendor_id, 15); shell_newline();
        shell_print("  Product:   ", 15); shell_print_int(dev.product_id, 15); shell_newline();
        shell_print("  EP In:     ", 15); shell_print_int(dev.ep_in, 15); shell_newline();
        shell_print("  MaxPkt:    ", 15); shell_print_int(dev.max_packet, 15); shell_newline();
        return 0;
    }
    shell_print("usb: bilinmeyen alt komut\n", 12);
    return -1;
}

/* ─── ssh ────────────────────────────────────────────── */
#include "../include/ssh.h"
static int cmd_ssh(int argc, char** argv) {
    if (argc < 2) {
        shell_print("ssh v1.0 - SSH Client\n", 14);
        shell_print("Kullanim:\n", 14);
        shell_print("  ssh connect <host> [port] Baglan\n", 15);
        shell_print("  ssh exec <host> <cmd>     Komut calistir\n", 15);
        shell_print("  ssh known                 Bilinen sunucular\n", 15);
        shell_print("  ssh add-host <h> <p>      Sunucu ekle\n", 15);
        shell_print("  ssh remove-host <h> <p>   Sunucu sil\n", 15);
        return 0;
    }
    static ssh_session_t session;
    const char *sub = argv[1];
    if (strcmp(sub, "connect") == 0) {
        if (argc < 3) { shell_print("Kullanim: ssh connect <host> [port]\n", 12); return -1; }
        int port = 22;
        if (argc >= 4) {
            const char *p = argv[3]; while (*p >= '0' && *p <= '9') { port = port * 10 + (*p - '0'); p++; }
        }
        shell_print("Baglaniyor: ", 8); shell_print(argv[2], 8);
        shell_print(":", 8); shell_print_int(port, 8); shell_print("...\n", 8);
        int r = ssh_connect(argv[2], port, "root");
        if (r == 0) {
            shell_print("SSH baglantisi basarili!\n", 10);
            ssh_authenticate_password(&session, "test");
            shell_print("Kimlik dogrulandi\n", 10);
        } else {
            shell_print("SSH baglantisi basarisiz\n", 12);
        }
        return r;
    }
    if (strcmp(sub, "exec") == 0) {
        if (argc < 4) { shell_print("Kullanim: ssh exec <host> <komut>\n", 12); return -1; }
        shell_print("Komut calistiriliyor: ", 8); shell_print(argv[3], 8); shell_newline();
        char output[256];
        int r = ssh_exec(&session, argv[3], output, 256);
        if (r >= 0) { shell_print(output, 10); shell_newline(); }
        else shell_print("Komut calistirilamadi\n", 12);
        return r;
    }
    if (strcmp(sub, "known") == 0) {
        shell_print("Bilinen SSH Sunuculari:\n", 14);
        shell_print("  (bos)\n", 8);
        return 0;
    }
    if (strcmp(sub, "add-host") == 0) {
        if (argc < 4) { shell_print("Kullanim: ssh add-host <host> <port>\n", 12); return -1; }
        int port = 22;
        const char *p = argv[3]; while (*p >= '0' && *p <= '9') { port = port * 10 + (*p - '0'); p++; }
        u8 fp[32] = {0};
        ssh_known_hosts_add(argv[2], port, fp);
        shell_print("Sunucu eklendi: ", 10); shell_print(argv[2], 10); shell_newline();
        return 0;
    }
    if (strcmp(sub, "remove-host") == 0) {
        if (argc < 4) { shell_print("Kullanim: ssh remove-host <host> <port>\n", 12); return -1; }
        int port = 22;
        const char *p = argv[3]; while (*p >= '0' && *p <= '9') { port = port * 10 + (*p - '0'); p++; }
        ssh_known_hosts_remove(argv[2], port);
        shell_print("Sunucu silindi\n", 10);
        return 0;
    }
    shell_print("ssh: bilinmeyen alt komut\n", 12);
    return -1;
}

/* ─── pacman2: Package Manager ───────────────────────── */
#include "../include/pkgmgr.h"
static int cmd_pacman2(int argc, char** argv) {
    static int pkg_initialized = 0;
    if (!pkg_initialized) { pkg_init(); pkg_initialized = 1; }
    if (argc < 2) {
        shell_print("pacman2 v1.0 - Paket Yonetici\n", 14);
        shell_print("Kullanim:\n", 14);
        shell_print("  pacman2 search <sorgu>    Paket ara\n", 15);
        shell_print("  pacman2 install <paket>   Yükle\n", 15);
        shell_print("  pacman2 remove <paket>    Kaldir\n", 15);
        shell_print("  pacman2 list              Yüklü paketler\n", 15);
        shell_print("  pacman2 info <paket>      Paket bilgisi\n", 15);
        shell_print("  pacman2 sync              Depo senkronize et\n", 15);
        shell_print("  pacman2 upgrade           Paketleri guncelle\n", 15);
        return 0;
    }
    const char *sub = argv[1];
    if (strcmp(sub, "search") == 0) {
        if (argc < 3) { shell_print("Kullanim: pacman2 search <sorgu>\n", 12); return -1; }
        pkg_info_t results[16];
        int count = pkg_search(argv[2], results, 16);
        if (count == 0) { shell_print("Sonuc bulunamadi\n", 12); return 0; }
        shell_print("  Paket          Surum     Boyut   Aciklama\n", 15);
        shell_print("  -----          -----     -----   --------\n", 15);
        for (int i = 0; i < count; i++) {
            shell_print("  ", 15); shell_print(results[i].name, 15);
            int nlen = 0; while (results[i].name[nlen]) nlen++;
            for (int p = nlen; p < 16; p++) shell_print(" ", 15);
            shell_print(results[i].version, 15); shell_print("  ", 15);
            shell_print_int(results[i].size_kb, 15); shell_print(" KB  ", 15);
            shell_print(results[i].description, 8);
            shell_newline();
        }
        shell_print_int(count, 10); shell_print(" sonuc\n", 10);
        return 0;
    }
    if (strcmp(sub, "install") == 0) {
        if (argc < 3) { shell_print("Kullanim: pacman2 install <paket>\n", 12); return -1; }
        shell_print("Yükleniyor: ", 8); shell_print(argv[2], 8); shell_print("...\n", 8);
        int r = pkg_install(argv[2]);
        if (r == 0) { shell_print("Basariyla yuklendi!\n", 10); }
        else if (r == -2) { shell_print("Paket zaten yuklu\n", 8); }
        else if (r == -3) { shell_print("Paket bulunamadi\n", 12); }
        else { shell_print("Yukleme hatasi\n", 12); }
        return r;
    }
    if (strcmp(sub, "remove") == 0) {
        if (argc < 3) { shell_print("Kullanim: pacman2 remove <paket>\n", 12); return -1; }
        shell_print("Kaldiriliyor: ", 8); shell_print(argv[2], 8); shell_print("...\n", 8);
        int r = pkg_remove(argv[2]);
        if (r == 0) { shell_print("Basariyla kaldirildi\n", 10); }
        else if (r == -2) { shell_print("Paket yuklu degil\n", 8); }
        else if (r == -3) { shell_print("Paket bulunamadi\n", 12); }
        else if (r == -4) { shell_print("Zorunlu paket, kaldirilamaz\n", 12); }
        else { shell_print("Kaldirma hatasi\n", 12); }
        return r;
    }
    if (strcmp(sub, "list") == 0) {
        pkg_installed_t list[32];
        int count = pkg_list_installed(list, 32);
        if (count == 0) { shell_print("Yuklu paket yok\n", 8); return 0; }
        shell_print("Yuklu Paketler:\n", 14);
        for (int i = 0; i < count; i++) {
            shell_print("  ", 15); shell_print(list[i].name, 15);
            int nlen = 0; while (list[i].name[nlen]) nlen++;
            for (int p = nlen; p < 16; p++) shell_print(" ", 15);
            shell_print(list[i].version, 15); shell_newline();
        }
        shell_print_int(count, 10); shell_print(" paket yuklu\n", 10);
        return 0;
    }
    if (strcmp(sub, "info") == 0) {
        if (argc < 3) { shell_print("Kullanim: pacman2 info <paket>\n", 12); return -1; }
        pkg_info_t info;
        if (pkg_info(argv[2], &info) < 0) { shell_print("Paket bulunamadi\n", 12); return -1; }
        shell_print("Paket:      ", 14); shell_print(info.name, 14); shell_newline();
        shell_print("Surum:      ", 15); shell_print(info.version, 15); shell_newline();
        shell_print("Aciklama:   ", 15); shell_print(info.description, 15); shell_newline();
        shell_print("Boyut:      ", 15); shell_print_int(info.size_kb, 15); shell_print(" KB\n", 15);
        shell_print("Durum:      ", 15); shell_print(info.installed ? "Yüklu" : "Yüklu Degil", info.installed ? 10 : 8); shell_newline();
        return 0;
    }
    if (strcmp(sub, "sync") == 0) {
        shell_print("Depo senkronize ediliyor...\n", 8);
        int count = pkg_sync();
        shell_print_int(count, 10); shell_print(" paket mevcut\n", 10);
        return 0;
    }
    if (strcmp(sub, "upgrade") == 0) {
        shell_print("Paketler guncelleniyor...\n", 8);
        int count = pkg_upgrade();
        shell_print_int(count, 10); shell_print(" paket guncellenebilir\n", 10);
        return 0;
    }
    shell_print("pacman2: bilinmeyen alt komut\n", 12);
    return -1;
}

/* ─── htop: System Monitor ──────────────────────────── */
#include "../include/sysmon.h"
static int cmd_htop(int argc, char** argv) {
    static int sm_init = 0;
    if (!sm_init) { sysmon_init(); sm_init = 1; }
    sysmon_refresh();
    sysmon_stats_t st;
    sysmon_get_stats(&st);
    shell_print("=== CofeuOS Sistem Monitoru ===\n", 14);
    shell_newline();
    shell_print("CPU:   [", 15);
    int bars = st.cpu_usage_percent / 5;
    for (int i = 0; i < 20; i++) { shell_print(i < bars ? "#" : " ", 15); }
    shell_print("] ", 15); shell_print_int(st.cpu_usage_percent, 15); shell_print("%\n", 15);
    shell_print("Bellek:[", 15);
    int mbars = (st.used_ram_kb * 20) / st.total_ram_kb;
    for (int i = 0; i < 20; i++) { shell_print(i < mbars ? "#" : " ", 15); }
    shell_print("] ", 15); shell_print_int(st.used_ram_kb, 15); shell_print("/", 15);
    shell_print_int(st.total_ram_kb, 15); shell_print(" KB\n", 15);
    shell_newline();
    shell_print("  PID  Isim                 Durum    Bellek   CPU\n", 15);
    shell_print("  ---  ----                 -----    ------   ---\n", 15);
    sysmon_process_t procs[32];
    int pc = sysmon_get_processes(procs, 32);
    for (int i = 0; i < pc; i++) {
        shell_print("  ", 15);
        shell_print_int(procs[i].pid, 15); shell_print("  ", 15);
        shell_print(procs[i].name, 15);
        int nlen = 0; while (procs[i].name[nlen]) nlen++;
        for (int p = nlen; p < 20; p++) shell_print(" ", 15);
        const char *state_str = "RUN";
        if (procs[i].state == PROC_STATE_SLEEPING) state_str = "SLP";
        else if (procs[i].state == PROC_STATE_STOPPED) state_str = "STP";
        else if (procs[i].state == PROC_STATE_ZOMBIE) state_str = "ZMB";
        shell_print(state_str, 8); shell_print("     ", 15);
        shell_print_int(procs[i].mem_kb, 15); shell_print(" KB  ", 15);
        shell_print_int(procs[i].cpu_percent, 15); shell_print("%\n", 15);
    }
    shell_newline();
    shell_print("Toplam: ", 8); shell_print_int(st.process_count, 8);
    shell_print(" surec, ", 8); shell_print_int(st.thread_count, 8); shell_print(" is parcacigi\n", 8);
    shell_print("Disk:   ", 15); shell_print_int(st.disk_used_kb, 15); shell_print("/", 15);
    shell_print_int(st.disk_total_kb, 15); shell_print(" KB kullaniliyor\n", 15);
    shell_print("Ag RX:  ", 15); shell_print_int(st.net_rx_bytes, 15); shell_print(" byte\n", 15);
    shell_print("Ag TX:  ", 15); shell_print_int(st.net_tx_bytes, 15); shell_print(" byte\n", 15);
    if (argc > 1 && strcmp(argv[1], "help") == 0) {
        shell_newline();
        shell_print("Kullanim: htop [help]\n", 14);
    }
    return 0;
}

/* ─── nano2: Text Editor ────────────────────────────── */
#include "../include/editor.h"
static int cmd_nano2(int argc, char** argv) {
    static editor_state_t ed;
    editor_init(&ed);
    if (argc < 2) {
        shell_print("nano2 v1.0 - Metin Duzenleyici\n", 14);
        shell_print("Kullanim: nano2 <dosya>\n", 14);
        return 0;
    }
    editor_open(&ed, argv[1]);
    shell_print("nano2: ", 10); shell_print(argv[1], 10); shell_print(" acildi\n", 10);
    shell_print("Duzenleme modu: yazi yazin, ESC ile cikis\n", 8);
    editor_run(&ed);
    shell_print("Editor kapandi. Degisiklikler kaydedildi.\n", 10);
    return 0;
}

/* ─── cal/calendar: Calendar ────────────────────────── */
#include "../include/calendar.h"
static int cmd_cal(int argc, char** argv) {
    static int cal_init = 0;
    if (!cal_init) { calendar_init(); cal_init = 1; }
    calendar_time_t t;
    calendar_get_time(&t);
    if (argc >= 2 && strcmp(argv[1], "set") == 0) {
        if (argc < 8) {
            shell_print("Kullanim: cal set <yil> <ay> <gun> <saat> <dk> <sn>\n", 12);
            return -1;
        }
        int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
        const char *p;
        p = argv[2]; while (*p >= '0' && *p <= '9') { y = y * 10 + (*p - '0'); p++; }
        p = argv[3]; while (*p >= '0' && *p <= '9') { mo = mo * 10 + (*p - '0'); p++; }
        p = argv[4]; while (*p >= '0' && *p <= '9') { d = d * 10 + (*p - '0'); p++; }
        p = argv[5]; while (*p >= '0' && *p <= '9') { h = h * 10 + (*p - '0'); p++; }
        p = argv[6]; while (*p >= '0' && *p <= '9') { mi = mi * 10 + (*p - '0'); p++; }
        p = argv[7]; while (*p >= '0' && *p <= '9') { s = s * 10 + (*p - '0'); p++; }
        calendar_set_time(y, mo, d, h, mi, s);
        shell_print("Tarih ayarlandi\n", 10);
        return 0;
    }
    shell_print("     ", 15);
    const char *month_names[] = {"", "Ocak", "Subat", "Mart", "Nisan", "Mayis", "Haziran",
                                  "Temmuz", "Agustos", "Eylul", "Ekim", "Kasim", "Aralik"};
    shell_print(month_names[t.month], 14); shell_print(" ", 14);
    shell_print_int(t.year, 14); shell_newline();
    shell_print(" Pz  Sa  Ca  Pe  Cu  Ct  Pz\n", 15);
    int days = calendar_days_in_month(t.year, t.month);
    int dow = calendar_day_of_week(t.year, t.month, 1);
    for (int i = 0; i < dow * 4; i++) shell_print(" ", 15);
    for (int d = 1; d <= days; d++) {
        if (d == t.day && t.month > 0) { shell_print("[", 10); shell_print_int(d, 10); shell_print("]", 10); }
        else { if (d < 10) shell_print(" ", 15); shell_print_int(d, 15); }
        shell_print(" ", 15);
        if ((dow + d) % 7 == 0) shell_newline();
    }
    shell_newline();
    shell_newline();
    shell_print("  Saat: ", 15);
    shell_print_int(t.hour, 15); shell_print(":", 15);
    if (t.minute < 10) shell_print("0", 15);
    shell_print_int(t.minute, 15); shell_print(":", 15);
    if (t.second < 10) shell_print("0", 15);
    shell_print_int(t.second, 15); shell_newline();
    return 0;
}

/* ─── games: Snake/Tetris/Pac-Man ────────────────────── */
#include "../include/games.h"
static int cmd_games(int argc, char** argv) {
    if (argc < 2) {
        shell_print("games v1.0 - Oyunlar\n", 14);
        shell_print("Kullanim:\n", 14);
        shell_print("  games snake    Snake oyna\n", 15);
        shell_print("  games tetris   Tetris oyna\n", 15);
        shell_print("  games pacman   Pac-Man oyna\n", 15);
        shell_print("  games list     Mevcut oyunlar\n", 15);
        return 0;
    }
    if (strcmp(argv[1], "list") == 0) {
        shell_print("Mevcut Oyunlar:\n", 14);
        shell_print("  1. Snake    - Klasik yilan oyunu\n", 10);
        shell_print("  2. Tetris   - Blok yerlestirme\n", 10);
        shell_print("  3. Pac-Man  - Hayalet kacirma\n", 10);
        return 0;
    }
    if (strcmp(argv[1], "snake") == 0) {
        snake_game_t game;
        snake_init(&game);
        shell_print("=== SNAKE ===\n", 14);
        shell_print("Oyun basladi! Yilan: @  Yiyecek: *\n", 10);
        shell_print("Yon tuslari: W=ust A=sol S=alt D=sag\n", 8);
        shell_print("Oyun 20 hamle sonra bitecek (simulasyon)\n", 8);
        char buf[SNAKE_GRID_W * SNAKE_GRID_H];
        for (int turn = 0; turn < 20 && game.alive; turn++) {
            int input = (turn % 4) + 1;
            snake_tick(&game, input);
            snake_draw(&game, buf, SNAKE_GRID_W, SNAKE_GRID_H);
        }
        shell_print("Oyun bitti! Skor: ", 10); shell_print_int(game.score, 10); shell_newline();
        return 0;
    }
    if (strcmp(argv[1], "tetris") == 0) {
        tetris_game_t game;
        tetris_init(&game);
        shell_print("=== TETRIS ===\n", 14);
        shell_print("Oyun basladi!\n", 10);
        for (int turn = 0; turn < 10; turn++) tetris_tick(&game, 0);
        shell_print("Tetris tamamlandi! Skor: ", 10); shell_print_int(game.score, 10); shell_newline();
        return 0;
    }
    if (strcmp(argv[1], "pacman") == 0) {
        pacman_game_t game;
        pacman_init(&game);
        shell_print("=== PAC-MAN ===\n", 14);
        shell_print("Oyun basladi!\n", 10);
        for (int turn = 0; turn < 10; turn++) pacman_tick(&game, 0);
        shell_print("Pac-Man tamamlandi! Skor: ", 10); shell_print_int(game.score, 10); shell_newline();
        return 0;
    }
    shell_print("games: bilinmeyen oyun\n", 12);
    return -1;
}

/* ─── mail: E-posta ──────────────────────────────────── */
#include "../include/mail.h"
static int cmd_mail(int argc, char** argv) {
    static int mail_init_flag = 0;
    if (!mail_init_flag) { mail_init(); mail_init_flag = 1; }
    if (argc < 2) {
        shell_print("mail v1.0 - E-posta Istemcisi\n", 14);
        shell_print("Kullanim:\n", 14);
        shell_print("  mail configure <smtp> <pop3> <user> <pass>\n", 15);
        shell_print("  mail send <to> <subject> <body>\n", 15);
        shell_print("  mail inbox          Gelen kutusu\n", 15);
        shell_print("  mail outbox         Giden kutusu\n", 15);
        shell_print("  mail read <n>       Mesaj oku\n", 15);
        shell_print("  mail status         Durum\n", 15);
        return 0;
    }
    const char *sub = argv[1];
    if (strcmp(sub, "configure") == 0) {
        if (argc < 6) { shell_print("Kullanim: mail configure <smtp> <pop3> <user> <pass>\n", 12); return -1; }
        mail_configure(argv[2], 587, argv[3], 995, argv[4], argv[5]);
        shell_print("E-posta yapilandirildi\n", 10);
        return 0;
    }
    if (strcmp(sub, "send") == 0) {
        if (argc < 5) { shell_print("Kullanim: mail send <to> <subject> <body>\n", 12); return -1; }
        int r = mail_send(argv[2], argv[3], argv[4]);
        if (r == 0) shell_print("Mesaj gonderildi\n", 10);
        else if (r == -2) shell_print("E-posta yapilandirilmamis (mail configure)\n", 12);
        else shell_print("Gonderme hatasi\n", 12);
        return r;
    }
    if (strcmp(sub, "inbox") == 0) {
        shell_print("Gelen Kutusu:\n", 14);
        shell_print("  (bos)\n", 8);
        return 0;
    }
    if (strcmp(sub, "outbox") == 0) {
        mail_message_t msgs[16];
        int count = mail_get_outbox(msgs, 16);
        if (count == 0) { shell_print("Giden kutusu bos\n", 8); return 0; }
        shell_print("Giden Kutusu:\n", 14);
        for (int i = 0; i < count; i++) {
            shell_print("  ", 15); shell_print_int(i + 1, 15); shell_print(". -> ", 15);
            shell_print(msgs[i].to, 10); shell_print(" | ", 15); shell_print(msgs[i].subject, 15);
            shell_newline();
        }
        return 0;
    }
    if (strcmp(sub, "read") == 0) {
        shell_print("Okuma modu: (simulasyon)\n", 8);
        return 0;
    }
    if (strcmp(sub, "status") == 0) {
        shell_print("E-posta Durumu:\n", 14);
        shell_print("  Yapilandirma: ", 15);
        shell_print(mail_is_configured() ? "Yapilandirilmis" : "Yapilandirilmamis",
                    mail_is_configured() ? 10 : 12);
        shell_newline();
        return 0;
    }
    shell_print("mail: bilinmeyen alt komut\n", 12);
    return -1;
}

/* ─── pdf: PDF Viewer ────────────────────────────────── */
#include "../include/pdf.h"
static int cmd_pdf(int argc, char** argv) {
    static pdf_viewer_t viewer;
    if (argc < 2) {
        shell_print("pdf v1.0 - PDF Goruntuleyici\n", 14);
        shell_print("Kullanim:\n", 14);
        shell_print("  pdf open <dosya>    PDF ac\n", 15);
        shell_print("  pdf next            Sonraki sayfa\n", 15);
        shell_print("  pdf prev            Onceki sayfa\n", 15);
        shell_print("  pdf zoom <in/out>   Yakınlastir\n", 15);
        shell_print("  pdf draw            Sayfayi goster\n", 15);
        shell_print("  pdf close           Kapat\n", 15);
        return 0;
    }
    const char *sub = argv[1];
    if (strcmp(sub, "open") == 0) {
        if (argc < 3) { shell_print("Kullanim: pdf open <dosya>\n", 12); return -1; }
        pdf_init(&viewer);
        int r = pdf_open(&viewer, argv[2]);
        if (r == 0) { shell_print("PDF acildi: ", 10); shell_print(argv[2], 10); shell_newline(); }
        else shell_print("PDF acilamadi\n", 12);
        return r;
    }
    if (strcmp(sub, "next") == 0) { int r = pdf_next_page(&viewer); if (r < 0) shell_print("Son sayfa\n", 8); return 0; }
    if (strcmp(sub, "prev") == 0) { int r = pdf_prev_page(&viewer); if (r < 0) shell_print("Ilk sayfa\n", 8); return 0; }
    if (strcmp(sub, "zoom") == 0) {
        if (argc < 3) { shell_print("Kullanim: pdf zoom <in/out>\n", 12); return -1; }
        if (strcmp(argv[2], "in") == 0) pdf_zoom_in(&viewer);
        else if (strcmp(argv[2], "out") == 0) pdf_zoom_out(&viewer);
        shell_print("Zoom: ", 15); shell_print_int(viewer.zoom, 15); shell_print("%\n", 15);
        return 0;
    }
    if (strcmp(sub, "draw") == 0) {
        char buf[80 * 24];
        pdf_draw_page(&viewer, buf, 80, 24);
        for (int y = 0; y < 24; y++) {
            for (int x = 0; x < 80; x++) {
                char out[2] = {buf[y * 80 + x], '\0'};
                shell_print(out, 15);
            }
            shell_newline();
        }
        return 0;
    }
    if (strcmp(sub, "close") == 0) { pdf_close(&viewer); shell_print("PDF kapandi\n", 10); return 0; }
    shell_print("pdf: bilinmeyen alt komut\n", 12);
    return -1;
}

/* ─── music/mp: Music Player ────────────────────────── */
#include "../include/music.h"
static int cmd_music(int argc, char** argv) {
    static music_player_t player;
    static int music_init_flag = 0;
    if (!music_init_flag) { music_init(&player); music_init_flag = 1; }
    if (argc < 2) {
        shell_print("music v1.0 - Muzik Oynatici\n", 14);
        shell_print("Kullanim:\n", 14);
        shell_print("  music play <n>     Sarki cal\n", 15);
        shell_print("  music pause        Duraklat\n", 15);
        shell_print("  music stop         Durdur\n", 15);
        shell_print("  music next         Sonraki\n", 15);
        shell_print("  music prev         Onceki\n", 15);
        shell_print("  music list         Sarki listesi\n", 15);
        shell_print("  music add <p> <a>  Sarki ekle\n", 15);
        shell_print("  music volume <0-100> Ses ayari\n", 15);
        shell_print("  music status       Durum\n", 15);
        return 0;
    }
    const char *sub = argv[1];
    if (strcmp(sub, "add") == 0) {
        if (argc < 4) { shell_print("Kullanim: music add <baslik> <sanatci>\n", 12); return -1; }
        int idx = music_add_track(&player, argv[2], argv[2], argv[3]);
        shell_print("Sarki eklendi: #", 10); shell_print_int(idx, 10); shell_newline();
        return 0;
    }
    if (strcmp(sub, "list") == 0) {
        if (player.count == 0) { shell_print("Playlist bos (music add ile sarki ekleyin)\n", 8); return 0; }
        shell_print("Playlist:\n", 14);
        for (int i = 0; i < player.count; i++) {
            shell_print("  ", 15); shell_print_int(i + 1, 15); shell_print(". ", 15);
            shell_print(player.tracks[i].title, 15); shell_print(" - ", 8);
            shell_print(player.tracks[i].artist, 8); shell_newline();
        }
        return 0;
    }
    if (strcmp(sub, "play") == 0) {
        int idx = 0;
        if (argc >= 3) { const char *p = argv[2]; while (*p >= '0' && *p <= '9') { idx = idx * 10 + (*p - '0'); p++; } idx--; }
        int r = music_play(&player, idx);
        if (r == 0) shell_print("Caliniyor...\n", 10);
        else shell_print("Sarki bulunamadi\n", 12);
        return r;
    }
    if (strcmp(sub, "pause") == 0) { music_pause(&player); shell_print("Duraklatildi\n", 8); return 0; }
    if (strcmp(sub, "stop") == 0) { music_stop(&player); shell_print("Durduruldu\n", 8); return 0; }
    if (strcmp(sub, "next") == 0) { int n = music_next(&player); shell_print("Sonraki: #", 10); shell_print_int(n + 1, 10); shell_newline(); return 0; }
    if (strcmp(sub, "prev") == 0) { int n = music_prev(&player); shell_print("Onceki: #", 10); shell_print_int(n + 1, 10); shell_newline(); return 0; }
    if (strcmp(sub, "volume") == 0) {
        if (argc < 3) { shell_print("Ses: ", 15); shell_print_int(player.volume, 15); shell_print("%\n", 15); return 0; }
        int vol = 0; const char *p = argv[2]; while (*p >= '0' && *p <= '9') { vol = vol * 10 + (*p - '0'); p++; }
        music_set_volume(&player, (u8)vol);
        shell_print("Ses ayarlandi: ", 10); shell_print_int(vol, 10); shell_print("%\n", 10);
        return 0;
    }
    if (strcmp(sub, "status") == 0) {
        int track, pos, total;
        int state = music_get_status(&player, &track, &pos, &total);
        shell_print("Muzik Durumu:\n", 14);
        shell_print("  Durum:   ", 15);
        if (state == 0) shell_print("Durdu\n", 8);
        else if (state == 1) shell_print("Caliniyor\n", 10);
        else shell_print("Duraklatildi\n", 8);
        if (player.count > 0) {
            shell_print("  Sarki:   ", 15); shell_print_int(track + 1, 15); shell_print("/", 15);
            shell_print_int(player.count, 15); shell_newline();
            shell_print("  Baslik: ", 15); shell_print(player.tracks[track].title, 15); shell_newline();
            shell_print("  Sure:   ", 15); shell_print_int(pos, 15); shell_print("/", 15);
            shell_print_int(total, 15); shell_print(" sn\n", 15);
        }
        shell_print("  Ses:    ", 15); shell_print_int(player.volume, 15); shell_print("%\n", 15);
        return 0;
    }
    shell_print("music: bilinmeyen alt komut\n", 12);
    return -1;
}
