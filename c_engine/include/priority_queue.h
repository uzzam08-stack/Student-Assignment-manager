#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "assignment.h"

#define PQ_CAPACITY 100

/**
 * Priority Queue implemented via an unsorted array of structures.
 * Capacity is strictly bounded to PQ_CAPACITY (100).
 * Insertion is O(1).
 * Highest-priority extraction/lookup is O(N) using the single authoritative comparator.
 */
typedef struct {
    Assignment items[PQ_CAPACITY];
    size_t size;
    uint64_t next_sequence;
} PriorityQueue;

/**
 * Initializes a priority queue to empty state.
 *
 * @param pq Pointer to PriorityQueue.
 * @return SAPM_SUCCESS on success, SAPM_ERR_NULL_PARAM if pq is NULL.
 */
SAPM_Status pq_init(PriorityQueue *pq);

/**
 * Clears all items in the priority queue and resets size to 0.
 *
 * @param pq Pointer to PriorityQueue.
 * @return SAPM_SUCCESS on success, SAPM_ERR_NULL_PARAM if pq is NULL.
 */
SAPM_Status pq_clear(PriorityQueue *pq);

/**
 * Returns current count of pending assignments.
 */
size_t pq_size(const PriorityQueue *pq);

/**
 * Checks if the priority queue is empty.
 */
bool pq_is_empty(const PriorityQueue *pq);

/**
 * Checks if the priority queue has reached maximum capacity (100).
 */
bool pq_is_full(const PriorityQueue *pq);

/**
 * Inserts a validated Assignment into the unsorted priority queue.
 * Rejects capacity overflow and duplicate IDs.
 * Assigns an arrival sequence number preserving insertion order for ties.
 *
 * @param pq         Pointer to PriorityQueue.
 * @param assignment Pointer to assignment to insert.
 * @return SAPM_SUCCESS on success,
 *         SAPM_ERR_QUEUE_FULL if size >= PQ_CAPACITY,
 *         SAPM_ERR_DUPLICATE_ID if an assignment with the same ID already exists,
 *         SAPM_ERR_NULL_PARAM or validation error codes otherwise.
 */
SAPM_Status pq_insert(PriorityQueue *pq, const Assignment *assignment);

/**
 * Convenience helper to create, validate, and insert an assignment in one step.
 *
 * @param pq       Pointer to PriorityQueue.
 * @param id       Unique positive ID.
 * @param title    Assignment title.
 * @param subject  Subject or course name.
 * @param deadline Due date (validated calendar date).
 * @param priority Priority level.
 * @return SAPM_SUCCESS on success, or appropriate error code.
 */
SAPM_Status pq_add(PriorityQueue *pq,
                   int id,
                   const char *title,
                   const char *subject,
                   const Date *deadline,
                   Priority priority);

/**
 * Finds and inspects the highest-priority assignment without removing it.
 * Uses the authoritative comparator to locate the optimal item in O(N) time.
 *
 * @param pq             Pointer to PriorityQueue.
 * @param out_assignment Output pointer to receive a copy of the highest-priority assignment.
 * @return SAPM_SUCCESS on success,
 *         SAPM_ERR_QUEUE_EMPTY if queue is empty,
 *         SAPM_ERR_NULL_PARAM if any argument is NULL.
 */
SAPM_Status pq_peek_highest(const PriorityQueue *pq, Assignment *out_assignment);

/**
 * Dequeues / processes the highest-priority assignment.
 * Removes the item from the queue, marks its status as COMPLETED, and shifts
 * subsequent elements to maintain array continuity.
 *
 * @param pq             Pointer to PriorityQueue.
 * @param out_assignment Optional output pointer to receive the processed assignment (can be NULL).
 * @return SAPM_SUCCESS on success,
 *         SAPM_ERR_QUEUE_EMPTY if queue is empty,
 *         SAPM_ERR_NULL_PARAM if pq is NULL.
 */
SAPM_Status pq_process_highest(PriorityQueue *pq, Assignment *out_assignment);

/**
 * Processes a specific assignment identified by its unique ID.
 * Removes the assignment from the queue and marks its status as COMPLETED.
 *
 * @param pq             Pointer to PriorityQueue.
 * @param id             Assignment ID.
 * @param out_assignment Optional output pointer to receive the processed assignment (can be NULL).
 * @return SAPM_SUCCESS on success,
 *         SAPM_ERR_NOT_FOUND if ID not found,
 *         SAPM_ERR_INVALID_ID if id <= 0,
 *         SAPM_ERR_QUEUE_EMPTY if queue is empty,
 *         SAPM_ERR_NULL_PARAM if pq is NULL.
 */
SAPM_Status pq_process_by_id(PriorityQueue *pq, int id, Assignment *out_assignment);

/**
 * Deletes a pending assignment by ID without processing it.
 *
 * @param pq          Pointer to PriorityQueue.
 * @param id          Assignment ID to delete.
 * @param out_deleted Optional output pointer to receive the deleted assignment (can be NULL).
 * @return SAPM_SUCCESS on success,
 *         SAPM_ERR_NOT_FOUND if ID is not present,
 *         SAPM_ERR_INVALID_ID if id <= 0,
 *         SAPM_ERR_QUEUE_EMPTY if queue is empty,
 *         SAPM_ERR_NULL_PARAM if pq is NULL.
 */
SAPM_Status pq_delete_by_id(PriorityQueue *pq, int id, Assignment *out_deleted);

/**
 * Searches for an assignment by its unique ID.
 *
 * @param pq             Pointer to PriorityQueue.
 * @param id             Assignment ID to locate.
 * @param out_assignment Output pointer to store the found assignment.
 * @return SAPM_SUCCESS on success,
 *         SAPM_ERR_NOT_FOUND if ID does not exist,
 *         SAPM_ERR_INVALID_ID if id <= 0,
 *         SAPM_ERR_NULL_PARAM if pointers are NULL.
 */
SAPM_Status pq_search_by_id(const PriorityQueue *pq, int id, Assignment *out_assignment);

/**
 * Searches for the first assignment matching a given title (case-insensitive).
 *
 * @param pq             Pointer to PriorityQueue.
 * @param title          Title string to search for.
 * @param out_assignment Output pointer to store the matching assignment.
 * @return SAPM_SUCCESS on success,
 *         SAPM_ERR_NOT_FOUND if no assignment matches,
 *         SAPM_ERR_INVALID_TITLE if title is NULL or empty,
 *         SAPM_ERR_NULL_PARAM if parameters are NULL.
 */
SAPM_Status pq_search_by_title(const PriorityQueue *pq, const char *title, Assignment *out_assignment);

/**
 * Searches for all assignments matching a given title (case-insensitive).
 *
 * @param pq            Pointer to PriorityQueue.
 * @param title         Title string to search for.
 * @param dest          Output array to store matching assignments.
 * @param dest_capacity Maximum number of items dest can hold.
 * @param out_count     Pointer to store number of matches found.
 * @return SAPM_SUCCESS on success, or appropriate error code.
 */
SAPM_Status pq_search_by_title_all(const PriorityQueue *pq,
                                   const char *title,
                                   Assignment *dest,
                                   size_t dest_capacity,
                                   size_t *out_count);

/**
 * Updates the priority level of an existing assignment.
 *
 * @param pq           Pointer to PriorityQueue.
 * @param id           Assignment ID to update.
 * @param new_priority New priority level (validated).
 * @return SAPM_SUCCESS on success,
 *         SAPM_ERR_INVALID_PRIORITY if new_priority is invalid,
 *         SAPM_ERR_NOT_FOUND if ID does not exist,
 *         SAPM_ERR_INVALID_ID if id <= 0,
 *         SAPM_ERR_NULL_PARAM if pq is NULL.
 */
SAPM_Status pq_update_priority(PriorityQueue *pq, int id, Priority new_priority);

/**
 * Updates the deadline date of an existing assignment.
 *
 * @param pq           Pointer to PriorityQueue.
 * @param id           Assignment ID to update.
 * @param new_deadline Pointer to validated new calendar date.
 * @return SAPM_SUCCESS on success,
 *         SAPM_ERR_INVALID_DATE if new_deadline is invalid or NULL,
 *         SAPM_ERR_NOT_FOUND if ID does not exist,
 *         SAPM_ERR_INVALID_ID if id <= 0,
 *         SAPM_ERR_NULL_PARAM if pq is NULL.
 */
SAPM_Status pq_update_deadline(PriorityQueue *pq, int id, const Date *new_deadline);

/**
 * Updates both priority and deadline atomically.
 *
 * @param pq           Pointer to PriorityQueue.
 * @param id           Assignment ID to update.
 * @param new_priority New priority level.
 * @param new_deadline New deadline date.
 * @return SAPM_SUCCESS on success, or error code on failure (no mutation on error).
 */
SAPM_Status pq_update_priority_and_deadline(PriorityQueue *pq,
                                            int id,
                                            Priority new_priority,
                                            const Date *new_deadline);

/**
 * Copies all pending assignments in their storage order into a caller-supplied array.
 *
 * @param pq            Pointer to PriorityQueue.
 * @param dest          Caller buffer of Assignment structures.
 * @param dest_capacity Size of caller buffer.
 * @param out_count     Pointer to receive actual number of items copied.
 * @return SAPM_SUCCESS on success,
 *         SAPM_ERR_BUFFER_TOO_SMALL if dest_capacity < pq->size,
 *         SAPM_ERR_NULL_PARAM if pointers are NULL.
 */
SAPM_Status pq_get_all(const PriorityQueue *pq,
                       Assignment *dest,
                       size_t dest_capacity,
                       size_t *out_count);

/**
 * Returns a priority-sorted snapshot of all pending assignments without altering
 * the underlying unsorted queue.
 *
 * @param pq            Pointer to PriorityQueue.
 * @param dest          Caller buffer of Assignment structures.
 * @param dest_capacity Size of caller buffer.
 * @param out_count     Pointer to receive actual count.
 * @return SAPM_SUCCESS on success, or appropriate error code.
 */
SAPM_Status pq_get_sorted_view(const PriorityQueue *pq,
                               Assignment *dest,
                               size_t dest_capacity,
                               size_t *out_count);

/**
 * Displays all pending assignments to a specified stream (e.g. stdout).
 * Completely decouples I/O from internal state.
 *
 * @param pq     Pointer to PriorityQueue.
 * @param stream Output stream.
 * @return SAPM_SUCCESS on success, SAPM_ERR_NULL_PARAM if pq or stream is NULL.
 */
SAPM_Status pq_display(const PriorityQueue *pq, FILE *stream);

#endif /* PRIORITY_QUEUE_H */
