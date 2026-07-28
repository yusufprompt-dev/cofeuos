/*
 * ============================================================================
 * FS.H - Dosya Sistemi Katmanı
 * ============================================================================
 */

#ifndef _FS_H
#define _FS_H

#include "types.h"
#include "string.h"

#define MAX_PATH_LEN    256
#define MAX_FILENAME    64
#define MAX_FILES       64
#define MAX_DIRS        32
/* curl ile indirilen küçük web sayfalarını kesmeden tutabilmek için 16 KiB. */
#define FILE_CONTENT_SIZE 16384

/* Dosya türleri */
typedef enum {
    FILE_TYPE_REGULAR = 0,
    FILE_TYPE_DIRECTORY = 1,
    FILE_TYPE_DEVICE = 2
} file_type;

/* Dosya bilgisi */
typedef struct {
    char name[MAX_FILENAME];
    char path[MAX_PATH_LEN];
    file_type type;
    size_t size;
    u32 permissions;
    u32 owner;
    u32 group;
    bool active;
    char content[FILE_CONTENT_SIZE + 1]; /* NUL sonlandırıcısı için ek bayt */
} file_info;

/* Dizin bilgisi */
typedef struct {
    char path[MAX_PATH_LEN];
    bool active;
} dir_info;

/* Dosya sistemi kontrol bloğu */
typedef struct {
    file_info files[MAX_FILES];
    dir_info dirs[MAX_DIRS];
    size_t file_count;
    size_t dir_count;
    u32 next_uid;
    u32 next_gid;
} fs_control_block;

/* Dosya sistemi fonksiyonları */
int fs_init(fs_control_block* fs);
int fs_create_file(fs_control_block* fs, const char* path, const char* content, size_t size);
int fs_create_dir(fs_control_block* fs, const char* path);
int fs_read_file(fs_control_block* fs, const char* path, char* buffer, size_t size);
int fs_write_file(fs_control_block* fs, const char* path, const char* content, size_t size);
int fs_delete_file(fs_control_block* fs, const char* path);
int fs_delete_dir(fs_control_block* fs, const char* path);
int fs_list_dir(fs_control_block* fs, const char* path, char* buffer, size_t size);
bool fs_file_exists(fs_control_block* fs, const char* path);
bool fs_dir_exists(fs_control_block* fs, const char* path);

/* Yardımcı fonksiyonlar */
int fs_resolve_path(const char* base, const char* path, char* resolved);
int fs_get_parent_path(const char* path, char* parent);
int fs_get_filename(const char* path, char* filename);

#endif /* _FS_H */
