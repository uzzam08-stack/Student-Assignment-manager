#include "date_utils.h"
#include <stdio.h>
#include <string.h>

bool date_is_leap_year(int year) {
    if (year <= 0) {
        return false;
    }
    return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

int date_days_in_month(int year, int month) {
    if (year <= 0 || month < 1 || month > 12) {
        return -1;
    }

    static const int days_per_month[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    if (month == 2) {
        return date_is_leap_year(year) ? 29 : 28;
    }

    return days_per_month[month - 1];
}

bool date_is_valid(const Date *date) {
    if (date == NULL) {
        return false;
    }
    if (date->year <= 0 || date->year > 9999) {
        return false;
    }
    if (date->month < 1 || date->month > 12) {
        return false;
    }
    int max_days = date_days_in_month(date->year, date->month);
    if (max_days < 0 || date->day < 1 || date->day > max_days) {
        return false;
    }
    return true;
}

int date_create(int year, int month, int day, Date *out_date) {
    if (out_date == NULL) {
        return -1;
    }
    Date temp = { year, month, day };
    if (!date_is_valid(&temp)) {
        return -1;
    }
    *out_date = temp;
    return 0;
}

int date_compare(const Date *d1, const Date *d2) {
    if (d1 == d2) {
        return 0;
    }
    if (d1 == NULL) {
        return -1;
    }
    if (d2 == NULL) {
        return 1;
    }

    if (d1->year != d2->year) {
        return (d1->year > d2->year) ? 1 : -1;
    }
    if (d1->month != d2->month) {
        return (d1->month > d2->month) ? 1 : -1;
    }
    if (d1->day != d2->day) {
        return (d1->day > d2->day) ? 1 : -1;
    }
    return 0;
}

int date_format(const Date *date, char *buf, size_t buf_size) {
    if (date == NULL || buf == NULL || buf_size < 11) {
        return -1;
    }
    if (!date_is_valid(date)) {
        return -1;
    }
    int written = snprintf(buf, buf_size, "%04d-%02d-%02d", date->year, date->month, date->day);
    if (written < 0 || (size_t)written >= buf_size) {
        return -1;
    }
    return written;
}

int date_parse(const char *str, Date *out_date) {
    if (str == NULL || out_date == NULL) {
        return -1;
    }

    /* Expecting exactly "YYYY-MM-DD" which has length 10 */
    if (strlen(str) != 10) {
        return -1;
    }
    if (str[4] != '-' || str[7] != '-') {
        return -1;
    }

    int year = 0;
    int month = 0;
    int day = 0;
    char trailing = '\0';

    if (sscanf(str, "%4d-%2d-%2d%c", &year, &month, &day, &trailing) != 3) {
        return -1;
    }

    return date_create(year, month, day, out_date);
}
