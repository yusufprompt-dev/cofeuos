/*
 * ============================================================================
 * STRING.H - Dize İşlemleri Kütüphanesi
 * ============================================================================
 */

#ifndef _STRING_H
#define _STRING_H

#include "types.h"

/* Temel dize fonksiyonları */
size_t strlen(const char* s);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
void strcpy(char* dest, const char* src);
void strncpy(char* dest, const char* src, size_t n);
char* strchr(const char* s, int c);
char* strstr(const char* haystack, const char* needle);
int memcmp(const void* s1, const void* s2, size_t n);
void* memcpy(void* dest, const void* src, size_t n);
void* memset(void* s, int c, size_t n);
void* memmove(void* dest, const void* src, size_t n);

/* Gelişmiş dize fonksiyonları */
char* strcat(char* dest, const char* src);
char* strncat(char* dest, const char* src, size_t n);
int strcmp_ci(const char* s1, const char* s2);  /* Case-insensitive */
int strncmp_ci(const char* s1, const char* s2, size_t n);
char to_upper(char c);
char to_lower(char c);
char* strtok(char* str, const char* delimiters);
char* strrchr(const char* s, int c);

/* Yol işlemleri */
char* path_basename(const char* path);
void path_parent(const char* path, char* parent);
int path_append(char* base, const char* component);

#endif /* _STRING_H */