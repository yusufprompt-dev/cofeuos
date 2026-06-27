#pragma once
/* stdlib stub for UEFI */
void *malloc(unsigned long size);
void *calloc(unsigned long n, unsigned long size);
void *realloc(void *ptr, unsigned long size);
void free(void *ptr);
void abort(void);
void exit(int code);
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
