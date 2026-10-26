#include "storage.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int write_csv_field(FILE *fp, const char *str) {
    bool needs_quotes = false;
    for (const char *p = str; *p != '\0'; p++) {
        if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') {
            needs_quotes = true;
            break;
        }
    }

    if (!needs_quotes) {
        return (fputs(str, fp) == EOF) ? -1 : 0;
    }

    if (fputc('"', fp) == EOF) {
        return -1;
    }
    for (const char *p = str; *p != '\0'; p++) {
        if (*p == '"') {
            if (fputc('"', fp) == EOF || fputc('"', fp) == EOF) {
                return -1;
            }
        } else {
            if (fputc(*p, fp) == EOF) {
                return -1;
            }
        }
    }
    if (fputc('"', fp) == EOF) {
        return -1;
    }
    return 0;
}

static int write_assignment_csv_row(FILE *fp, const Assignment *a) {
    char date_str[16];
    if (date_format(&a->deadline, date_str, sizeof(date_str)) < 0) {
        return -1;
    }

    if (fprintf(fp, "%d,", a->id) < 0) {
        return -1;
    }
    if (write_csv_field(fp, a->title) < 0) {
        return -1;
    }
    if (fputc(',', fp) == EOF) {
        return -1;
    }
    if (write_csv_field(fp, a->subject) < 0) {
        return -1;
    }
    if (fputc(',', fp) == EOF) {
        return -1;
    }
    if (fprintf(fp, "%s,%s,%llu,%s\n",
                date_str,
                priority_to_string(a->priority),
                (unsigned long long)a->sequence_num,
                assignment_status_to_string(a->status)) < 0) {
        return -1;
    }

    return 0;
}

SAPM_Status storage_save(const char *filepath,
                        const PriorityQueue *pq,
                        const AssignmentHistory *hist) {
    if (filepath == NULL || pq == NULL || hist == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }

    char temp_path[1024];
    int n = snprintf(temp_path, sizeof(temp_path), "%s.tmp", filepath);
    if (n < 0 || (size_t)n >= sizeof(temp_path)) {
        return SAPM_ERR_FILE_WRITE;
    }

    FILE *fp = fopen(temp_path, "w");
    if (fp == NULL) {
        return SAPM_ERR_FILE_OPEN;
    }

    /* 1. Write standard CSV header */
    if (fprintf(fp, "%s\n", STORAGE_HEADER) < 0) {
        fclose(fp);
        remove(temp_path);
        return SAPM_ERR_FILE_WRITE;
    }

    /* 2. Write pending assignments */
    for (size_t i = 0; i < pq->size; i++) {
        if (write_assignment_csv_row(fp, &pq->items[i]) < 0) {
            fclose(fp);
            remove(temp_path);
            return SAPM_ERR_FILE_WRITE;
        }
    }

    /* 3. Write completed history assignments */
    for (size_t i = 0; i < hist->size; i++) {
        if (write_assignment_csv_row(fp, &hist->items[i]) < 0) {
            fclose(fp);
            remove(temp_path);
            return SAPM_ERR_FILE_WRITE;
        }
    }

    /* 4. Flush and check for write errors */
    if (fflush(fp) != 0 || ferror(fp) != 0) {
        fclose(fp);
        remove(temp_path);
        return SAPM_ERR_FILE_WRITE;
    }

    /* 5. Close safely */
    if (fclose(fp) != 0) {
        remove(temp_path);
        return SAPM_ERR_FILE_CLOSE;
    }

    /* 6. Atomically replace target file */
    if (rename(temp_path, filepath) != 0) {
        remove(temp_path);
        return SAPM_ERR_FILE_WRITE;
    }

    return SAPM_SUCCESS;
}

static int parse_csv_line(const char *line, char fields[7][256]) {
    int col = 0;
    const char *ptr = line;

    while (*ptr != '\0') {
        if (*ptr == '\r' || *ptr == '\n') {
            break;
        }

        if (col >= 7) {
            return -1;
        }

        size_t idx = 0;
        if (*ptr == '"') {
            ptr++;
            while (1) {
                if (*ptr == '\0') {
                    return -1; /* Unclosed quote */
                }
                if (*ptr == '"') {
                    if (*(ptr + 1) == '"') {
                        if (idx + 1 < sizeof(fields[0])) {
                            fields[col][idx++] = '"';
                        }
                        ptr += 2;
                    } else {
                        ptr++;
                        break;
                    }
                } else {
                    if (idx + 1 < sizeof(fields[0])) {
                        fields[col][idx++] = *ptr;
                    }
                    ptr++;
                }
            }
            fields[col][idx] = '\0';
            while (*ptr == ' ' || *ptr == '\t') {
                ptr++;
            }
            if (*ptr == ',') {
                ptr++;
            } else if (*ptr != '\0' && *ptr != '\r' && *ptr != '\n') {
                return -1;
            }
            col++;
        } else {
            /* Skip leading whitespace in unquoted field */
            while (isspace((unsigned char)*ptr) && *ptr != ',' && *ptr != '\r' && *ptr != '\n') {
                ptr++;
            }
            while (*ptr != '\0' && *ptr != ',' && *ptr != '\r' && *ptr != '\n') {
                if (idx + 1 < sizeof(fields[0])) {
                    fields[col][idx++] = *ptr;
                }
                ptr++;
            }
            /* Trim trailing whitespace */
            while (idx > 0 && isspace((unsigned char)fields[col][idx - 1])) {
                idx--;
            }
            fields[col][idx] = '\0';
            if (*ptr == ',') {
                ptr++;
            }
            col++;
        }
    }

    return (col == 7) ? 0 : -1;
}

static bool id_exists_in_pq_or_hist(int id, const PriorityQueue *pq, const AssignmentHistory *hist) {
    for (size_t i = 0; i < pq->size; i++) {
        if (pq->items[i].id == id) {
            return true;
        }
    }
    for (size_t i = 0; i < hist->size; i++) {
        if (hist->items[i].id == id) {
            return true;
        }
    }
    return false;
}

SAPM_Status storage_load(const char *filepath,
                        PriorityQueue *pq,
                        AssignmentHistory *hist,
                        bool *out_first_run) {
    if (filepath == NULL || pq == NULL || hist == NULL) {
        return SAPM_ERR_NULL_PARAM;
    }

    if (out_first_run != NULL) {
        *out_first_run = false;
    }

    /* Check if file exists */
    if (access(filepath, F_OK) != 0) {
        if (errno == ENOENT) {
            pq_init(pq);
            history_init(hist);
            if (out_first_run != NULL) {
                *out_first_run = true;
            }
            return SAPM_SUCCESS_FIRST_RUN;
        }
        return SAPM_ERR_FILE_OPEN;
    }

    FILE *fp = fopen(filepath, "r");
    if (fp == NULL) {
        if (errno == ENOENT) {
            pq_init(pq);
            history_init(hist);
            if (out_first_run != NULL) {
                *out_first_run = true;
            }
            return SAPM_SUCCESS_FIRST_RUN;
        }
        return SAPM_ERR_FILE_OPEN;
    }

    /* Use staging structures so active structures are never corrupted on failure */
    PriorityQueue stage_pq;
    AssignmentHistory stage_hist;
    pq_init(&stage_pq);
    history_init(&stage_hist);

    char line_buf[1024];
    /* 1. Read and validate header line */
    if (fgets(line_buf, sizeof(line_buf), fp) == NULL) {
        fclose(fp);
        return SAPM_ERR_CORRUPT_RECORD;
    }

    /* Verify header contains key field names */
    if (strstr(line_buf, "id") == NULL ||
        strstr(line_buf, "title") == NULL ||
        strstr(line_buf, "deadline") == NULL ||
        strstr(line_buf, "priority") == NULL ||
        strstr(line_buf, "status") == NULL) {
        fclose(fp);
        return SAPM_ERR_CORRUPT_RECORD;
    }

    uint64_t max_seq = 0;

    /* 2. Read records */
    while (fgets(line_buf, sizeof(line_buf), fp) != NULL) {

        /* Skip blank lines */
        const char *p = line_buf;
        while (isspace((unsigned char)*p)) p++;
        if (*p == '\0') {
            continue;
        }

        char fields[7][256];
        if (parse_csv_line(line_buf, fields) != 0) {
            fclose(fp);
            return SAPM_ERR_CORRUPT_RECORD;
        }

        /* Field 0: ID */
        char *endptr = NULL;
        long id_val = strtol(fields[0], &endptr, 10);
        if (endptr == fields[0] || *endptr != '\0' || id_val <= 0 || id_val > INT_MAX) {
            fclose(fp);
            return SAPM_ERR_CORRUPT_RECORD;
        }
        int id = (int)id_val;

        /* Enforce unique ID across both queues */
        if (id_exists_in_pq_or_hist(id, &stage_pq, &stage_hist)) {
            fclose(fp);
            return SAPM_ERR_DUPLICATE_ID;
        }

        /* Field 1: Title */
        if (fields[1][0] == '\0') {
            fclose(fp);
            return SAPM_ERR_INVALID_TITLE;
        }

        /* Field 2: Subject */
        if (fields[2][0] == '\0') {
            fclose(fp);
            return SAPM_ERR_INVALID_SUBJECT;
        }

        /* Field 3: Deadline */
        Date deadline;
        if (date_parse(fields[3], &deadline) != 0 || !date_is_valid(&deadline)) {
            fclose(fp);
            return SAPM_ERR_INVALID_DATE;
        }

        /* Field 4: Priority */
        Priority priority;
        if (priority_from_string(fields[4], &priority) != SAPM_SUCCESS) {
            fclose(fp);
            return SAPM_ERR_INVALID_PRIORITY;
        }

        /* Field 5: Sequence number */
        unsigned long long seq_val = strtoull(fields[5], &endptr, 10);
        if (endptr == fields[5] || *endptr != '\0') {
            fclose(fp);
            return SAPM_ERR_CORRUPT_RECORD;
        }

        /* Field 6: Status */
        AssignmentStatus status;
        if (assignment_status_from_string(fields[6], &status) != SAPM_SUCCESS) {
            fclose(fp);
            return SAPM_ERR_INVALID_STATUS;
        }

        /* Construct assignment record */
        Assignment item;
        SAPM_Status create_st = assignment_create(id, fields[1], fields[2], &deadline, priority, &item);
        if (create_st != SAPM_SUCCESS) {
            fclose(fp);
            return create_st;
        }
        item.sequence_num = (uint64_t)seq_val;
        item.status = status;

        if (item.sequence_num > max_seq) {
            max_seq = item.sequence_num;
        }

        /* Route to PriorityQueue or History based on status */
        if (status == ASSIGNMENT_STATUS_PENDING) {
            if (stage_pq.size >= PQ_CAPACITY) {
                /* Never exceed 100 pending assignments */
                fclose(fp);
                return SAPM_ERR_QUEUE_FULL;
            }
            stage_pq.items[stage_pq.size] = item;
            stage_pq.size++;
        } else if (status == ASSIGNMENT_STATUS_COMPLETED) {
            if (stage_hist.size >= HISTORY_CAPACITY) {
                fclose(fp);
                return SAPM_ERR_HISTORY_FULL;
            }
            stage_hist.items[stage_hist.size] = item;
            stage_hist.size++;
        }
    }

    if (ferror(fp) != 0) {
        fclose(fp);
        return SAPM_ERR_FILE_READ;
    }

    if (fclose(fp) != 0) {
        return SAPM_ERR_FILE_CLOSE;
    }

    /* Restore monotonic sequence for future insertions */
    stage_pq.next_sequence = max_seq + 1;
    if (stage_pq.next_sequence == 0) {
        stage_pq.next_sequence = 1;
    }

    /* Atomically commit loaded staging data to active structures */
    *pq = stage_pq;
    *hist = stage_hist;

    return SAPM_SUCCESS;
}
