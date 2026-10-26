#include "priority_queue.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

SAPM_Status pq_init(PriorityQueue *pq) {
    if (pq == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }
    memset(pq->items, 0, sizeof(pq->items));
    pq->size = 0;
    pq->next_sequence = 1;
    return SAPM_SUCCESS;
}

SAPM_Status pq_clear(PriorityQueue *pq) {
    if (pq == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }
    memset(pq->items, 0, sizeof(pq->items));
    pq->size = 0;
    return SAPM_SUCCESS;
}

size_t pq_size(const PriorityQueue *pq) {
    return (pq != NULL) ? pq->size : 0;
}

bool pq_is_empty(const PriorityQueue *pq) {
    return (pq == NULL || pq->size == 0);
}

bool pq_is_full(const PriorityQueue *pq) {
    return (pq != NULL && pq->size >= PQ_CAPACITY);
}

static int pq_find_index_by_id(const PriorityQueue *pq, int id, size_t *out_index) {
    if (pq == NULL || out_index == NULL) {
        return -1;
    }
    for (size_t i = 0; i < pq->size; i++) {
        if (pq->items[i].id == id) {
            *out_index = i;
            return 0;
        }
    }
    return -1;
}

static void pq_remove_at_index(PriorityQueue *pq, size_t index) {
    for (size_t i = index; i + 1 < pq->size; i++) {
        pq->items[i] = pq->items[i + 1];
    }
    pq->size--;
    memset(&pq->items[pq->size], 0, sizeof(Assignment));
}

/**
 * Single authoritative search for the highest-priority assignment in O(N).
 * Strictly utilizes assignment_compare_priority to guarantee consistency.
 */
static SAPM_Status pq_find_highest_index(const PriorityQueue *pq, size_t *out_index) {
    if (pq == NULL || out_index == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }
    if (pq->size == 0) {
        return SAPM_ERR_QUEUE_EMPTY;
    }

    size_t best_index = 0;
    for (size_t i = 1; i < pq->size; i++) {
        if (assignment_compare_priority(&pq->items[i], &pq->items[best_index]) > 0) {
            best_index = i;
        }
    }

    *out_index = best_index;
    return SAPM_SUCCESS;
}

SAPM_Status pq_insert(PriorityQueue *pq, const Assignment *assignment) {
    if (pq == NULL || assignment == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }
    if (pq->size >= PQ_CAPACITY) {
        return SAPM_ERR_QUEUE_FULL;
    }
    if (assignment->id <= 0) {
        return SAPM_ERR_INVALID_ID;
    }
    if (!priority_is_valid(assignment->priority)) {
        return SAPM_ERR_INVALID_PRIORITY;
    }
    if (!date_is_valid(&assignment->deadline)) {
        return SAPM_ERR_INVALID_DATE;
    }
    if (assignment->title[0] == '\0') {
        return SAPM_ERR_INVALID_TITLE;
    }
    if (assignment->subject[0] == '\0') {
        return SAPM_ERR_INVALID_SUBJECT;
    }

    /* Check for duplicate ID */
    size_t existing_index = 0;
    if (pq_find_index_by_id(pq, assignment->id, &existing_index) == 0) {
        return SAPM_ERR_DUPLICATE_ID;
    }

    /* Append to unsorted array */
    pq->items[pq->size] = *assignment;
    pq->items[pq->size].sequence_num = pq->next_sequence++;
    pq->items[pq->size].status = ASSIGNMENT_STATUS_PENDING;
    pq->size++;

    return SAPM_SUCCESS;
}

SAPM_Status pq_add(PriorityQueue *pq,
                   int id,
                   const char *title,
                   const char *subject,
                   const Date *deadline,
                   Priority priority) {
    if (pq == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }

    Assignment item;
    SAPM_Status status = assignment_create(id, title, subject, deadline, priority, &item);
    if (status != SAPM_SUCCESS) {
        return status;
    }

    return pq_insert(pq, &item);
}

SAPM_Status pq_peek_highest(const PriorityQueue *pq, Assignment *out_assignment) {
    if (pq == NULL || out_assignment == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }

    size_t best_index = 0;
    SAPM_Status status = pq_find_highest_index(pq, &best_index);
    if (status != SAPM_SUCCESS) {
        return status;
    }

    *out_assignment = pq->items[best_index];
    return SAPM_SUCCESS;
}

SAPM_Status pq_process_highest(PriorityQueue *pq, Assignment *out_assignment) {
    if (pq == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }

    size_t best_index = 0;
    SAPM_Status status = pq_find_highest_index(pq, &best_index);
    if (status != SAPM_SUCCESS) {
        return status;
    }

    if (out_assignment != NULL) {
        *out_assignment = pq->items[best_index];
        out_assignment->status = ASSIGNMENT_STATUS_COMPLETED;
    }

    pq_remove_at_index(pq, best_index);
    return SAPM_SUCCESS;
}

SAPM_Status pq_process_by_id(PriorityQueue *pq, int id, Assignment *out_assignment) {
    if (pq == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }
    if (id <= 0) {
        return SAPM_ERR_INVALID_ID;
    }
    if (pq->size == 0) {
        return SAPM_ERR_QUEUE_EMPTY;
    }

    size_t index = 0;
    if (pq_find_index_by_id(pq, id, &index) != 0) {
        return SAPM_ERR_NOT_FOUND;
    }

    if (out_assignment != NULL) {
        *out_assignment = pq->items[index];
        out_assignment->status = ASSIGNMENT_STATUS_COMPLETED;
    }

    pq_remove_at_index(pq, index);
    return SAPM_SUCCESS;
}

SAPM_Status pq_delete_by_id(PriorityQueue *pq, int id, Assignment *out_deleted) {
    if (pq == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }
    if (id <= 0) {
        return SAPM_ERR_INVALID_ID;
    }
    if (pq->size == 0) {
        return SAPM_ERR_QUEUE_EMPTY;
    }

    size_t index = 0;
    if (pq_find_index_by_id(pq, id, &index) != 0) {
        return SAPM_ERR_NOT_FOUND;
    }

    if (out_deleted != NULL) {
        *out_deleted = pq->items[index];
    }

    pq_remove_at_index(pq, index);
    return SAPM_SUCCESS;
}

SAPM_Status pq_search_by_id(const PriorityQueue *pq, int id, Assignment *out_assignment) {
    if (pq == NULL || out_assignment == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }
    if (id <= 0) {
        return SAPM_ERR_INVALID_ID;
    }

    size_t index = 0;
    if (pq_find_index_by_id(pq, id, &index) != 0) {
        return SAPM_ERR_NOT_FOUND;
    }

    *out_assignment = pq->items[index];
    return SAPM_SUCCESS;
}

static bool title_matches(const char *item_title, const char *query) {
    if (item_title == NULL || query == NULL) {
        return false;
    }
    if (strcasecmp(item_title, query) == 0) {
        return true;
    }
    size_t qlen = strlen(query);
    size_t tlen = strlen(item_title);
    if (qlen == 0 || qlen > tlen) {
        return false;
    }
    for (size_t i = 0; i <= tlen - qlen; i++) {
        if (strncasecmp(&item_title[i], query, qlen) == 0) {
            return true;
        }
    }
    return false;
}

SAPM_Status pq_search_by_title(const PriorityQueue *pq, const char *title, Assignment *out_assignment) {
    if (pq == NULL || title == NULL || out_assignment == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }

    /* Trim leading spaces of query */
    while (isspace((unsigned char)*title)) {
        title++;
    }
    if (*title == '\0') {
        return SAPM_ERR_INVALID_TITLE;
    }

    for (size_t i = 0; i < pq->size; i++) {
        if (title_matches(pq->items[i].title, title)) {
            *out_assignment = pq->items[i];
            return SAPM_SUCCESS;
        }
    }

    return SAPM_ERR_NOT_FOUND;
}

SAPM_Status pq_search_by_title_all(const PriorityQueue *pq,
                                   const char *title,
                                   Assignment *dest,
                                   size_t dest_capacity,
                                   size_t *out_count) {
    if (pq == NULL || title == NULL || dest == NULL || out_count == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }

    while (isspace((unsigned char)*title)) {
        title++;
    }
    if (*title == '\0') {
        return SAPM_ERR_INVALID_TITLE;
    }

    size_t count = 0;
    for (size_t i = 0; i < pq->size; i++) {
        if (title_matches(pq->items[i].title, title)) {
            if (count < dest_capacity) {
                dest[count] = pq->items[i];
            }
            count++;
        }
    }

    *out_count = count;
    if (count > dest_capacity) {
        return SAPM_ERR_BUFFER_TOO_SMALL;
    }

    return (count > 0) ? SAPM_SUCCESS : SAPM_ERR_NOT_FOUND;
}

SAPM_Status pq_update_priority(PriorityQueue *pq, int id, Priority new_priority) {
    if (pq == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }
    if (id <= 0) {
        return SAPM_ERR_INVALID_ID;
    }
    if (!priority_is_valid(new_priority)) {
        return SAPM_ERR_INVALID_PRIORITY;
    }

    size_t index = 0;
    if (pq_find_index_by_id(pq, id, &index) != 0) {
        return SAPM_ERR_NOT_FOUND;
    }

    pq->items[index].priority = new_priority;
    return SAPM_SUCCESS;
}

SAPM_Status pq_update_deadline(PriorityQueue *pq, int id, const Date *new_deadline) {
    if (pq == NULL || new_deadline == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }
    if (id <= 0) {
        return SAPM_ERR_INVALID_ID;
    }
    if (!date_is_valid(new_deadline)) {
        return SAPM_ERR_INVALID_DATE;
    }

    size_t index = 0;
    if (pq_find_index_by_id(pq, id, &index) != 0) {
        return SAPM_ERR_NOT_FOUND;
    }

    pq->items[index].deadline = *new_deadline;
    return SAPM_SUCCESS;
}

SAPM_Status pq_update_priority_and_deadline(PriorityQueue *pq,
                                            int id,
                                            Priority new_priority,
                                            const Date *new_deadline) {
    if (pq == NULL || new_deadline == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }
    if (id <= 0) {
        return SAPM_ERR_INVALID_ID;
    }
    if (!priority_is_valid(new_priority)) {
        return SAPM_ERR_INVALID_PRIORITY;
    }
    if (!date_is_valid(new_deadline)) {
        return SAPM_ERR_INVALID_DATE;
    }

    size_t index = 0;
    if (pq_find_index_by_id(pq, id, &index) != 0) {
        return SAPM_ERR_NOT_FOUND;
    }

    pq->items[index].priority = new_priority;
    pq->items[index].deadline = *new_deadline;
    return SAPM_SUCCESS;
}

SAPM_Status pq_get_all(const PriorityQueue *pq,
                       Assignment *dest,
                       size_t dest_capacity,
                       size_t *out_count) {
    if (pq == NULL || dest == NULL || out_count == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }
    if (dest_capacity < pq->size) {
        return SAPM_ERR_BUFFER_TOO_SMALL;
    }

    for (size_t i = 0; i < pq->size; i++) {
        dest[i] = pq->items[i];
    }
    *out_count = pq->size;
    return SAPM_SUCCESS;
}

static int qsort_assignment_comparator(const void *p1, const void *p2) {
    const Assignment *a = (const Assignment *)p1;
    const Assignment *b = (const Assignment *)p2;
    /* Highest priority first, so negate the return value of assignment_compare_priority */
    int cmp = assignment_compare_priority(a, b);
    return -cmp;
}

SAPM_Status pq_get_sorted_view(const PriorityQueue *pq,
                               Assignment *dest,
                               size_t dest_capacity,
                               size_t *out_count) {
    if (pq == NULL || dest == NULL || out_count == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }
    if (dest_capacity < pq->size) {
        return SAPM_ERR_BUFFER_TOO_SMALL;
    }

    for (size_t i = 0; i < pq->size; i++) {
        dest[i] = pq->items[i];
    }
    *out_count = pq->size;

    if (pq->size > 1) {
        qsort(dest, pq->size, sizeof(Assignment), qsort_assignment_comparator);
    }

    return SAPM_SUCCESS;
}

SAPM_Status pq_display(const PriorityQueue *pq, FILE *stream) {
    if (pq == NULL || stream == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }

    fprintf(stream, "--- Student Assignment Priority Queue (Count: %zu / Capacity: %d) ---\n",
            pq->size, PQ_CAPACITY);

    if (pq->size == 0) {
        fprintf(stream, "  [Queue is empty]\n");
        return SAPM_SUCCESS;
    }

    for (size_t i = 0; i < pq->size; i++) {
        fprintf(stream, "  [%2zu] ", i + 1);
        assignment_print(&pq->items[i], stream);
    }

    return SAPM_SUCCESS;
}
