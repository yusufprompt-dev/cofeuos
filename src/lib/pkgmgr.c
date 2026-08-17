#include "../include/pkgmgr.h"
#include "../include/string.h"

static pkg_info_t g_packages[PKG_MAX_PACKAGES];
static int g_pkg_count = 0;
static pkg_installed_t g_installed[PKG_INSTALLED_MAX];
static int g_installed_count = 0;

static void str_copy(char *dst, const char *src, int max) {
    int i = 0; while (src[i] && i < max - 1) { dst[i] = src[i]; i++; } dst[i] = '\0';
}
static int str_len(const char *s) { int n = 0; while (s[n]) n++; return n; }
static int str_eq(const char *a, const char *b) {
    int i = 0; while (a[i] && b[i]) { if (a[i] != b[i]) return 0; i++; }
    return a[i] == b[i];
}

static void add_package(const char *name, const char *ver, const char *desc, u32 size, u8 repo) {
    if (g_pkg_count >= PKG_MAX_PACKAGES) return;
    pkg_info_t *p = &g_packages[g_pkg_count];
    str_copy(p->name, name, PKG_NAME_MAX_LEN);
    str_copy(p->version, ver, PKG_VERSION_MAX);
    str_copy(p->description, desc, PKG_DESC_MAX);
    p->size_kb = size;
    p->repo = repo;
    p->installed = 0;
    p->mandatory = 0;
    g_pkg_count++;
}

void pkg_init(void) {
    g_pkg_count = 0;
    g_installed_count = 0;
    add_package("base",        "1.0.0", "CofeuOS temel sistemi",        2048, 0);
    add_package("kernel",      "2.1.0", "CofeuOS kernel",               1024, 0);
    add_package("shell",       "1.5.0", "Unix-like shell",               512, 0);
    add_package("network",     "2.0.0", "TCP/IP ag yiginlari",          1536, 0);
    add_package("wifi",        "1.0.0", "WiFi surucu ve yonetici",       768, 0);
    add_package("bluetooth",   "1.0.0", "Bluetooth destegi",             512, 0);
    add_package("ssh",         "1.0.0", "SSH client",                    384, 0);
    add_package("tls",         "2.0.0", "TLS 1.2 + ECDHE",              640, 0);
    add_package("web-browser", "1.2.0", "HTML/CSS/JS tarayici",        1024, 0);
    add_package("python",      "3.11",  "MicroPython yorumlayici",     2048, 0);
    add_package("nano",        "2.8.0", "Metin duzenleyici",             256, 0);
    add_package("git",         "2.40.0","Git versiyon kontrol",         1024, 1);
    add_package("gcc",         "13.2",  "C derleyici",                  4096, 1);
    add_package("vim",         "9.0",   "Vi improved editor",            512, 1);
    add_package("htop",        "3.2.0", "Sistem monitoru",               128, 1);
    add_package("tmux",        "3.3",   "Terminal multiplexer",          256, 1);
    add_package("ffmpeg",      "6.1",   "Video/audio isleme",           8192, 1);
    add_package("gimp",        "2.10",  "Gorsel duzenleyici",           6144, 1);
    add_package("firefox",     "120",   "Web tarayici",                 4096, 1);
    add_package("obsidian",    "1.4",   "Not alma uygulamasi",          2048, 2);
    add_package("vscode",      "1.85",  "Kod duzenleyici",              8192, 2);
    add_package("docker",      "24.0",  "Container motoru",             4096, 2);
    add_package("nodejs",      "20.10", "JavaScript runtime",           2048, 2);
    add_package("rust",        "1.74",  "Rust derleyici",               4096, 3);
    add_package("go",          "1.21",  "Go dili",                      2048, 3);
    add_package("neovim",      "0.9",   "Modern vim fork",               384, 1);
    add_package("ranger",      "1.9",   "Dosya yöneticisi",             128, 1);
    add_package("cmatrix",     "1.0",   "Matrix efekti",                 32, 2);
    add_package("cowsay",      "3.0",   "Inek konusturucu",              16, 2);
    add_package("figlet",      "2.2",   "Büyük yazi",                    32, 2);
}

int pkg_search(const char *query, pkg_info_t *results, int max_results) {
    if (!query || !results || max_results <= 0) return 0;
    int qlen = str_len(query);
    int found = 0;
    for (int i = 0; i < g_pkg_count && found < max_results; i++) {
        int plen = str_len(g_packages[i].name);
        int match = 0;
        if (plen >= qlen) {
            match = 1;
            for (int j = 0; j < qlen; j++) {
                if (g_packages[i].name[j] != query[j]) { match = 0; break; }
            }
        }
        if (match) results[found++] = g_packages[i];
    }
    return found;
}

int pkg_install(const char *name) {
    if (!name) return -1;
    for (int i = 0; i < g_pkg_count; i++) {
        if (str_eq(g_packages[i].name, name)) {
            if (g_packages[i].installed) return -2;
            g_packages[i].installed = 1;
            if (g_installed_count < PKG_INSTALLED_MAX) {
                pkg_installed_t *inst = &g_installed[g_installed_count];
                str_copy(inst->name, g_packages[i].name, PKG_NAME_MAX_LEN);
                str_copy(inst->version, g_packages[i].version, PKG_VERSION_MAX);
                inst->size_kb = g_packages[i].size_kb;
                inst->repo = g_packages[i].repo;
                g_installed_count++;
            }
            return 0;
        }
    }
    return -3;
}

int pkg_remove(const char *name) {
    if (!name) return -1;
    for (int i = 0; i < g_pkg_count; i++) {
        if (str_eq(g_packages[i].name, name)) {
            if (!g_packages[i].installed) return -2;
            if (g_packages[i].mandatory) return -4;
            g_packages[i].installed = 0;
            for (int j = 0; j < g_installed_count; j++) {
                if (str_eq(g_installed[j].name, name)) {
                    g_installed[j] = g_installed[g_installed_count - 1];
                    g_installed_count--;
                    return 0;
                }
            }
            return 0;
        }
    }
    return -3;
}

int pkg_update(void) {
    return g_pkg_count;
}

int pkg_list_installed(pkg_installed_t *list, int max_entries) {
    if (!list || max_entries <= 0) return 0;
    int c = (g_installed_count < max_entries) ? g_installed_count : max_entries;
    for (int i = 0; i < c; i++) list[i] = g_installed[i];
    return c;
}

int pkg_info(const char *name, pkg_info_t *out) {
    if (!name || !out) return -1;
    for (int i = 0; i < g_pkg_count; i++) {
        if (str_eq(g_packages[i].name, name)) {
            *out = g_packages[i];
            return 0;
        }
    }
    return -1;
}

int pkg_sync(void) {
    return g_pkg_count;
}

int pkg_upgrade(void) {
    int count = 0;
    for (int i = 0; i < g_pkg_count; i++) {
        if (g_packages[i].installed) count++;
    }
    return count;
}
