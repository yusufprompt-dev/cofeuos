/*
 * ============================================================================
 * TYPES.H - Temel Veri Tipleri ve Tanımlamalar
 * ============================================================================
 */

#ifndef _TYPES_H
#define _TYPES_H

/* Temel veri tipleri */
typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;
typedef signed char         s8;
typedef signed short        s16;
typedef signed int          s32;
typedef signed long long    s64;
typedef __SIZE_TYPE__ size_t;
typedef int                 ssize_t;

/* Boolean tipi */
#ifndef __bool_true_false_are_defined
#define __bool_true_false_are_defined 1
#define bool _Bool
#define true  1
#define false 0
#endif

/* NULL tanımı */
#ifndef NULL
#define NULL ((void*)0)
#endif

/* Hata kodları */
#define OK          0
#define ERROR       -1
#define ENOENT      -2   /* Dosya/dizin yok */
#define EEXIST      -3   /* Zaten var */
#define EPERM       -4   /* İzin yok */
#define ENOSPC      -5   /* Alan yok */
#define EINVAL      -6   /* Geçersiz parametre */
#define EIO         -7   /* Giriş/çıkış hatası */

/* Dosya modları */
#define O_RDONLY    0x00
#define O_WRONLY    0x01
#define O_RDWR      0x02
#define O_CREAT     0x40
#define O_APPEND    0x80

/* Seek modları */
#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

/* İzin bayrakları */
#define S_IRUSR     0x100   /* Kullanıcı okuma */
#define S_IWUSR     0x200   /* Kullanıcı yazma */
#define S_IXUSR     0x400   /* Kullanıcı çalıştırma */
#define S_IRGRP     0x010   /* Grup okuma */
#define S_IWGRP     0x020   /* Grup yazma */
#define S_IXGRP     0x040   /* Grup çalıştırma */
#define S_IROTH     0x001   /* Diğerleri okuma */
#define S_IWOTH     0x002   /* Diğerleri yazma */
#define S_IXOTH     0x004   /* Diğerleri çalıştırma */

#endif /* _TYPES_H */