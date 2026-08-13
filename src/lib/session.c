/*
 * ============================================================================
 * SESSION.C - Kalıcı Oturum (Login) Hafızası
 *
 * Giriş bilgileri, RAM yerine üç katmanlı kalıcı depolamaya yazılır:
 *   1) EFI System Partition üzerindeki \cofeuos\user_session.json
 *      (diskte kalıcı yapılandırma dosyası — birincil kaynak)
 *   2) UEFI NVRAM değişkeni "CofeuOSSession" (yığın diski yoksa yedek)
 *   3) RAM dosya sistemindeki /etc/user_session.json (shell'den görülebilir)
 *
 * Parola düz metin yazılmaz: kullanıcı adından türetilen bir anahtarla XOR
 * karartılmış (obfuscated) ve hex kodlanmış olarak saklanır. Ayrıca parolanın
 * SHA-256 özeti saklanır; yüklemede bütünlük doğrulaması yapılır, kurcalanmış
 * veride parola otomatik doldurma devre dışı bırakılır.
 * ============================================================================
 */

#include "../include/session.h"
#include "../include/string.h"
#include "../include/sha256.h"
#include "../include/io.h"
#include "../include/fs.h"
#include <efi.h>

#define SESSION_NVRAM_NAME  L"CofeuOSSession"
#define SESSION_ESP_DIR     L"cofeuos"
#define SESSION_ESP_FILE    L"user_session.json"
#define SESSION_RAM_FILE    "/etc/user_session.json"
#define SESSION_JSON_MAX    512

/* CofeuOS'a özel satıcı GUID'i (NVRAM değişkeni ad alanı) */
static EFI_GUID session_guid = {
    0x9E4F8A2B, 0x1D3C, 0x4E5F, {0x8A, 0x9B, 0x0C, 0x1D, 0x2E, 0x3F, 0x4A, 0x5B}
};

/* main.c içinde tanımlanan RAM dosya sistemi */
extern fs_control_block g_fs;

/* ─── Yardımcılar ───────────────────────────────────────────── */

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static void bytes_to_hex(const u8 *data, int len, char *out) {
    static const char *h = "0123456789abcdef";
    for (int i = 0; i < len; i++) {
        out[i * 2]     = h[data[i] >> 4];
        out[i * 2 + 1] = h[data[i] & 0xF];
    }
    out[len * 2] = '\0';
}

/* Parolayı kullanıcı adından türetilen anahtarla XOR'lar ve hex kodlar */
static void xor_obfuscate(const char *plain, const char *key, char *hex_out) {
    int klen = (int)strlen(key);
    if (klen == 0) klen = 1;
    int i = 0;
    while (plain[i]) {
        unsigned char b = (unsigned char)plain[i];
        b ^= (unsigned char)((unsigned char)key[i % klen] + (unsigned char)i);
        static const char *h = "0123456789ABCDEF";
        hex_out[i * 2]     = h[b >> 4];
        hex_out[i * 2 + 1] = h[b & 0xF];
        i++;
    }
    hex_out[i * 2] = '\0';
}

/* Hex kodlu karartılmış parolayı çözer */
static void xor_deobfuscate(const char *hex, const char *key,
                            char *plain_out, int max) {
    int klen = (int)strlen(key);
    if (klen == 0) klen = 1;
    int i = 0;
    while (hex[i * 2] && hex[i * 2 + 1] && i < max - 1) {
        unsigned char b = (unsigned char)((hexval(hex[i * 2]) << 4) |
                                          hexval(hex[i * 2 + 1]));
        b ^= (unsigned char)((unsigned char)key[i % klen] + (unsigned char)i);
        plain_out[i] = (char)b;
        i++;
    }
    plain_out[i] = '\0';
}

/* JSON'a yazılacak kullanıcı adını temizle (tırnak/ters bölü/kontrol sil) */
static void sanitize_user(const char *in, char *out, int max) {
    int i = 0;
    while (in[i] && i < max - 1) {
        char c = in[i];
        if (c == '"' || c == '\\' || c < ' ') out[i] = '_';
        else out[i] = c;
        i++;
    }
    out[i] = '\0';
}

/* JSON içinden "key":"value" çiftini çıkarır */
static void json_extract(const char *json, const char *key, char *out, int max) {
    char pat[64];
    int  pi = 0;
    pat[pi++] = '"';
    while (*key) pat[pi++] = *key++;
    pat[pi++] = '"';
    pat[pi++] = ':';
    pat[pi++] = '"';
    pat[pi] = '\0';

    const char *p = strstr(json, pat);
    if (!p) { out[0] = '\0'; return; }
    p += pi;
    int i = 0;
    while (*p && *p != '"' && i < max - 1) out[i++] = *p++;
    out[i] = '\0';
}

/* Oturum JSON'ını oluşturur */
static int session_build_json(const char *user, const char *pass,
                              char *out, int max) {
    char clean[SESSION_USER_MAX];
    sanitize_user(user, clean, sizeof(clean));

    u8 digest[SHA256_DIGEST_SIZE];
    sha256_hash((const u8*)pass, strlen(pass), digest);
    char hash_hex[SESSION_HASH_HEX];
    bytes_to_hex(digest, SHA256_DIGEST_SIZE, hash_hex);

    char pass_hex[SESSION_PASS_MAX * 2 + 1];
    xor_obfuscate(pass, clean, pass_hex);

    out[0] = '\0';
    strcat(out, "{\"user\":\"");
    strcat(out, clean);
    strcat(out, "\",\"pass\":\"");
    strcat(out, pass_hex);
    strcat(out, "\",\"hash\":\"");
    strcat(out, hash_hex);
    strcat(out, "\"}");
    return (int)strlen(out) < max ? 0 : -1;
}

/* ─── EFI System Partition dosya erişimi ─────────────────────── */

static EFI_FILE_PROTOCOL *esp_root_volume(void) {
    EFI_SYSTEM_TABLE *st = (EFI_SYSTEM_TABLE*)io_get_system_table();
    if (!st || !st->BootServices) return NULL;

    EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    EFI_STATUS status = st->BootServices->LocateProtocol(&fs_guid, NULL, (VOID**)&fs);
    if (EFI_ERROR(status) || !fs) return NULL;

    EFI_FILE_PROTOCOL *root = NULL;
    status = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(status) || !root) return NULL;
    return root;
}

static int esp_write_file(const char *content) {
    EFI_FILE_PROTOCOL *root = esp_root_volume();
    if (!root) return -1;

    EFI_FILE_PROTOCOL *dir = NULL;
    EFI_STATUS status = root->Open(root, &dir, SESSION_ESP_DIR,
                                   EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE |
                                   EFI_FILE_MODE_CREATE,
                                   EFI_FILE_DIRECTORY);
    if (!EFI_ERROR(status) && dir) {
        EFI_FILE_PROTOCOL *file = NULL;
        status = dir->Open(dir, &file, SESSION_ESP_FILE,
                           EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE |
                           EFI_FILE_MODE_CREATE, 0);
        if (!EFI_ERROR(status) && file) {
            UINTN size = strlen(content);
            if (EFI_ERROR(file->Write(file, &size, (VOID*)content)))
                status = EFI_DEVICE_ERROR;
            file->Flush(file);
            file->Close(file);
        }
        dir->Close(dir);
    }
    root->Close(root);
    return EFI_ERROR(status) ? -1 : 0;
}

static int esp_read_file(char *out, int max) {
    EFI_FILE_PROTOCOL *root = esp_root_volume();
    if (!root) return -1;

    EFI_FILE_PROTOCOL *dir = NULL;
    EFI_STATUS status = root->Open(root, &dir, SESSION_ESP_DIR,
                                   EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status) || !dir) {
        root->Close(root);
        return -1;
    }

    EFI_FILE_PROTOCOL *file = NULL;
    status = dir->Open(dir, &file, SESSION_ESP_FILE, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status) || !file) {
        dir->Close(dir);
        root->Close(root);
        return -1;
    }

    UINTN size = (max > 0) ? (UINTN)(max - 1) : 0;
    status = file->Read(file, &size, (VOID*)out);
    if (EFI_ERROR(status)) size = 0;
    out[size] = '\0';

    file->Close(file);
    dir->Close(dir);
    root->Close(root);
    return (int)size;
}

static int esp_delete_file(void) {
    EFI_FILE_PROTOCOL *root = esp_root_volume();
    if (!root) return -1;

    EFI_FILE_PROTOCOL *dir = NULL;
    EFI_STATUS status = root->Open(root, &dir, SESSION_ESP_DIR,
                                   EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
    if (EFI_ERROR(status) || !dir) {
        root->Close(root);
        return -1;
    }

    EFI_FILE_PROTOCOL *file = NULL;
    status = dir->Open(dir, &file, SESSION_ESP_FILE,
                       EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
    if (!EFI_ERROR(status) && file) {
        file->Delete(file);
    } else if (file) {
        file->Close(file);
    }
    dir->Close(dir);
    root->Close(root);
    return 0;
}

/* ─── UEFI NVRAM değişken erişimi ────────────────────────────── */

#define NVRAM_ATTR (EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS)

static int nv_write(const char *data) {
    EFI_RUNTIME_SERVICES *rt = (EFI_RUNTIME_SERVICES*)io_get_runtime_services();
    if (!rt) return -1;
    EFI_STATUS status = rt->SetVariable(SESSION_NVRAM_NAME, &session_guid,
                                        NVRAM_ATTR, strlen(data) + 1, (VOID*)data);
    return EFI_ERROR(status) ? -1 : 0;
}

static int nv_read(char *out, int max) {
    EFI_RUNTIME_SERVICES *rt = (EFI_RUNTIME_SERVICES*)io_get_runtime_services();
    if (!rt) return -1;
    UINTN size = (max > 0) ? (UINTN)(max - 1) : 0;
    EFI_STATUS status = rt->GetVariable(SESSION_NVRAM_NAME, &session_guid,
                                        NULL, &size, (VOID*)out);
    if (EFI_ERROR(status)) return -1;
    out[size] = '\0';
    return (int)size;
}

/* ─── RAM dosya sistemi aynası ───────────────────────────────── */

static void ram_mirror(const char *json) {
    fs_write_file(&g_fs, SESSION_RAM_FILE, json, strlen(json));
}

static int ram_read(char *out, int max) {
    int n = fs_read_file(&g_fs, SESSION_RAM_FILE, out, (size_t)max);
    if (n < 0) return -1;
    return n;
}

/* ─── Genel API ──────────────────────────────────────────────── */

int session_load(login_session *s) {
    memset(s, 0, sizeof(login_session));
    s->valid = 0;

    char json[SESSION_JSON_MAX];
    int got = 0;

    /* 1) Öncelikle ESP'deki kalıcı yapılandırma dosyası */
    if (esp_read_file(json, (int)sizeof(json)) > 0) got = 1;
    /* 2) Yedek: NVRAM değişkeni */
    if (!got && nv_read(json, (int)sizeof(json)) > 0) got = 1;
    /* 3) Son yedek: RAM aynası */
    if (!got && ram_read(json, (int)sizeof(json)) > 0) got = 1;

    if (!got) return 0;

    char user[SESSION_USER_MAX];
    char pass_hex[SESSION_PASS_MAX * 2 + 1];
    char hash[SESSION_HASH_HEX];
    json_extract(json, "user", user, sizeof(user));
    json_extract(json, "pass", pass_hex, sizeof(pass_hex));
    json_extract(json, "hash", hash, sizeof(hash));

    if (user[0] == '\0' || pass_hex[0] == '\0' || hash[0] == '\0') return 0;

    strncpy(s->username, user, sizeof(s->username) - 1);
    xor_deobfuscate(pass_hex, s->username, s->password, (int)sizeof(s->password));
    strncpy(s->password_hash, hash, sizeof(s->password_hash) - 1);

    /* Bütünlük doğrulaması: hash tutmuyorsa parolayı doldurma */
    u8 digest[SHA256_DIGEST_SIZE];
    sha256_hash((const u8*)s->password, strlen(s->password), digest);
    char calc[SESSION_HASH_HEX];
    bytes_to_hex(digest, SHA256_DIGEST_SIZE, calc);
    if (strcmp(calc, hash) != 0)
        s->password[0] = '\0';

    s->valid = 1;

    /* Diğer katmanları senkronize et (ESP/NVRAM/RAM birbirini tamamlasın) */
    nv_write(json);
    ram_mirror(json);

    return 1;
}

int session_save(const char *user, const char *pass) {
    char json[SESSION_JSON_MAX];
    if (session_build_json(user, pass, json, (int)sizeof(json)) != 0) return -1;

    esp_write_file(json);   /* birincil: disk dosyası */
    nv_write(json);         /* yedek:  NVRAM */
    ram_mirror(json);       /* ayna:   RAM FS */
    return 0;
}

void session_clear(void) {
    EFI_RUNTIME_SERVICES *rt = (EFI_RUNTIME_SERVICES*)io_get_runtime_services();
    if (rt)
        rt->SetVariable(SESSION_NVRAM_NAME, &session_guid, NVRAM_ATTR, 0, NULL);
    esp_delete_file();
    fs_delete_file(&g_fs, SESSION_RAM_FILE);
}
