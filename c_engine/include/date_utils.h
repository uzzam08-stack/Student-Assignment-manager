#ifndef DATE_UTILS_H
#define DATE_UTILS_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Calendar Date structure representing year, month (1-12), and day (1-31).
 */
typedef struct {
    int year;
    int month;
    int day;
} Date;

/**
 * Determines whether a given year is a leap year in the Gregorian calendar.
 * Rule: Divisible by 4, but not by 100 unless also divisible by 400.
 *
 * @param year The year to test (must be positive).
 * @return true if leap year, false otherwise.
 */
bool date_is_leap_year(int year);

/**
 * Returns the maximum number of days in the specified month of a given year.
 * Correctly accounts for February in leap years.
 *
 * @param year  The year (must be positive).
 * @param month The month (1-12).
 * @return Number of days in the month (28, 29, 30, or 31), or -1 if invalid.
 */
int date_days_in_month(int year, int month);

/**
 * Validates whether a Date represents a valid calendar date.
 *
 * @param date Pointer to the Date structure to validate.
 * @return true if valid, false if NULL or invalid.
 */
bool date_is_valid(const Date *date);

/**
 * Creates and initializes a validated Date structure.
 *
 * @param year     The year (> 0).
 * @param month    The month (1-12).
 * @param day      The day (1-31, valid for the month/year).
 * @param out_date Output pointer to store the validated date.
 * @return 0 on success, negative value on validation failure or NULL pointer.
 */
int date_create(int year, int month, int day, Date *out_date);

/**
 * Compares two dates chronologically.
 *
 * @param d1 Pointer to first date.
 * @param d2 Pointer to second date.
 * @return Negative if d1 < d2 (d1 is earlier),
 *         Positive if d1 > d2 (d1 is later),
 *         Zero if d1 and d2 represent the same calendar day.
 */
int date_compare(const Date *d1, const Date *d2);

/**
 * Formats a Date struct as a null-terminated string "YYYY-MM-DD".
 *
 * @param date     Pointer to the Date structure.
 * @param buf      Output character buffer.
 * @param buf_size Size of the output buffer (must be at least 11 bytes).
 * @return Number of characters written on success (excluding null byte),
 *         or negative error code on failure.
 */
int date_format(const Date *date, char *buf, size_t buf_size);

/**
 * Parses a date string in "YYYY-MM-DD" format and validates it.
 *
 * @param str      Input string to parse.
 * @param out_date Output pointer to store the parsed Date.
 * @return 0 on success, negative error code on failure.
 */
int date_parse(const char *str, Date *out_date);

#endif /* DATE_UTILS_H */
