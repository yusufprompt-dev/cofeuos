#ifndef PKGMGR_H
#define PKGMGR_H

#include "types.h"

#define PKG_MAX_PACKAGES   64
#define PKG_NAME_MAX_LEN   32
#define PKG_VERSION_MAX    16
#define PKG_DESC_MAX       128
#define PKG_REPOS_MAX      4
#define PKG_INSTALLED_MAX  32

typedef struct {
    char name[PKG_NAME_MAX_LEN];
    char version[PKG_VERSION_MAX];
    char description[PKG_DESC_MAX];
    u32  size_kb;
    u8   repo;
    u8   installed;
    u8   mandatory;
} pkg_info_t;

typedef struct {
    char name[PKG_NAME_MAX_LEN];
    char version[PKG_VERSION_MAX];
    u32  size_kb;
    u8   repo;
} pkg_installed_t;

void pkg_init(void);
int  pkg_search(const char *query, pkg_info_t *results, int max_results);
int  pkg_install(const char *name);
int  pkg_remove(const char *name);
int  pkg_update(void);
int  pkg_list_installed(pkg_installed_t *list, int max_entries);
int  pkg_info(const char *name, pkg_info_t *out);
int  pkg_sync(void);
int  pkg_upgrade(void);

#endif
