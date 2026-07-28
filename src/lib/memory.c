/*
 * ============================================================================
 * MEMORY.C - Bellek Yönetimi Uygulamaları
 * ============================================================================
 */

#include "../include/memory.h"
#include "../include/string.h"

/* Bellek bloğu boyutu */
#define BLOCK_SIZE sizeof(memory_block)

/* Bellek alanını başlat */
void mem_init(memory_arena* arena, void* start, size_t size) {
    arena->start = start;
    arena->total_size = size;
    
    /* İlk bloğu oluştur */
    arena->first_block = (memory_block*)start;
    arena->first_block->size = size - BLOCK_SIZE;
    arena->first_block->free = true;
    arena->first_block->next = NULL;
    arena->first_block->prev = NULL;
}

/* Bellek ayır */
void* kmalloc(memory_arena* arena, size_t size) {
    if (size == 0) return NULL;
    
    /* 16 byte hizalama (cache line uyumlu) */
    size = (size + 15) & ~15;
    
    memory_block* current = arena->first_block;
    
    while (current != NULL) {
        if (current->free && current->size >= size) {
            /* Blok yeterli büyüklükte */
            
            if (current->size >= size + BLOCK_SIZE + 8) {
                /* Blok yeterince büyükse böl */
                memory_block* new_block = (memory_block*)((u8*)current + BLOCK_SIZE + size);
                new_block->size = current->size - size - BLOCK_SIZE;
                new_block->free = true;
                new_block->next = current->next;
                new_block->prev = current;
                
                if (current->next != NULL) {
                    current->next->prev = new_block;
                }
                current->next = new_block;
                current->size = size;
            }
            
            current->free = false;
            return (void*)((u8*)current + BLOCK_SIZE);
        }
        current = current->next;
    }
    
    return NULL; /* Yeterli bellek yok */
}

/* Belleği serbest bırak */
void kfree(memory_arena* arena, void* ptr) {
    if (ptr == NULL) return;
    
    memory_block* block = (memory_block*)((u8*)ptr - BLOCK_SIZE);
    block->free = true;
    
    /* Birleştirme: önceki blokla birleştir */
    if (block->prev != NULL && block->prev->free) {
        block->prev->size += BLOCK_SIZE + block->size;
        block->prev->next = block->next;
        if (block->next != NULL) {
            block->next->prev = block->prev;
        }
        block = block->prev;
    }
    
    /* Birleştirme: sonraki blokla birleştir */
    if (block->next != NULL && block->next->free) {
        block->size += BLOCK_SIZE + block->next->size;
        block->next = block->next->next;
        if (block->next != NULL) {
            block->next->prev = block;
        }
    }
}

/* Belleği yeniden boyutlandır */
void* krealloc(memory_arena* arena, void* ptr, size_t new_size) {
    if (ptr == NULL) return kmalloc(arena, new_size);
    if (new_size == 0) {
        kfree(arena, ptr);
        return NULL;
    }
    
    memory_block* block = (memory_block*)((u8*)ptr - BLOCK_SIZE);
    
    /* Yeni boyut mevcut blok boyutundan küçükse */
    if (new_size <= block->size) {
        if (block->size >= new_size + BLOCK_SIZE + 8) {
            /* Blok yeterince büyükse böl */
            memory_block* new_block = (memory_block*)((u8*)block + BLOCK_SIZE + new_size);
            new_block->size = block->size - new_size - BLOCK_SIZE;
            new_block->free = true;
            new_block->next = block->next;
            new_block->prev = block;
            
            if (block->next != NULL) {
                block->next->prev = new_block;
            }
            block->next = new_block;
            block->size = new_size;
        }
        return ptr;
    }
    
    /* Yeni boyut mevcut bloktan büyükse */
    void* new_ptr = kmalloc(arena, new_size);
    if (new_ptr != NULL) {
        memcpy(new_ptr, ptr, block->size);
        kfree(arena, ptr);
    }
    return new_ptr;
}

/* Sıfırlanmış bellek ayır */
void* kcalloc(memory_arena* arena, size_t nmemb, size_t size) {
    size_t total_size = nmemb * size;
    void* ptr = kmalloc(arena, total_size);
    if (ptr != NULL) {
        memset(ptr, 0, total_size);
    }
    return ptr;
}

/* Boş bellek miktarını döndür */
size_t mem_free_space(memory_arena* arena) {
    size_t free_space = 0;
    memory_block* current = arena->first_block;
    
    while (current != NULL) {
        if (current->free) {
            free_space += current->size;
        }
        current = current->next;
    }
    
    return free_space;
}

/* Kullanılan bellek miktarını döndür */
size_t mem_used_space(memory_arena* arena) {
    return arena->total_size - mem_free_space(arena);
}