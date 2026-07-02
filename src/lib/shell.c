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


int shell_execute(const char* cmd);

static void shell_print(const char* str, u8 color) {
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
    shell_print("Sistem: whoami uname clear date uptime free ps df echo env sysinfo\n", 15);
    shell_print("Diger: neofetch calc apps about theme desktop startx rodo reboot halt\n", 15);
    shell_print("Paket: pacman\n", 15);
    shell_print("Ag: ifconfig ping\n", 15);
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
static int cmd_uname(int argc, char** argv) { shell_print("cofeuOS v3.0 x86_64 UEFI", 14); return 0; }

static int cmd_clear(int argc, char** argv) {
    video_clear(0); cursor_x = 5; cursor_y = 30; return 0;
}

static int cmd_neofetch(int argc, char** argv) {
    shell_print("    .-\"-.     ", 15);
    shell_print("   / ..  \\    ", 15); shell_print("OS: cofeuOS v3.0", 11);
    shell_print("  | (  )  |   ", 15); shell_print("Kernel: x86_64 UEFI", 11);
    shell_print("   \\ ..  /    ", 15); shell_print("Shell: Cofeu Shell", 11);
    shell_print("    `---'      ", 15); shell_print("Host: cofeu", 11);
    shell_print("Python: MicroPython", 11);
    shell_print("Disk: /dev/sda1", 11);
    shell_print("Memory: 16MB", 11);
    return 0;
}

static int cmd_text_editor(int argc, char** argv, const char* editor_name) {
    shell_print("========================================", 14);
    shell_print(editor_name, 14);
    shell_print(" editor - :w = save, :q = quit, :wq = save+quit", 14);

    char path[MAX_PATH_LEN] = "";
    if (argc > 1) fs_resolve_path(g_shell.cwd, argv[1], path);
    shell_print("File: ", 7);
    shell_print(path[0] ? path : "<no file>", 15);
    shell_print("========================================", 14);

    char buffer[FILE_CONTENT_SIZE];
    int len = 0;
    if (path[0]) {
        int sz = fs_read_file(&g_fs, path, buffer, sizeof(buffer) - 1);
        if (sz >= 0) { buffer[sz] = '\0'; len = sz; }
        else { buffer[0] = '\0'; }
    } else { buffer[0] = '\0'; }

    char line[80];
    while (1) {
        cursor_x = 5;
        video_print("> ", 7, cursor_y, 7);
        cursor_x = 25;
        main_get_input(line, 80);

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
        if (len + ll + 1 < (int)sizeof(buffer)) {
            memcpy(buffer + len, line, ll);
            len += ll;
            buffer[len++] = '\n';
            buffer[len] = '\0';
        }
    }
    return 0;
}

static int cmd_vim(int argc, char** argv)  { return cmd_text_editor(argc, argv, "vim"); }
static int cmd_nano(int argc, char** argv) { return cmd_text_editor(argc, argv, "nano"); }

static int cmd_reboot(int argc, char** argv) {
    shell_print("Rebooting...", 14);
    outb(0x64, 0xFE);
    while(1); return 0;
}

static int cmd_halt(int argc, char** argv) {
    shell_print("cofeuOS halted.", 12);
    while(1); return 0;
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
static int parse_http_url(const char* input, char* host, int host_len, char* path, int path_len) {
    const char* p = input;
    if (p == NULL || host == NULL || path == NULL) return -1;

    if (strncmp(p, "http://", 7) == 0) {
        p += 7;
    } else if (strncmp(p, "https://", 8) == 0) {
        p += 8;
    } else {
        return -1;
    }

    const char* slash = strchr(p, '/');
    const char* query = strchr(p, '?');
    const char* end = slash ? slash : (query ? query : p + strlen(p));
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

static int download_directory_contents(const char* host, const char* remote_dir, const char* local_dir, char* buf, int buflen) {
    char remote_path[256];
    char local_path[256];
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

    int len = wget(host, remote_path, temp_target, temp_buf, sizeof(temp_buf) - 1);
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
            download_directory_contents(host, child_remote, child_local, buf, buflen);
        } else {
            wget(host, child_remote, child_local, buf, buflen);
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

    if (parse_http_url(argv[1], host, sizeof(host), http_path, sizeof(http_path)) == 0) {
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
        if (download_directory_contents(host, remote_path, target_dir, buf, sizeof(buf) - 1) < 0) {
            shell_print("wget: dizin indirilemedi", 12); shell_newline();
            return -1;
        }
        shell_print("wget: dizin indirildi", 10); shell_newline();
        return 0;
    }

    len = wget(host, remote_path, target, buf, sizeof(buf) - 1);
    
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
    shell_print("OS: cofeuOS v3.0\n", 11);
    shell_print("Kernel: x86_64 UEFI GOP\n", 11);
    shell_print("Video: ", 11);
    shell_print_int(SCREEN_WIDTH, 11); shell_print("x", 11);
    shell_print_int(SCREEN_HEIGHT, 11); shell_print("x32bpp\n", 11);
    shell_print("Bellek: 16MB\n", 11);
    shell_print("FS: cofeuFS (RAM)\n", 11);
    shell_print("Python: MicroPython embed\n", 11);
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

static void desktop_text(const char* str, int x, int y, u8 color) { video_print(str, x, y, color); }

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
    desktop_text(status, 6, SCREEN_HEIGHT - 15, 15);
}

static void desktop_show_files(void) {
    char buf[512];
    int sz = fs_list_dir(&g_fs, g_shell.cwd, buf, sizeof(buf));
    desktop_draw_base("Files app");
    desktop_draw_window(48, 82, SCREEN_WIDTH - 96, 72, "Files", 7);
    desktop_text(g_shell.cwd, 56, 104, 1);
    desktop_text(sz >= 0 && buf[0] ? buf : "(empty)", 56, 120, 0);
}

static void desktop_show_notes(void) {
    char note[256];
    int sz = fs_read_file(&g_fs, "/home/notes.txt", note, sizeof(note) - 1);
    desktop_draw_base("Notes app");
    desktop_draw_window(38, 82, SCREEN_WIDTH - 76, 76, "Notes", 7);
    if (sz >= 0) { note[sz] = '\0'; desktop_text(note, 46, 104, 0); }
    else { desktop_text("Henuz not yok. write /home/notes.txt ile yaz.", 46, 104, 0); }
}

static void desktop_show_info(void) {
    desktop_draw_base("Sistem bilgisi");
    desktop_draw_window(48, 82, SCREEN_WIDTH - 96, 84, "System", 7);
    desktop_text("cofeuOS v3.0 UEFI", 58, 104, 0);
    desktop_text("Kernel: x86_64 GOP", 58, 118, 0);
    desktop_text("Python: MicroPython embed", 58, 132, 0);
    desktop_text("Shell: cofeu shell + cofeuDE", 58, 146, 0);
}

static int cmd_desktop(int argc, char** argv) {
    char input[64];
    fs_create_dir(&g_fs, "/home");
    if (!fs_file_exists(&g_fs, "/home/notes.txt"))
        fs_create_file(&g_fs, "/home/notes.txt", "cofeuDE'ye hosgeldiniz.", 23);
    desktop_draw_base("Hazir");
    while (1) {
        cursor_x = 8; cursor_y = SCREEN_HEIGHT - 15;
        video_fill_rect(0, SCREEN_HEIGHT - 20, SCREEN_WIDTH, 20, 8);
        desktop_text("desktop> ", 6, SCREEN_HEIGHT - 15, 15);
        cursor_x = 78;
        main_get_input(input, sizeof(input));
        if (strcmp(input, "exit") == 0 || strcmp(input, "term") == 0) {
            video_clear(0); cursor_x = 5; cursor_y = 30;
            shell_print("cofeuDE'den cikild.", 10); shell_newline();
            return 0;
        }
        if (strcmp(input, "files") == 0) { desktop_show_files(); continue; }
        if (strcmp(input, "notes") == 0) { desktop_show_notes(); continue; }
        if (strcmp(input, "info")  == 0) { desktop_show_info();  continue; }
        if (strcmp(input, "clear") == 0 || strcmp(input, "home") == 0) { desktop_draw_base("Hazir"); continue; }
        desktop_draw_base("Bilinmeyen komut. files notes info term exit");
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
