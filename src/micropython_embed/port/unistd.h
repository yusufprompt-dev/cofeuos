#pragma once
typedef __SIZE_TYPE__ size_t;
long write(int fd, const void *buf, size_t count);
long read(int fd, void *buf, size_t count);
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
