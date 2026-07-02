/*
 * ============================================================================
 * STRING.C - Dize İşlemleri Kütüphanesi Uygulamaları
 * ============================================================================
 */

#include "../include/string.h"
#include "../include/types.h"

/* Temel dize fonksiyonları */

size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (int)((unsigned char)*s1 - (unsigned char)*s2);
}

int strncmp(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return (int)((unsigned char)s1[i] - (unsigned char)s2[i]);
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

void strcpy(char* dest, const char* src) {
    while ((*dest++ = *src++));
}

void strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
}

char* strchr(const char* s, int c) {
    while (*s != (char)c) {
        if (*s == '\0') return NULL;
        s++;
    }
    return (char*)s;
}

char* strstr(const char* haystack, const char* needle) {
    if (*needle == '\0') return (char*)haystack;
    while (*haystack != '\0') {
        const char* h = haystack;
        const char* n = needle;
        while (*n != '\0' && *h == *n) {
            h++;
            n++;
        }
        if (*n == '\0') return (char*)haystack;
        haystack++;
    }
    return NULL;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return (int)p1[i] - (int)p2[i];
        }
    }
    return 0;
}

void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

void* memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;
    for (size_t i = 0; i < n; i++) {
        p[i] = (unsigned char)c;
    }
    return s;
}

void* memmove(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    
    if (d < s) {
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = n; i > 0; i--) {
            d[i-1] = s[i-1];
        }
    }
    return dest;
}

/* Gelişmiş dize fonksiyonları */

char* strcat(char* dest, const char* src) {
    char* d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

char* strncat(char* dest, const char* src, size_t n) {
    char* d = dest;
    while (*d) d++;
    
    for (size_t i = 0; i < n && src[i] != '\0'; i++) {
        *d++ = src[i];
    }
    *d = '\0';
    return dest;
}

int strcmp_ci(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        char c1 = to_upper(*s1++);
        char c2 = to_upper(*s2++);
        if (c1 != c2) return (int)((unsigned char)c1 - (unsigned char)c2);
    }
    return (int)((unsigned char)to_upper(*s1) - (unsigned char)to_upper(*s2));
}

int strncmp_ci(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char c1 = to_upper(s1[i]);
        char c2 = to_upper(s2[i]);
        if (c1 != c2) return (int)((unsigned char)c1 - (unsigned char)c2);
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

char to_upper(char c) {
    if (c >= 'a' && c <= 'z') return c - ('a' - 'A');
    return c;
}

char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

/* String tokenizer */
char* strtok(char* str, const char* delimiters) {
    static char* last_token = NULL;
    char* token;
    
    if (str != NULL) {
        last_token = str;
    }
    
    if (last_token == NULL) {
        return NULL;
    }
    
    /* Başlangıçtaki delimiter karakterlerini atla */
    while (*last_token && strchr(delimiters, *last_token)) {
        last_token++;
    }
    
    if (*last_token == '\0') {
        last_token = NULL;
        return NULL;
    }
    
    token = last_token;
    
    /* Token sonunu bul */
    while (*last_token && !strchr(delimiters, *last_token)) {
        last_token++;
    }
    
    if (*last_token != '\0') {
        *last_token = '\0';
        last_token++;
    }
    
    return token;
}

/* Son karakteri bul */
char* strrchr(const char* s, int c) {
    const char* last = NULL;
    
    while (*s) {
        if (*s == (char)c) {
            last = s;
        }
        s++;
    }
    
    return (char*)last;
}

/* Yol işlemleri */

char* path_basename(const char* path) {
    const char* last_slash = NULL;
    const char* p = path;
    
    while (*p) {
        if (*p == '/') last_slash = p;
        p++;
    }
    
    if (last_slash) {
        return (char*)(last_slash + 1);
    }
    return (char*)path;
}

void path_parent(const char* path, char* parent) {
    const char* last_slash = NULL;
    const char* p = path;
    
    while (*p) {
        if (*p == '/') last_slash = p;
        p++;
    }
    
    if (last_slash == NULL || last_slash == path) {
        parent[0] = '/';
        parent[1] = '\0';
    } else {
        size_t len = last_slash - path;
        for (size_t i = 0; i < len; i++) {
            parent[i] = path[i];
        }
        parent[len] = '\0';
    }
}

int path_append(char* base, const char* component) {
    size_t base_len = strlen(base);
    size_t comp_len = strlen(component);
    
    if (base_len + 1 + comp_len >= 256) return -1; /* PATH_MAX */
    
    if (base[base_len - 1] != '/') {
        base[base_len] = '/';
        base_len++;
    }
    
    for (size_t i = 0; i < comp_len; i++) {
        base[base_len + i] = component[i];
    }
    base[base_len + comp_len] = '\0';
    
    return 0;
}