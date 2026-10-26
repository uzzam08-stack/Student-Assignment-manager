#ifndef HISTORY_H
#define HISTORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "assignment.h"

#define HISTORY_CAPACITY 500

/**
 * Completion history storing completed assignment records.
 */
typedef struct {
    Assignment items[HISTORY_CAPACITY];
    size_t size;
} AssignmentHistory;

/**
 * Initializes the completion history structure.
 */
SAPM_Status history_init(AssignmentHistory *hist);

/**
 * Clears all records in completion history.
 */
SAPM_Status history_clear(AssignmentHistory *hist);

/**
 * Returns count of completed assignments.
 */
size_t history_size(const AssignmentHistory *hist);

/**
 * Checks if completion history is empty.
 */
bool history_is_empty(const AssignmentHistory *hist);

/**
 * Checks if completion history has reached maximum capacity.
 */
bool history_is_full(const AssignmentHistory *hist);

/**
 * Adds a completed assignment to history.
 * Ensures status is ASSIGNMENT_STATUS_COMPLETED and rejects duplicate IDs.
 *
 * @param hist       Pointer to AssignmentHistory.
 * @param assignment Pointer to completed assignment to record.
 * @return SAPM_SUCCESS on success,
 *         SAPM_ERR_HISTORY_FULL if capacity reached,
 *         SAPM_ERR_DUPLICATE_ID if ID already in history,
 *         SAPM_ERR_NULL_PARAM on NULL pointers.
 */
SAPM_Status history_add(AssignmentHistory *hist, const Assignment *assignment);

/**
 * Retrieves a completed assignment by its unique ID.
 */
SAPM_Status history_get_by_id(const AssignmentHistory *hist, int id, Assignment *out_assignment);

/**
 * Copies all completed assignments to a caller-supplied buffer.
 */
SAPM_Status history_get_all(const AssignmentHistory *hist,
                            Assignment *dest,
                            size_t dest_capacity,
                            size_t *out_count);

/**
 * Displays completion history to a specified stream.
 */
SAPM_Status history_display(const AssignmentHistory *hist, FILE *stream);

#endif /* HISTORY_H */
