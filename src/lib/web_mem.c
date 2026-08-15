/*
 * WEB_MEM.C - CofeuTarayici Web Motoru Bellek Katmani
 *
 * web_malloc/web_free platforma ozeldir; OS icin kernel arena'sini kullanir.
 * (Native testlerde bu dosya yerine malloc/free saglanir.)
 */
#include "../include/web.h"
#include "../include/memory.h"

extern memory_arena g_mem_arena;

void *web_malloc(unsigned int size) {
    return kmalloc(&g_mem_arena, size);
}

void web_free(void *ptr) {
    kfree(&g_mem_arena, ptr);
}
