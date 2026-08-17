/*
 * CofeuOS Time abstraction (UEFI RTC / POSIX fallback)
 */
#ifndef TIME_H
#define TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t time_t;

/* Initialize time subsystem (UEFI: pass RuntimeServices pointer) */
void time_uefi_init(void *runtime_services);

/* Get current Unix timestamp (seconds since 1970-01-01 UTC)
 * Returns 0 on success, -1 on error */
int time_get_unix(time_t *out_ts);

#ifdef __cplusplus
}
#endif

#endif /* TIME_H */