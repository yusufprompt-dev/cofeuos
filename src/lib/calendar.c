#include "../include/calendar.h"
#include "../include/time.h"

static calendar_time_t g_time;

void calendar_init(void) {
    /* UEFI RTC'den gercek zaman al, eger basarisizsa varsayilan kullan */
    time_t ts;
    if (time_get_unix(&ts) == 0 && ts > 1000000000) {
        calendar_from_unix((u64)ts, &g_time);
    } else {
        g_time.year = 2024;
        g_time.month = 1;
        g_time.day = 1;
        g_time.hour = 0;
        g_time.minute = 0;
        g_time.second = 0;
        g_time.day_of_week = 0;
    }
}

int calendar_is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int calendar_days_in_month(int year, int month) {
    int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 0;
    int d = days[month - 1];
    if (month == 2 && calendar_is_leap_year(year)) d = 29;
    return d;
}

int calendar_day_of_week(int year, int month, int day) {
    if (month < 3) { month += 12; year--; }
    int q = day;
    int m = month;
    int k = year % 100;
    int j = year / 100;
    int h = (q + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
    return (h + 6) % 7;
}

void calendar_get_time(calendar_time_t *out) {
    if (out) *out = g_time;
}

void calendar_set_time(int year, int month, int day, int hour, int minute, int second) {
    g_time.year = year;
    g_time.month = month;
    g_time.day = day;
    g_time.hour = hour;
    g_time.minute = minute;
    g_time.second = second;
    g_time.day_of_week = calendar_day_of_week(year, month, day);
}

void calendar_print_month(int year, int month) {
    (void)year; (void)month;
}

void calendar_print_calendar(int year, int month) {
    (void)year; (void)month;
}

u64 calendar_to_unix(calendar_time_t *t) {
    if (!t) return 0;
    int y = t->year;
    int m = t->month;
    int d = t->day;
    if (m <= 2) { y--; m += 12; }
    u64 days = (u64)(365 * y + y / 4 - y / 100 + y / 400 + (153 * (m - 3) + 2) / 5 + d - 719468);
    return days * 86400 + t->hour * 3600 + t->minute * 60 + t->second;
}

void calendar_from_unix(u64 unix_time, calendar_time_t *out) {
    if (!out) return;
    u64 days = unix_time / 86400;
    u64 secs = unix_time % 86400;
    out->hour = (int)(secs / 3600);
    out->minute = (int)((secs % 3600) / 60);
    out->second = (int)(secs % 60);
    u64 z = days + 719468;
    u64 era = z / 146097;
    u64 doe = z - era * 146097;
    u64 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    u64 y = yoe + era * 400;
    u64 doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    u64 mp = (5 * doy + 2) / 153;
    out->day = (int)(doy - (153 * mp + 2) / 5 + 1);
    out->month = (int)(mp + 3 - 12 * (mp / 10));
    out->year = (int)(y);
    out->day_of_week = calendar_day_of_week(out->year, out->month, out->day);
}
