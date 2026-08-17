#ifndef CALENDAR_H
#define CALENDAR_H

#include "types.h"

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int day_of_week;
} calendar_time_t;

void calendar_init(void);
void calendar_get_time(calendar_time_t *out);
void calendar_set_time(int year, int month, int day, int hour, int minute, int second);
void calendar_print_month(int year, int month);
void calendar_print_calendar(int year, int month);
int  calendar_days_in_month(int year, int month);
int  calendar_day_of_week(int year, int month, int day);
int  calendar_is_leap_year(int year);
u64  calendar_to_unix(calendar_time_t *t);
void calendar_from_unix(u64 unix_time, calendar_time_t *out);

#endif
