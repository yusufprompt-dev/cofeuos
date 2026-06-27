/*
 * UEFI stdlib stub - malloc, free, printf için
 */
#include <efi.h>

/* Basit bump allocator */
static char heap[512 * 1024]; /* 512KB */
static size_t heap_pos = 0;

void *malloc(size_t size) {
    size = (size + 15) & ~15; /* 16 byte hizala */
    if (heap_pos + size > sizeof(heap)) return (void*)0;
    void *ptr = &heap[heap_pos];
    heap_pos += size;
    return ptr;
}

void *calloc(size_t n, size_t size) {
    void *ptr = malloc(n * size);
    if (ptr) {
        char *p = ptr;
        for (size_t i = 0; i < n * size; i++) p[i] = 0;
    }
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    /* Basit: yeni alan al, kopyala */
    void *new = malloc(size);
    if (ptr && new) {
        char *s = ptr, *d = new;
        for (size_t i = 0; i < size; i++) d[i] = s[i];
    }
    return new;
}

void free(void *ptr) {
    (void)ptr; /* bump allocator'da free yok */
}




void abort(void) {
    while (1) __asm__ volatile("hlt");
}

void exit(int code) {
    (void)code;
    while (1) __asm__ volatile("hlt");
}
