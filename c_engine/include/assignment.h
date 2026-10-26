#ifndef ASSIGNMENT_H
#define ASSIGNMENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "date_utils.h"

#define ASSIGNMENT_TITLE_MAX 128
#define ASSIGNMENT_SUBJECT_MAX 64

/**
 * Priority levels for assignments.
 * Higher numeric value represents strictly higher priority.
 */
typedef enum {
    PRIORITY_INVALID = 0,
    PRIORITY_LOW     = 1,
    PRIORITY_MEDIUM  = 2,
    PRIORITY_HIGH    = 3,
    PRIORITY_URGENT  = 4
} Priority;

#define PRIORITY_MIN PRIORITY_LOW
#define PRIORITY_MAX PRIORITY_URGENT

/**
 * Status of an assignment in the lifecycle.
 */
typedef enum {
    ASSIGNMENT_STATUS_PENDING   = 0,
    ASSIGNMENT_STATUS_COMPLETED = 1
} AssignmentStatus;

/**
 * Unified Status / Error Codes for the Student Assignment Priority Manager.
 */
typedef enum {
    SAPM_SUCCESS              =  0,
    SAPM_SUCCESS_FIRST_RUN    =  1,  /* Missing file handled cleanly as first-run */
    SAPM_ERR_NULL_PARAM       = -1,
    SAPM_ERR_INVALID_ID       = -2,
    SAPM_ERR_INVALID_TITLE    = -3,
    SAPM_ERR_INVALID_SUBJECT  = -4,
    SAPM_ERR_INVALID_DATE     = -5,
    SAPM_ERR_INVALID_PRIORITY = -6,
    SAPM_ERR_DUPLICATE_ID     = -7,
    SAPM_ERR_NOT_FOUND        = -8,
    SAPM_ERR_QUEUE_FULL       = -9,
    SAPM_ERR_QUEUE_EMPTY      = -10,
    SAPM_ERR_BUFFER_TOO_SMALL = -11,
    SAPM_ERR_INVALID_FORMAT   = -12,
    SAPM_ERR_FILE_NOT_FOUND   = -13,
    SAPM_ERR_FILE_OPEN        = -14,
    SAPM_ERR_FILE_READ        = -15,
    SAPM_ERR_FILE_WRITE       = -16,
    SAPM_ERR_FILE_CLOSE       = -17,
    SAPM_ERR_CORRUPT_RECORD   = -18,
    SAPM_ERR_HISTORY_FULL     = -19,
    SAPM_ERR_INVALID_STATUS   = -20
} SAPM_Status;

/**
 * Assignment data structure.
 */
typedef struct {
    int id;                                  /* Unique positive assignment ID */
    char title[ASSIGNMENT_TITLE_MAX];        /* Title of assignment */
    char subject[ASSIGNMENT_SUBJECT_MAX];    /* Subject or course code */
    Date deadline;                           /* Due date (calendar validated) */
    Priority priority;                       /* Priority level */
    uint64_t sequence_num;                   /* Monotonic arrival order counter */
    AssignmentStatus status;                 /* Pending or Completed */
} Assignment;

/**
 * Returns human-readable string representation of a status/error code.
 */
const char *sapm_status_to_string(SAPM_Status status);

/**
 * Validates whether a Priority value is within valid range.
 */
bool priority_is_valid(Priority priority);

/**
 * Converts a Priority enum to its string representation ("LOW", "MEDIUM", etc.).
 */
const char *priority_to_string(Priority priority);

/**
 * Parses a string (case-insensitive or numeric) into a Priority enum.
 *
 * @param str          Input string (e.g., "HIGH", "High", "3").
 * @param out_priority Output pointer to store the parsed Priority.
 * @return SAPM_SUCCESS on success, SAPM_ERR_INVALID_PRIORITY on failure.
 */
SAPM_Status priority_from_string(const char *str, Priority *out_priority);

/**
 * Converts an AssignmentStatus enum to string ("PENDING" or "COMPLETED").
 */
const char *assignment_status_to_string(AssignmentStatus status);

/**
 * Parses an AssignmentStatus from string ("PENDING", "COMPLETED", case-insensitive).
 */
SAPM_Status assignment_status_from_string(const char *str, AssignmentStatus *out_status);

/**
 * Initializes and validates an Assignment structure.
 * Trims leading and trailing whitespace from title and subject.
 *
 * @param id             Assignment ID (must be > 0).
 * @param title          Assignment title (non-empty string, <= ASSIGNMENT_TITLE_MAX - 1).
 * @param subject        Subject name (non-empty string, <= ASSIGNMENT_SUBJECT_MAX - 1).
 * @param deadline       Valid calendar date pointer.
 * @param priority       Valid priority level.
 * @param out_assignment Output pointer to store the initialized assignment.
 * @return SAPM_SUCCESS on success, or appropriate SAPM_ERR_* code on validation failure.
 */
SAPM_Status assignment_create(int id,
                             const char *title,
                             const char *subject,
                             const Date *deadline,
                             Priority priority,
                             Assignment *out_assignment);

/**
 * Single Authoritative Comparator for determining priority between two assignments.
 *
 * Evaluates priority strictly based on:
 * 1. Priority level: Higher priority level wins (URGENT > HIGH > MEDIUM > LOW).
 * 2. Deadline date: If priority levels match, earlier deadline wins.
 * 3. Tie-breaking: If priority and deadline are identical, the assignment
 *    inserted earlier (lower sequence_num) wins.
 *
 * @param a Pointer to first assignment.
 * @param b Pointer to second assignment.
 * @return > 0 if `a` has strictly higher priority than `b`,
 *         < 0 if `b` has strictly higher priority than `a`,
 *           0 if `a` and `b` have identical priority, deadline, and sequence number.
 */
int assignment_compare_priority(const Assignment *a, const Assignment *b);

/**
 * Formats an assignment into a human-readable text representation.
 * Keep console I/O decoupled by writing into a caller-supplied buffer.
 *
 * @param assignment Pointer to the assignment.
 * @param buf        Output character buffer.
 * @param buf_size   Capacity of the output buffer.
 * @return SAPM_SUCCESS on success, SAPM_ERR_BUFFER_TOO_SMALL or SAPM_ERR_NULL_PARAM on failure.
 */
SAPM_Status assignment_format(const Assignment *assignment, char *buf, size_t buf_size);

/**
 * Prints an assignment to the specified stream without coupling stdout to core logic.
 *
 * @param assignment Pointer to the assignment.
 * @param stream     Output stream (e.g. stdout, stderr, or file).
 * @return SAPM_SUCCESS on success, SAPM_ERR_NULL_PARAM on failure.
 */
SAPM_Status assignment_print(const Assignment *assignment, FILE *stream);

#endif /* ASSIGNMENT_H */
