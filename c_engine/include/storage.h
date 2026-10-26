#ifndef STORAGE_H
#define STORAGE_H

#include <stdbool.h>
#include "assignment.h"
#include "history.h"
#include "priority_queue.h"

#define STORAGE_DEFAULT_FILE "assignments_data.csv"
#define STORAGE_HEADER "id,title,subject,deadline,priority,sequence_num,status"

/**
 * Saves both pending priority queue items and completed history records
 * into a CSV format file using an atomic replacement strategy:
 * 1. Writes to temporary file (<filepath>.tmp).
 * 2. Checks every write, flush, and close operation.
 * 3. On success, atomically renames temporary file to target file.
 * 4. On failure, removes temporary file, leaving original file completely intact.
 *
 * @param filepath Target file path.
 * @param pq       Pointer to PriorityQueue containing pending assignments.
 * @param hist     Pointer to AssignmentHistory containing completed assignments.
 * @return SAPM_SUCCESS on success, or appropriate SAPM_ERR_* code on failure.
 */
SAPM_Status storage_save(const char *filepath,
                        const PriorityQueue *pq,
                        const AssignmentHistory *hist);

/**
 * Loads pending and completed assignments from a CSV file.
 *
 * Guarantees:
 * - If the file does not exist, handles as a first-run scenario without error:
 *   initializes pq and hist to empty, sets out_first_run = true, and returns SAPM_SUCCESS_FIRST_RUN.
 * - Staging verification: parses and validates all records into temporary staging
 *   buffers before modifying active pq and hist.
 * - Never silently resets existing in-memory records on load failure!
 * - Enforces max 100 pending assignments (rejects with SAPM_ERR_QUEUE_FULL).
 * - Enforces unique IDs across all records (pending and completed).
 * - Restores monotonic sequence numbers so arrival order is preserved.
 *
 * @param filepath      Path to data file.
 * @param pq            Pointer to active PriorityQueue to populate on success.
 * @param hist          Pointer to active AssignmentHistory to populate on success.
 * @param out_first_run Optional pointer returning true if first run was detected.
 * @return SAPM_SUCCESS or SAPM_SUCCESS_FIRST_RUN on success, error code on failure.
 */
SAPM_Status storage_load(const char *filepath,
                        PriorityQueue *pq,
                        AssignmentHistory *hist,
                        bool *out_first_run);

#endif /* STORAGE_H */
