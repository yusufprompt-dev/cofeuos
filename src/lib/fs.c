/*
 * ============================================================================
 * FS.C - Dosya Sistemi Katmanı Uygulamaları
 * ============================================================================
 */

#include "../include/fs.h"
#include "../include/string.h"
#include "../include/memory.h"

/* Not: g_fs main.c'de tanımlıdır */

/* Dosya sistemi başlat */
int fs_init(fs_control_block* fs) {
    /* Belleği temizle */
    memset(fs, 0, sizeof(fs_control_block));
    
    /* Kök dizini oluştur */
    strcpy(fs->dirs[0].path, "/");
    fs->dirs[0].active = true;
    fs->dir_count = 1;
    
    /* Sistem dosyaları */
    fs->next_uid = 1000;
    fs->next_gid = 1000;
    
    /* Kök dizin yapısı */
    fs_create_dir(fs, "/dev");
    fs_create_dir(fs, "/etc");
    fs_create_dir(fs, "/mnt");
    
    /* Örnek dosyalar ekle */
    fs_create_file(fs, "/readme.txt", "cofeuOS v3.0 - Geliştirilmiş Kernel", 32);
    fs_create_file(fs, "/etc/hostname", "cofeu", 5);
    fs_create_file(fs, "/etc/fstab", "/dev/sda1 / ext4 defaults 0 1", 28);
    fs_create_file(fs, "/etc/motd", "Welcome to cofeuOS v3.0", 23);
    fs_create_file(fs, "/dev/sda1", "Virtual partition device", 24);
    
    return 0;
}

static int fs_ensure_parent_dirs(fs_control_block* fs, const char* path) {
    char parent[MAX_PATH_LEN];
    if (fs_get_parent_path(path, parent) != 0) {
        return -1;
    }

    if (strcmp(parent, "/") == 0 || strcmp(parent, ".") == 0) {
        return 0;
    }

    if (fs_dir_exists(fs, parent)) {
        return 0;
    }

    if (fs_ensure_parent_dirs(fs, parent) != 0) {
        return -1;
    }

    return fs_create_dir(fs, parent);
}

/* Dosya oluştur */
int fs_create_file(fs_control_block* fs, const char* path, const char* content, size_t size) {
    /* Yolu çöz */
    char resolved[MAX_PATH_LEN];
    if (fs_resolve_path("/", path, resolved) != 0) {
        return -1; /* Geçersiz yol */
    }
    
    /* Üst dizinleri oluştur */
    char parent[MAX_PATH_LEN];
    fs_get_parent_path(resolved, parent);
    if (!fs_dir_exists(fs, parent)) {
        if (fs_ensure_parent_dirs(fs, resolved) != 0) {
            return -2; /* Dizin oluşturulamadı */
        }
    }
    
    /* Dosya zaten var mı kontrol et */
    if (fs_file_exists(fs, resolved)) {
        return -3; /* Dosya zaten var */
    }
    
    /* Yeni dosya ekle */
    if (fs->file_count >= MAX_FILES) {
        return -4; /* Dosya limiti */
    }
    
    file_info* file = &fs->files[fs->file_count++];
    
    /* Dosya adını al */
    fs_get_filename(resolved, file->name);
    strcpy(file->path, resolved);
    file->type = FILE_TYPE_REGULAR;
    file->size = (size > FILE_CONTENT_SIZE) ? FILE_CONTENT_SIZE : size;
    file->permissions = 0644; /* rw-r--r-- */
    file->owner = 0; /* root */
    file->group = 0; /* root */
    file->active = true;
    
    /* İçeriği kopyala */
    if (content != NULL && size > 0) {
        size_t copy_size = (size > FILE_CONTENT_SIZE) ? FILE_CONTENT_SIZE : size;
        memcpy(file->content, content, copy_size);
        file->content[copy_size] = '\0';
    } else {
        file->content[0] = '\0';
    }
    
    return 0;
}

/* Dizin oluştur */
int fs_create_dir(fs_control_block* fs, const char* path) {
    /* Yolu çöz */
    char resolved[MAX_PATH_LEN];
    if (fs_resolve_path("/", path, resolved) != 0) {
        return -1; /* Geçersiz yol */
    }
    
    /* Dizin zaten var mı kontrol et */
    if (fs_dir_exists(fs, resolved)) {
        return -2; /* Dizin zaten var */
    }
    
    /* Dizin limiti kontrol et */
    if (fs->dir_count >= MAX_DIRS) {
        return -3; /* Dizin limiti */
    }
    
    /* Üst dizin var mı kontrol et */
    char parent[MAX_PATH_LEN];
    fs_get_parent_path(resolved, parent);
    if (strcmp(parent, "/") != 0 && !fs_dir_exists(fs, parent)) {
        return -4; /* Üst dizin yok */
    }
    
    /* Yeni dizin ekle */
    dir_info* dir = &fs->dirs[fs->dir_count++];
    strcpy(dir->path, resolved);
    dir->active = true;
    
    return 0;
}

/* Dosya oku */
int fs_read_file(fs_control_block* fs, const char* path, char* buffer, size_t size) {
    /* Yolu çöz */
    char resolved[MAX_PATH_LEN];
    if (fs_resolve_path("/", path, resolved) != 0) {
        return -1; /* Geçersiz yol */
    }
    
    /* Dosya var mı kontrol et */
    for (size_t i = 0; i < fs->file_count; i++) {
        if (fs->files[i].active && strcmp(fs->files[i].path, resolved) == 0) {
            /* size-1: null terminator için yer bırak (buffer overflow önlemi) */
            size_t max_read = (size > 0) ? size - 1 : 0;
            size_t copy_size = (max_read < fs->files[i].size) ? max_read : fs->files[i].size;
            memcpy(buffer, fs->files[i].content, copy_size);
            buffer[copy_size] = '\0';
            return (int)copy_size;
        }
    }
    
    return -2; /* Dosya yok */
}

/* Dosya yaz */
int fs_write_file(fs_control_block* fs, const char* path, const char* content, size_t size) {
    /* Yolu çöz */
    char resolved[MAX_PATH_LEN];
    if (fs_resolve_path("/", path, resolved) != 0) {
        return -1; /* Geçersiz yol */
    }
    
    /* Dosya var mı kontrol et */
    for (size_t i = 0; i < fs->file_count; i++) {
        if (fs->files[i].active && strcmp(fs->files[i].path, resolved) == 0) {
            size_t copy_size = (size > FILE_CONTENT_SIZE) ? FILE_CONTENT_SIZE : size;
            memcpy(fs->files[i].content, content, copy_size);
            fs->files[i].content[copy_size] = '\0';
            fs->files[i].size = copy_size;
            return (int)copy_size;
        }
    }
    
    /* Dosya yoksa oluştur */
    return fs_create_file(fs, resolved, content, size);
}

/* Dosya sil */
int fs_delete_file(fs_control_block* fs, const char* path) {
    /* Yolu çöz */
    char resolved[MAX_PATH_LEN];
    if (fs_resolve_path("/", path, resolved) != 0) {
        return -1; /* Geçersiz yol */
    }
    
    /* Dosya var mı kontrol et */
    for (size_t i = 0; i < fs->file_count; i++) {
        if (fs->files[i].active && strcmp(fs->files[i].path, resolved) == 0) {
            fs->files[i].active = false;
            return 0;
        }
    }
    
    return -2; /* Dosya yok */
}

/* Dizin sil */
int fs_delete_dir(fs_control_block* fs, const char* path) {
    /* Yolu çöz */
    char resolved[MAX_PATH_LEN];
    if (fs_resolve_path("/", path, resolved) != 0) {
        return -1; /* Geçersiz yol */
    }
    
    /* Dizin var mı kontrol et */
    for (size_t i = 0; i < fs->dir_count; i++) {
        if (fs->dirs[i].active && strcmp(fs->dirs[i].path, resolved) == 0) {
            /* Alt dosyaları ve dizinleri kontrol et */
            for (size_t j = 0; j < fs->file_count; j++) {
                if (fs->files[j].active && strncmp(fs->files[j].path, resolved, strlen(resolved)) == 0) {
                    return -3; /* Dizin içinde dosyalar var */
                }
            }
            
            for (size_t j = 0; j < fs->dir_count; j++) {
                if (fs->dirs[j].active && j != i && strncmp(fs->dirs[j].path, resolved, strlen(resolved)) == 0) {
                    return -4; /* Dizin içinde alt dizinler var */
                }
            }
            
            fs->dirs[i].active = false;
            return 0;
        }
    }
    
    return -2; /* Dizin yok */
}

/* Dizini listele */
int fs_list_dir(fs_control_block* fs, const char* path, char* buffer, size_t size) {
    /* Yolu çöz */
    char resolved[MAX_PATH_LEN];
    if (fs_resolve_path("/", path, resolved) != 0) {
        return -1; /* Geçersiz yol */
    }
    
    /* Dizin var mı kontrol et */
    if (!fs_dir_exists(fs, resolved)) {
        return -2; /* Dizin yok */
    }
    
    /* Dosyaları listele */
    size_t pos = 0;
    for (size_t i = 0; i < fs->file_count; i++) {
        if (fs->files[i].active) {
            char parent[MAX_PATH_LEN];
            fs_get_parent_path(fs->files[i].path, parent);
            if (strcmp(parent, resolved) == 0) {
                /* Basit string birleştirme */
                const char* name = fs->files[i].name;
                size_t name_len = strlen(name);
                if (pos + name_len + 1 >= size) {
                    return -3; /* Buffer taşması */
                }
                strcpy(&buffer[pos], name);
                pos += name_len;
                buffer[pos++] = ' ';
            }
        }
    }
    
    /* Alt dizinleri listele */
    for (size_t i = 0; i < fs->dir_count; i++) {
        if (fs->dirs[i].active) {
            char parent[MAX_PATH_LEN];
            fs_get_parent_path(fs->dirs[i].path, parent);
            if (strcmp(parent, resolved) == 0 && strcmp(fs->dirs[i].path, resolved) != 0) {
                /* Basit string birleştirme */
                const char* dir_name = fs->dirs[i].path + strlen(resolved) + 1;
                size_t name_len = strlen(dir_name);
                if (pos + name_len + 2 >= size) {
                    return -3; /* Buffer taşması */
                }
                strcpy(&buffer[pos], dir_name);
                pos += name_len;
                buffer[pos++] = '/';
                buffer[pos++] = ' ';
            }
        }
    }
    
    if (pos > 0 && buffer[pos-1] == ' ') {
        buffer[pos-1] = '\0';
    } else {
        buffer[pos] = '\0';
    }
    
    return (int)pos;
}

/* Dosya var mı */
bool fs_file_exists(fs_control_block* fs, const char* path) {
    char resolved[MAX_PATH_LEN];
    if (fs_resolve_path("/", path, resolved) != 0) {
        return false;
    }
    
    for (size_t i = 0; i < fs->file_count; i++) {
        if (fs->files[i].active && strcmp(fs->files[i].path, resolved) == 0) {
            return true;
        }
    }
    return false;
}

/* Dizin var mı */
bool fs_dir_exists(fs_control_block* fs, const char* path) {
    char resolved[MAX_PATH_LEN];
    if (fs_resolve_path("/", path, resolved) != 0) {
        return false;
    }
    
    for (size_t i = 0; i < fs->dir_count; i++) {
        if (fs->dirs[i].active && strcmp(fs->dirs[i].path, resolved) == 0) {
            return true;
        }
    }
    return false;
}

/* Yolu çöz */
int fs_resolve_path(const char* base, const char* path, char* resolved) {
    if (path == NULL || resolved == NULL) {
        return -1;
    }

    bool path_absolute = (path[0] == '/');
    bool result_absolute = path_absolute;  /* true if path is absolute, OR if base is absolute */
    
    if (path_absolute) {
        strcpy(resolved, path);
    } else {
        strcpy(resolved, base);
        result_absolute = (base[0] == '/');  /* inherit absolute status from base */
        if (resolved[strlen(resolved) - 1] != '/') {
            strcat(resolved, "/");
        }
        strcat(resolved, path);
    }

    char temp[MAX_PATH_LEN];
    strcpy(temp, resolved);
    resolved[0] = '\0';

    char* token = strtok(temp, "/");
    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            /* mevcut dizin - atla */
        } else if (strcmp(token, "..") == 0) {
            if (strcmp(resolved, "/") != 0 && resolved[0] != '\0') {
                char* last_slash = strrchr(resolved, '/');
                if (last_slash != NULL) {
                    if (last_slash == resolved) {
                        *(last_slash + 1) = '\0';
                    } else {
                        *last_slash = '\0';
                    }
                }
            }
        } else {
            if (resolved[0] == '\0') {
                if (result_absolute) {
                    strcpy(resolved, "/");
                }
            }
            if (strcmp(resolved, "/") != 0 && resolved[0] != '\0') {
                strcat(resolved, "/");
            }
            strcat(resolved, token);
        }
        token = strtok(NULL, "/");
    }

    if (resolved[0] == '\0') {
        strcpy(resolved, result_absolute ? "/" : ".");
    }

    return 0;
}

/* Üst dizini al */
int fs_get_parent_path(const char* path, char* parent) {
    if (path == NULL || parent == NULL) {
        return -1;
    }
    
    strcpy(parent, path);
    
    /* Son slash'ı bul */
    char* last_slash = strrchr(parent, '/');
    
    if (last_slash == NULL) {
        strcpy(parent, "/");
        return 0;
    }
    
    /* Son slash'ın konumunu bul */
    size_t slash_pos = last_slash - parent;
    
    if (slash_pos == 0) {
        /* Kök dizin */
        strcpy(parent, "/");
    } else {
        /* Son slash'ı sil */
        parent[slash_pos] = '\0';
        /* Eğer boş kalırsa kök dizin yap */
        if (parent[0] == '\0') {
            strcpy(parent, "/");
        }
    }
    
    return 0;
}

/* Dosya adını al */
int fs_get_filename(const char* path, char* filename) {
    if (path == NULL || filename == NULL) {
        return -1;
    }
    
    /* Son slash'ı bul */
    char* last_slash = strrchr(path, '/');
    
    if (last_slash == NULL) {
        /* Slash yok, tüm yol dosya adı */
        strcpy(filename, path);
    } else {
        /* Slash sonrası dosya adı */
        strcpy(filename, last_slash + 1);
    }
    
    return 0;
}