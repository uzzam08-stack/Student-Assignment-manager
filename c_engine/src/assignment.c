#include "assignment.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

const char *sapm_status_to_string(SAPM_Status status) {
    switch (status) {
        case SAPM_SUCCESS:
            return "Success";
        case SAPM_SUCCESS_FIRST_RUN:
            return "Success: First run detected, created fresh data store";
        case SAPM_ERR_NULL_PARAM:
            return "Error: NULL pointer parameter provided";
        case SAPM_ERR_INVALID_ID:
            return "Error: Invalid assignment ID (must be > 0)";
        case SAPM_ERR_INVALID_TITLE:
            return "Error: Invalid title (must be non-empty and fit buffer)";
        case SAPM_ERR_INVALID_SUBJECT:
            return "Error: Invalid subject (must be non-empty and fit buffer)";
        case SAPM_ERR_INVALID_DATE:
            return "Error: Invalid calendar date (check month, day, leap year)";
        case SAPM_ERR_INVALID_PRIORITY:
            return "Error: Invalid priority level (must be LOW, MEDIUM, HIGH, or URGENT)";
        case SAPM_ERR_DUPLICATE_ID:
            return "Error: Assignment ID already exists in queue";
        case SAPM_ERR_NOT_FOUND:
            return "Error: Assignment not found";
        case SAPM_ERR_QUEUE_FULL:
            return "Error: Priority queue capacity exceeded (maximum 100)";
        case SAPM_ERR_QUEUE_EMPTY:
            return "Error: Priority queue is empty";
        case SAPM_ERR_BUFFER_TOO_SMALL:
            return "Error: Destination buffer too small";
        case SAPM_ERR_INVALID_FORMAT:
            return "Error: Invalid format string";
        case SAPM_ERR_FILE_NOT_FOUND:
            return "Error: File not found";
        case SAPM_ERR_FILE_OPEN:
            return "Error: Unable to open file";
        case SAPM_ERR_FILE_READ:
            return "Error: Failed to read from file";
        case SAPM_ERR_FILE_WRITE:
            return "Error: Failed to write to file";
        case SAPM_ERR_FILE_CLOSE:
            return "Error: Failed to close file safely";
        case SAPM_ERR_CORRUPT_RECORD:
            return "Error: Corrupted or malformed record encountered";
        case SAPM_ERR_HISTORY_FULL:
            return "Error: Completion history capacity exceeded";
        case SAPM_ERR_INVALID_STATUS:
            return "Error: Invalid assignment status (must be PENDING or COMPLETED)";
        default:
            return "Error: Unknown status code";
    }
}

bool priority_is_valid(Priority priority) {
    return (priority >= PRIORITY_MIN && priority <= PRIORITY_MAX);
}

const char *priority_to_string(Priority priority) {
    switch (priority) {
        case PRIORITY_LOW:
            return "LOW";
        case PRIORITY_MEDIUM:
            return "MEDIUM";
        case PRIORITY_HIGH:
            return "HIGH";
        case PRIORITY_URGENT:
            return "URGENT";
        case PRIORITY_INVALID:
        default:
            return "UNKNOWN";
    }
}

SAPM_Status priority_from_string(const char *str, Priority *out_priority) {
    if (str == NULL || out_priority == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }

    while (isspace((unsigned char)*str)) {
        str++;
    }

    char buf[32];
    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        len--;
    }
    if (len == 0 || len >= sizeof(buf)) {
        return SAPM_ERR_INVALID_PRIORITY;
    }
    memcpy(buf, str, len);
    buf[len] = '\0';

    if (strcasecmp(buf, "LOW") == 0 || strcmp(buf, "1") == 0) {
        *out_priority = PRIORITY_LOW;
        return SAPM_SUCCESS;
    }
    if (strcasecmp(buf, "MEDIUM") == 0 || strcmp(buf, "2") == 0) {
        *out_priority = PRIORITY_MEDIUM;
        return SAPM_SUCCESS;
    }
    if (strcasecmp(buf, "HIGH") == 0 || strcmp(buf, "3") == 0) {
        *out_priority = PRIORITY_HIGH;
        return SAPM_SUCCESS;
    }
    if (strcasecmp(buf, "URGENT") == 0 || strcmp(buf, "4") == 0) {
        *out_priority = PRIORITY_URGENT;
        return SAPM_SUCCESS;
    }

    return SAPM_ERR_INVALID_PRIORITY;
}

const char *assignment_status_to_string(AssignmentStatus status) {
    switch (status) {
        case ASSIGNMENT_STATUS_PENDING:
            return "PENDING";
        case ASSIGNMENT_STATUS_COMPLETED:
            return "COMPLETED";
        default:
            return "UNKNOWN";
    }
}

SAPM_Status assignment_status_from_string(const char *str, AssignmentStatus *out_status) {
    if (str == NULL || out_status == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }

    while (isspace((unsigned char)*str)) {
        str++;
    }

    char buf[32];
    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        len--;
    }
    if (len == 0 || len >= sizeof(buf)) {
        return SAPM_ERR_INVALID_STATUS;
    }
    memcpy(buf, str, len);
    buf[len] = '\0';

    if (strcasecmp(buf, "PENDING") == 0 || strcmp(buf, "0") == 0) {
        *out_status = ASSIGNMENT_STATUS_PENDING;
        return SAPM_SUCCESS;
    }
    if (strcasecmp(buf, "COMPLETED") == 0 || strcmp(buf, "1") == 0) {
        *out_status = ASSIGNMENT_STATUS_COMPLETED;
        return SAPM_SUCCESS;
    }

    return SAPM_ERR_INVALID_STATUS;
}

static bool trim_and_copy(const char *src, char *dst, size_t dst_size) {
    if (src == NULL || dst == NULL || dst_size == 0) {
        return false;
    }

    while (isspace((unsigned char)*src)) {
        src++;
    }

    if (*src == '\0') {
        return false;
    }

    size_t len = strlen(src);
    while (len > 0 && isspace((unsigned char)src[len - 1])) {
        len--;
    }

    if (len == 0 || len >= dst_size) {
        return false;
    }

    memcpy(dst, src, len);
    dst[len] = '\0';
    return true;
}

SAPM_Status assignment_create(int id,
                             const char *title,
                             const char *subject,
                             const Date *deadline,
                             Priority priority,
                             Assignment *out_assignment) {
    if (out_assignment == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }
    if (id <= 0) {
        return SAPM_ERR_INVALID_ID;
    }
    if (title == NULL) {
        return SAPM_ERR_INVALID_TITLE;
    }
    if (subject == NULL) {
        return SAPM_ERR_INVALID_SUBJECT;
    }
    if (deadline == NULL || !date_is_valid(deadline)) {
        return SAPM_ERR_INVALID_DATE;
    }
    if (!priority_is_valid(priority)) {
        return SAPM_ERR_INVALID_PRIORITY;
    }

    char clean_title[ASSIGNMENT_TITLE_MAX];
    if (!trim_and_copy(title, clean_title, sizeof(clean_title))) {
        return SAPM_ERR_INVALID_TITLE;
    }

    char clean_subject[ASSIGNMENT_SUBJECT_MAX];
    if (!trim_and_copy(subject, clean_subject, sizeof(clean_subject))) {
        return SAPM_ERR_INVALID_SUBJECT;
    }

    out_assignment->id = id;
    memcpy(out_assignment->title, clean_title, sizeof(out_assignment->title));
    memcpy(out_assignment->subject, clean_subject, sizeof(out_assignment->subject));
    out_assignment->deadline = *deadline;
    out_assignment->priority = priority;
    out_assignment->sequence_num = 0;
    out_assignment->status = ASSIGNMENT_STATUS_PENDING;

    return SAPM_SUCCESS;
}

int assignment_compare_priority(const Assignment *a, const Assignment *b) {
    if (a == b) {
        return 0;
    }
    if (a == NULL && b == NULL) {
        return 0;
    }
    if (a == NULL) {
        return -1;
    }
    if (b == NULL) {
        return 1;
    }

    /* 1. Priority level: higher numeric value has higher priority */
    if (a->priority != b->priority) {
        return (a->priority > b->priority) ? 1 : -1;
    }

    /* 2. Deadline date: earlier deadline has higher urgency */
    int date_cmp = date_compare(&a->deadline, &b->deadline);
    if (date_cmp != 0) {
        return (date_cmp < 0) ? 1 : -1;
    }

    /* 3. Tie-breaker: earlier insertion sequence wins */
    if (a->sequence_num != b->sequence_num) {
        return (a->sequence_num < b->sequence_num) ? 1 : -1;
    }

    return 0;
}

SAPM_Status assignment_format(const Assignment *assignment, char *buf, size_t buf_size) {
    if (assignment == NULL || buf == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }
    if (buf_size == 0) {
        return SAPM_ERR_BUFFER_TOO_SMALL;
    }

    char date_buf[16];
    if (date_format(&assignment->deadline, date_buf, sizeof(date_buf)) < 0) {
        return SAPM_ERR_INVALID_DATE;
    }

    const char *status_str = (assignment->status == ASSIGNMENT_STATUS_COMPLETED) ? "COMPLETED" : "PENDING";

    int written = snprintf(buf, buf_size,
                          "[ID: %d] \"%s\" | Subject: %s | Priority: %s | Deadline: %s | Status: %s",
                          assignment->id,
                          assignment->title,
                          assignment->subject,
                          priority_to_string(assignment->priority),
                          date_buf,
                          status_str);

    if (written < 0 || (size_t)written >= buf_size) {
        return SAPM_ERR_BUFFER_TOO_SMALL;
    }

    return SAPM_SUCCESS;
}

SAPM_Status assignment_print(const Assignment *assignment, FILE *stream) {
    if (assignment == NULL || stream == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }

    char buf[512];
    SAPM_Status status = assignment_format(assignment, buf, sizeof(buf));
    if (status != SAPM_SUCCESS) {
        return status;
    }

    fprintf(stream, "%s\n", buf);
    return SAPM_SUCCESS;
}
