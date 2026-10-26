#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "assignment.h"
#include "date_utils.h"
#include "history.h"
#include "priority_queue.h"
#include "storage.h"

#define DATA_FILE "assignments_data.csv"

static void trim_trailing_newline(char *str) {
    if (str == NULL) return;
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
        len--;
    }
}

static bool prompt_string(const char *prompt, char *dest, size_t dest_size) {
    printf("%s: ", prompt);
    if (fgets(dest, (int)dest_size, stdin) == NULL) {
        return false;
    }
    trim_trailing_newline(dest);
    return true;
}

static bool prompt_int(const char *prompt, int *out_val) {
    char buf[64];
    printf("%s: ", prompt);
    if (fgets(buf, sizeof(buf), stdin) == NULL) {
        return false;
    }
    char *endptr = NULL;
    long val = strtol(buf, &endptr, 10);
    if (endptr == buf || (*endptr != '\n' && *endptr != '\r' && *endptr != '\0')) {
        return false;
    }
    *out_val = (int)val;
    return true;
}

static void print_menu(void) {
    printf("\n=======================================================\n");
    printf("     STUDENT ASSIGNMENT PRIORITY MANAGER (SAPM)        \n");
    printf("=======================================================\n");
    printf(" [1] Add New Assignment\n");
    printf(" [2] Display Pending Assignments (Queue View)\n");
    printf(" [3] Display Priority Focus Order (Sorted Snapshot)\n");
    printf(" [4] Find Highest-Priority Assignment (Peek)\n");
    printf(" [5] Process Next Assignment (Extract Highest -> History)\n");
    printf(" [6] Process Assignment by ID (Move to History)\n");
    printf(" [7] Search Assignment (by ID or Title)\n");
    printf(" [8] Update Priority or Deadline\n");
    printf(" [9] Delete a Pending Assignment\n");
    printf(" [10] View Completed Assignments History\n");
    printf(" [11] Save Data to Disk\n");
    printf(" [0] Exit (with Auto-Save)\n");
    printf("=======================================================\n");
}

static void handle_add_assignment(PriorityQueue *pq) {
    int id = 0;
    char title[ASSIGNMENT_TITLE_MAX];
    char subject[ASSIGNMENT_SUBJECT_MAX];
    char date_str[32];
    char priority_str[32];

    printf("\n--- Add New Assignment ---\n");
    if (!prompt_int("Enter Unique Assignment ID (> 0)", &id) || id <= 0) {
        printf("Invalid ID provided.\n");
        return;
    }

    if (!prompt_string("Enter Assignment Title", title, sizeof(title)) || strlen(title) == 0) {
        printf("Title cannot be empty.\n");
        return;
    }

    if (!prompt_string("Enter Subject / Course", subject, sizeof(subject)) || strlen(subject) == 0) {
        printf("Subject cannot be empty.\n");
        return;
    }

    if (!prompt_string("Enter Deadline (YYYY-MM-DD)", date_str, sizeof(date_str))) {
        printf("Failed to read deadline.\n");
        return;
    }

    Date deadline;
    if (date_parse(date_str, &deadline) != 0 || !date_is_valid(&deadline)) {
        printf("Error: Invalid calendar date '%s'. Check month, day, or leap year.\n", date_str);
        return;
    }

    printf("Priority Levels: 1=LOW, 2=MEDIUM, 3=HIGH, 4=URGENT\n");
    if (!prompt_string("Enter Priority (1-4 or LOW/MEDIUM/HIGH/URGENT)", priority_str, sizeof(priority_str))) {
        printf("Failed to read priority.\n");
        return;
    }

    Priority priority;
    if (priority_from_string(priority_str, &priority) != SAPM_SUCCESS) {
        printf("Error: Invalid priority '%s'. Must be 1-4 or LOW/MEDIUM/HIGH/URGENT.\n", priority_str);
        return;
    }

    SAPM_Status st = pq_add(pq, id, title, subject, &deadline, priority);
    if (st == SAPM_SUCCESS) {
        printf("Success: Assignment #%d added to pending queue!\n", id);
    } else {
        printf("Failed to add assignment: %s\n", sapm_status_to_string(st));
    }
}

static void handle_search(const PriorityQueue *pq) {
    printf("\n--- Search Assignments ---\n");
    printf(" [1] Search by ID\n");
    printf(" [2] Search by Title\n");
    int choice = 0;
    if (!prompt_int("Select Search Type", &choice)) {
        return;
    }

    if (choice == 1) {
        int id = 0;
        if (!prompt_int("Enter ID to search", &id)) return;
        Assignment res;
        SAPM_Status st = pq_search_by_id(pq, id, &res);
        if (st == SAPM_SUCCESS) {
            printf("\nMatch Found:\n  ");
            assignment_print(&res, stdout);
        } else {
            printf("No pending assignment found with ID %d.\n", id);
        }
    } else if (choice == 2) {
        char title[128];
        if (!prompt_string("Enter Title to search", title, sizeof(title))) return;
        Assignment matches[PQ_CAPACITY];
        size_t count = 0;
        SAPM_Status st = pq_search_by_title_all(pq, title, matches, PQ_CAPACITY, &count);
        if (st == SAPM_SUCCESS && count > 0) {
            printf("\nFound %zu matching assignment(s):\n", count);
            for (size_t i = 0; i < count; i++) {
                printf("  [%zu] ", i + 1);
                assignment_print(&matches[i], stdout);
            }
        } else {
            printf("No pending assignments match title query '%s'.\n", title);
        }
    } else {
        printf("Invalid search choice.\n");
    }
}

static void handle_update(PriorityQueue *pq) {
    printf("\n--- Update Assignment ---\n");
    int id = 0;
    if (!prompt_int("Enter Assignment ID to update", &id) || id <= 0) {
        printf("Invalid ID.\n");
        return;
    }

    Assignment existing;
    if (pq_search_by_id(pq, id, &existing) != SAPM_SUCCESS) {
        printf("Error: Assignment ID %d not found in pending queue.\n", id);
        return;
    }

    printf("Current Assignment:\n  ");
    assignment_print(&existing, stdout);

    printf("\nUpdate Options:\n");
    printf(" [1] Update Priority\n");
    printf(" [2] Update Deadline\n");
    printf(" [3] Update Both\n");
    int choice = 0;
    if (!prompt_int("Select update option", &choice)) return;

    if (choice == 1 || choice == 3) {
        char pstr[32];
        printf("Priority Levels: 1=LOW, 2=MEDIUM, 3=HIGH, 4=URGENT\n");
        if (!prompt_string("Enter New Priority", pstr, sizeof(pstr))) return;
        Priority np;
        if (priority_from_string(pstr, &np) != SAPM_SUCCESS) {
            printf("Error: Invalid priority '%s'.\n", pstr);
            return;
        }
        if (choice == 1) {
            SAPM_Status st = pq_update_priority(pq, id, np);
            printf("%s\n", sapm_status_to_string(st));
            return;
        }
        existing.priority = np;
    }

    if (choice == 2 || choice == 3) {
        char dstr[32];
        if (!prompt_string("Enter New Deadline (YYYY-MM-DD)", dstr, sizeof(dstr))) return;
        Date nd;
        if (date_parse(dstr, &nd) != 0 || !date_is_valid(&nd)) {
            printf("Error: Invalid date '%s'.\n", dstr);
            return;
        }
        if (choice == 2) {
            SAPM_Status st = pq_update_deadline(pq, id, &nd);
            printf("%s\n", sapm_status_to_string(st));
            return;
        }
        existing.deadline = nd;
    }

    if (choice == 3) {
        SAPM_Status st = pq_update_priority_and_deadline(pq, id, existing.priority, &existing.deadline);
        printf("%s\n", sapm_status_to_string(st));
    }
}

int main(void) {
    PriorityQueue pq;
    AssignmentHistory hist;
    pq_init(&pq);
    history_init(&hist);

    bool first_run = false;
    SAPM_Status load_st = storage_load(DATA_FILE, &pq, &hist, &first_run);
    if (load_st == SAPM_SUCCESS_FIRST_RUN || first_run) {
        printf("[SAPM] Welcome! First-run detected: Initialized fresh assignment store.\n");
    } else if (load_st == SAPM_SUCCESS) {
        printf("[SAPM] Loaded %zu pending assignment(s) and %zu completed record(s) from '%s'.\n",
               pq_size(&pq), history_size(&hist), DATA_FILE);
    } else {
        printf("[SAPM] Warning: Could not load data (%s). Starting with empty queue.\n",
               sapm_status_to_string(load_st));
    }

    int choice = -1;
    while (choice != 0) {
        print_menu();
        if (!prompt_int("Select an option", &choice)) {
            printf("Please enter a valid number.\n");
            continue;
        }

        switch (choice) {
            case 1:
                handle_add_assignment(&pq);
                break;
            case 2:
                printf("\n");
                pq_display(&pq, stdout);
                break;
            case 3: {
                printf("\n--- Priority Focus Order (Sorted View) ---\n");
                Assignment sorted[PQ_CAPACITY];
                size_t count = 0;
                if (pq_get_sorted_view(&pq, sorted, PQ_CAPACITY, &count) == SAPM_SUCCESS) {
                    if (count == 0) {
                        printf("  [Queue is empty]\n");
                    } else {
                        for (size_t i = 0; i < count; i++) {
                            printf("  [%2zu] ", i + 1);
                            assignment_print(&sorted[i], stdout);
                        }
                    }
                }
                break;
            }
            case 4: {
                printf("\n--- Highest-Priority Assignment (Peek) ---\n");
                Assignment top;
                SAPM_Status st = pq_peek_highest(&pq, &top);
                if (st == SAPM_SUCCESS) {
                    printf("Top Priority:\n  ");
                    assignment_print(&top, stdout);
                } else {
                    printf("Queue is empty: %s\n", sapm_status_to_string(st));
                }
                break;
            }
            case 5: {
                printf("\n--- Process Next Assignment ---\n");
                Assignment processed;
                SAPM_Status st = pq_process_highest(&pq, &processed);
                if (st == SAPM_SUCCESS) {
                    history_add(&hist, &processed);
                    printf("Success: Processed highest-priority assignment and moved to history:\n  ");
                    assignment_print(&processed, stdout);
                } else {
                    printf("Cannot process: %s\n", sapm_status_to_string(st));
                }
                break;
            }
            case 6: {
                printf("\n--- Process Assignment by ID ---\n");
                int id = 0;
                if (prompt_int("Enter ID to mark completed", &id)) {
                    Assignment processed;
                    SAPM_Status st = pq_process_by_id(&pq, id, &processed);
                    if (st == SAPM_SUCCESS) {
                        history_add(&hist, &processed);
                        printf("Success: Assignment #%d processed and moved to history:\n  ", id);
                        assignment_print(&processed, stdout);
                    } else {
                        printf("Failed: %s\n", sapm_status_to_string(st));
                    }
                }
                break;
            }
            case 7:
                handle_search(&pq);
                break;
            case 8:
                handle_update(&pq);
                break;
            case 9: {
                printf("\n--- Delete Pending Assignment ---\n");
                int id = 0;
                if (prompt_int("Enter Assignment ID to delete", &id)) {
                    Assignment deleted;
                    SAPM_Status st = pq_delete_by_id(&pq, id, &deleted);
                    if (st == SAPM_SUCCESS) {
                        printf("Success: Deleted assignment:\n  ");
                        assignment_print(&deleted, stdout);
                    } else {
                        printf("Failed: %s\n", sapm_status_to_string(st));
                    }
                }
                break;
            }
            case 10:
                printf("\n");
                history_display(&hist, stdout);
                break;
            case 11: {
                SAPM_Status st = storage_save(DATA_FILE, &pq, &hist);
                if (st == SAPM_SUCCESS) {
                    printf("Success: Saved %zu pending and %zu completed record(s) to '%s'.\n",
                           pq_size(&pq), history_size(&hist), DATA_FILE);
                } else {
                    printf("Error: Failed to save data: %s\n", sapm_status_to_string(st));
                }
                break;
            }
            case 0: {
                printf("[SAPM] Saving changes before exit...\n");
                SAPM_Status st = storage_save(DATA_FILE, &pq, &hist);
                if (st == SAPM_SUCCESS) {
                    printf("[SAPM] Auto-save complete. Goodbye!\n");
                } else {
                    printf("[SAPM] Warning: Could not auto-save data: %s\n", sapm_status_to_string(st));
                }
                break;
            }
            default:
                printf("Invalid selection. Please choose an option from the menu.\n");
                break;
        }
    }

    return 0;
}
