/*
 * CofeuOS Time abstraction (UEFI RTC / POSIX fallback)
 *
 * malloc yok; sabit fonksiyonlar.
 */
#include <stdint.h>
#include "../include/string.h"
#include "../include/time.h"

#ifdef UEFI_BUILD
/* UEFI RTC (Runtime Services) */
#include <efi.h>
#include <efilib.h>

static EFI_RUNTIME_SERVICES *g_rt = NULL;

void time_uefi_init(void *runtime_services) {
    g_rt = (EFI_RUNTIME_SERVICES *)runtime_services;
}

int time_get_unix(time_t *out_ts) {
    if (!g_rt) return -1;
    EFI_TIME t;
    EFI_STATUS st = g_rt->GetTime(&t, NULL);
    if (EFI_ERROR(st)) return -1;

    /* EFI_TIME -> Unix timestamp (1970-01-01) */
    static const uint16_t days_before_month[12] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };

    int year = t.Year - 1970;
    int leap_days = (year + 1) / 4 - (year + 69) / 100 + (year + 369) / 400;
    int days = year * 365 + leap_days + days_before_month[t.Month - 1] + (t.Day - 1);
    if (t.Month > 2 && ((t.Year % 4 == 0 && t.Year % 100 != 0) || t.Year % 400 == 0))
        days++;

    time_t ts = ((time_t)days * 86400) + t.Hour * 3600 + t.Minute * 60 + t.Second;
    *out_ts = ts;
    return 0;
}

#else
/* POSIX / host fallback */
#include <time.h>

void time_uefi_init(void *runtime_services) {
    (void)runtime_services;
}

int time_get_unix(time_t *out_ts) {
    *out_ts = time(NULL);
    return 0;
}

#endif