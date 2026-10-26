#include "history.h"
#include <string.h>

SAPM_Status history_init(AssignmentHistory *hist) {
    if (hist == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }
    memset(hist->items, 0, sizeof(hist->items));
    hist->size = 0;
    return SAPM_SUCCESS;
}

SAPM_Status history_clear(AssignmentHistory *hist) {
    if (hist == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }
    memset(hist->items, 0, sizeof(hist->items));
    hist->size = 0;
    return SAPM_SUCCESS;
}

size_t history_size(const AssignmentHistory *hist) {
    return (hist != NULL) ? hist->size : 0;
}

bool history_is_empty(const AssignmentHistory *hist) {
    return (hist == NULL || hist->size == 0);
}

bool history_is_full(const AssignmentHistory *hist) {
    return (hist != NULL && hist->size >= HISTORY_CAPACITY);
}

SAPM_Status history_add(AssignmentHistory *hist, const Assignment *assignment) {
    if (hist == NULL || assignment == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }
    if (hist->size >= HISTORY_CAPACITY) {
        return SAPM_ERR_HISTORY_FULL;
    }
    if (assignment->id <= 0) {
        return SAPM_ERR_INVALID_ID;
    }

    /* Reject duplicate ID in history */
    for (size_t i = 0; i < hist->size; i++) {
        if (hist->items[i].id == assignment->id) {
            return SAPM_ERR_DUPLICATE_ID;
        }
    }

    hist->items[hist->size] = *assignment;
    hist->items[hist->size].status = ASSIGNMENT_STATUS_COMPLETED;
    hist->size++;

    return SAPM_SUCCESS;
}

SAPM_Status history_get_by_id(const AssignmentHistory *hist, int id, Assignment *out_assignment) {
    if (hist == NULL || out_assignment == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }
    if (id <= 0) {
        return SAPM_ERR_INVALID_ID;
    }

    for (size_t i = 0; i < hist->size; i++) {
        if (hist->items[i].id == id) {
            *out_assignment = hist->items[i];
            return SAPM_SUCCESS;
        }
    }

    return SAPM_ERR_NOT_FOUND;
}

SAPM_Status history_get_all(const AssignmentHistory *hist,
                            Assignment *dest,
                            size_t dest_capacity,
                            size_t *out_count) {
    if (hist == NULL || dest == NULL || out_count == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }
    if (dest_capacity < hist->size) {
        return SAPM_ERR_BUFFER_TOO_SMALL;
    }

    for (size_t i = 0; i < hist->size; i++) {
        dest[i] = hist->items[i];
    }
    *out_count = hist->size;

    return SAPM_SUCCESS;
}

SAPM_Status history_display(const AssignmentHistory *hist, FILE *stream) {
    if (hist == NULL || stream == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }

    fprintf(stream, "--- Completed Assignments History (Count: %zu / Capacity: %d) ---\n",
            hist->size, HISTORY_CAPACITY);

    if (hist->size == 0) {
        fprintf(stream, "  [No completed assignments]\n");
        return SAPM_SUCCESS;
    }

    for (size_t i = 0; i < hist->size; i++) {
        fprintf(stream, "  [%2zu] ", i + 1);
        assignment_print(&hist->items[i], stream);
    }

    return SAPM_SUCCESS;
}
