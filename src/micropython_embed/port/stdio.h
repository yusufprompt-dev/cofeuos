#pragma once
/* stdio stub for UEFI */
typedef void FILE;
#define NULL ((void*)0)
static inline int printf(const char *fmt, ...) { (void)fmt; return 0; }
static inline int fprintf(FILE *f, const char *fmt, ...) { (void)f; (void)fmt; return 0; }
static inline int snprintf(char *buf, int n, const char *fmt, ...) { (void)buf; (void)n; (void)fmt; return 0; }
