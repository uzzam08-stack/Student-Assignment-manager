#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "assignment.h"
#include "date_utils.h"
#include "history.h"
#include "priority_queue.h"
#include "storage.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    tests_run++; \
    if (cond) { \
        tests_passed++; \
    } else { \
        tests_failed++; \
        printf("  [FAIL] Line %d: %s\n", __LINE__, msg); \
    } \
} while (0)

#define TEST_SECTION(name) printf("\n=== %s ===\n", name)

#define TEST_TMP_DIR "/tmp/sapm_test"

static void setup_test_dir(void) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", TEST_TMP_DIR);
    if (system(cmd) != 0) {
        /* Ignore if directory already exists */
    }
}

static void cleanup_test_dir(void) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", TEST_TMP_DIR);
    if (system(cmd) != 0) {
        /* Ignore */
    }
}

/* ========================================================================= */
/* Test Group 1: Missing File First-Run Scenario                             */
/* ========================================================================= */
static void test_first_run_missing_file(void) {
    TEST_SECTION("Storage Test 1: Missing File / First-Run Scenario");

    PriorityQueue pq;
    AssignmentHistory hist;
    pq_init(&pq);
    history_init(&hist);

    const char *missing_path = TEST_TMP_DIR "/non_existent_data.csv";
    remove(missing_path);

    bool first_run = false;
    SAPM_Status st = storage_load(missing_path, &pq, &hist, &first_run);

    TEST_ASSERT(st == SAPM_SUCCESS_FIRST_RUN, "Missing file returns SAPM_SUCCESS_FIRST_RUN");
    TEST_ASSERT(first_run == true, "first_run flag is set to true");
    TEST_ASSERT(pq_size(&pq) == 0, "Queue is initialized to empty");
    TEST_ASSERT(history_size(&hist) == 0, "History is initialized to empty");
}

/* ========================================================================= */
/* Test Group 2: Restart Persistence (Roundtrip Save & Load)                */
/* ========================================================================= */
static void test_save_and_load_roundtrip(void) {
    TEST_SECTION("Storage Test 2: Restart Persistence (Roundtrip Save & Load)");

    const char *path = TEST_TMP_DIR "/roundtrip.csv";
    remove(path);

    PriorityQueue pq;
    AssignmentHistory hist;
    pq_init(&pq);
    history_init(&hist);

    Date d1 = { 2026, 10, 20 };
    Date d2 = { 2026, 11, 15 };
    Date d3 = { 2026, 12, 1 };

    /* Add 3 pending assignments */
    TEST_ASSERT(pq_add(&pq, 1, "Algorithms Lab", "CS201", &d1, PRIORITY_HIGH) == SAPM_SUCCESS, "Add pending 1");
    TEST_ASSERT(pq_add(&pq, 2, "Physics Homework", "PHYS101", &d2, PRIORITY_MEDIUM) == SAPM_SUCCESS, "Add pending 2");
    TEST_ASSERT(pq_add(&pq, 3, "Urgent Thesis Proposal", "GRAD101", &d1, PRIORITY_URGENT) == SAPM_SUCCESS, "Add pending 3");

    /* Add 2 completed assignments to history */
    Assignment c1, c2;
    assignment_create(10, "Calculus Midterm", "MATH101", &d1, PRIORITY_HIGH, &c1);
    c1.sequence_num = 100;
    c1.status = ASSIGNMENT_STATUS_COMPLETED;
    history_add(&hist, &c1);

    assignment_create(11, "Chemistry Quiz", "CHEM101", &d3, PRIORITY_LOW, &c2);
    c2.sequence_num = 101;
    c2.status = ASSIGNMENT_STATUS_COMPLETED;
    history_add(&hist, &c2);

    /* Save to disk */
    SAPM_Status save_st = storage_save(path, &pq, &hist);
    TEST_ASSERT(save_st == SAPM_SUCCESS, "storage_save succeeds");
    TEST_ASSERT(access(path, F_OK) == 0, "Target file exists on disk");

    /* Clear in-memory structures to simulate shutdown */
    pq_clear(&pq);
    history_clear(&hist);
    TEST_ASSERT(pq_size(&pq) == 0, "In-memory queue cleared");
    TEST_ASSERT(history_size(&hist) == 0, "In-memory history cleared");

    /* Reload from disk */
    bool first_run = true;
    SAPM_Status load_st = storage_load(path, &pq, &hist, &first_run);
    TEST_ASSERT(load_st == SAPM_SUCCESS, "storage_load succeeds");
    TEST_ASSERT(first_run == false, "first_run is false for existing file");
    TEST_ASSERT(pq_size(&pq) == 3, "Queue size restored to 3");
    TEST_ASSERT(history_size(&hist) == 2, "History size restored to 2");

    /* Verify pending records */
    Assignment check;
    TEST_ASSERT(pq_search_by_id(&pq, 1, &check) == SAPM_SUCCESS, "ID 1 exists in restored queue");
    TEST_ASSERT(strcmp(check.title, "Algorithms Lab") == 0, "ID 1 title matches");
    TEST_ASSERT(strcmp(check.subject, "CS201") == 0, "ID 1 subject matches");
    TEST_ASSERT(check.priority == PRIORITY_HIGH, "ID 1 priority matches");
    TEST_ASSERT(check.deadline.year == 2026 && check.deadline.month == 10 && check.deadline.day == 20, "ID 1 deadline matches");
    TEST_ASSERT(check.status == ASSIGNMENT_STATUS_PENDING, "ID 1 status is PENDING");

    /* Verify priority ordering in restored queue */
    TEST_ASSERT(pq_peek_highest(&pq, &check) == SAPM_SUCCESS && check.id == 3, "Highest priority is ID 3 (URGENT)");

    /* Verify completed records */
    Assignment hist_check;
    TEST_ASSERT(history_get_by_id(&hist, 10, &hist_check) == SAPM_SUCCESS, "ID 10 recovered in history");
    TEST_ASSERT(hist_check.status == ASSIGNMENT_STATUS_COMPLETED, "ID 10 status is COMPLETED");
    TEST_ASSERT(strcmp(hist_check.title, "Calculus Midterm") == 0, "ID 10 title matches");

    TEST_ASSERT(history_get_by_id(&hist, 11, &hist_check) == SAPM_SUCCESS, "ID 11 recovered in history");
    TEST_ASSERT(hist_check.status == ASSIGNMENT_STATUS_COMPLETED, "ID 11 status is COMPLETED");
}

/* ========================================================================= */
/* Test Group 3: Sequence Number Continuity and Tie-Breaking After Restart   */
/* ========================================================================= */
static void test_sequence_continuity(void) {
    TEST_SECTION("Storage Test 3: Sequence Continuity & Tie-Breaking After Restart");

    const char *path = TEST_TMP_DIR "/sequence_test.csv";
    remove(path);

    PriorityQueue pq;
    AssignmentHistory hist;
    pq_init(&pq);
    history_init(&hist);

    Date d = { 2026, 10, 25 };

    /* Insert 2 items with identical priority and deadline */
    pq_add(&pq, 100, "Old Task 1", "SUB", &d, PRIORITY_HIGH);
    pq_add(&pq, 200, "Old Task 2", "SUB", &d, PRIORITY_HIGH);

    storage_save(path, &pq, &hist);

    /* Reload */
    pq_clear(&pq);
    storage_load(path, &pq, &hist, NULL);

    /* Insert a new item after restart with identical priority and deadline */
    pq_add(&pq, 300, "New Task After Restart", "SUB", &d, PRIORITY_HIGH);

    /* Processing highest must respect arrival order: 100, then 200, then 300! */
    Assignment top;
    TEST_ASSERT(pq_process_highest(&pq, &top) == SAPM_SUCCESS && top.id == 100, "First processed is ID 100");
    TEST_ASSERT(pq_process_highest(&pq, &top) == SAPM_SUCCESS && top.id == 200, "Second processed is ID 200");
    TEST_ASSERT(pq_process_highest(&pq, &top) == SAPM_SUCCESS && top.id == 300, "Third processed is ID 300 (newly inserted)");
}

/* ========================================================================= */
/* Test Group 4: RFC 4180 Escaping with Quotes and Commas                   */
/* ========================================================================= */
static void test_csv_escaping(void) {
    TEST_SECTION("Storage Test 4: RFC 4180 CSV Escaping (Quotes and Commas)");

    const char *path = TEST_TMP_DIR "/escaping_test.csv";
    remove(path);

    PriorityQueue pq;
    AssignmentHistory hist;
    pq_init(&pq);
    history_init(&hist);

    Date d = { 2026, 11, 10 };

    /* Title with comma and quotes */
    const char *complex_title = "Data Structures, Part 1: \"Pointers & Trees\"";
    const char *complex_subject = "CS, EE & MATH";

    TEST_ASSERT(pq_add(&pq, 55, complex_title, complex_subject, &d, PRIORITY_URGENT) == SAPM_SUCCESS, "Insert assignment with special chars");

    TEST_ASSERT(storage_save(path, &pq, &hist) == SAPM_SUCCESS, "Save assignment with quotes/commas");

    pq_clear(&pq);
    TEST_ASSERT(storage_load(path, &pq, &hist, NULL) == SAPM_SUCCESS, "Load assignment with quotes/commas");

    Assignment loaded;
    TEST_ASSERT(pq_search_by_id(&pq, 55, &loaded) == SAPM_SUCCESS, "Found loaded assignment");
    TEST_ASSERT(strcmp(loaded.title, complex_title) == 0, "Title with commas and quotes accurately preserved");
    TEST_ASSERT(strcmp(loaded.subject, complex_subject) == 0, "Subject with commas accurately preserved");
}

/* ========================================================================= */
/* Test Group 4b: Whitespace Resilience in Unquoted CSV Fields               */
/* ========================================================================= */
static void test_csv_whitespace_resilience(void) {
    TEST_SECTION("Storage Test 4b: Whitespace Resilience in Unquoted CSV Fields");

    const char *path = TEST_TMP_DIR "/whitespace_test.csv";
    FILE *fp = fopen(path, "w");
    fprintf(fp, "%s\n", STORAGE_HEADER);
    fprintf(fp, " 77 , Spaced Title , Spaced Subject , 2026-10-25 , HIGH , 12 , PENDING \n");
    fclose(fp);

    PriorityQueue pq;
    AssignmentHistory hist;
    pq_init(&pq);
    history_init(&hist);

    SAPM_Status st = storage_load(path, &pq, &hist, NULL);
    TEST_ASSERT(st == SAPM_SUCCESS, "Load with padded whitespace succeeds");
    TEST_ASSERT(pq_size(&pq) == 1, "Queue has 1 item");

    Assignment item;
    TEST_ASSERT(pq_search_by_id(&pq, 77, &item) == SAPM_SUCCESS, "Found ID 77");
    TEST_ASSERT(strcmp(item.title, "Spaced Title") == 0, "Title whitespace trimmed properly");
    TEST_ASSERT(strcmp(item.subject, "Spaced Subject") == 0, "Subject whitespace trimmed properly");
    TEST_ASSERT(item.priority == PRIORITY_HIGH, "Priority HIGH parsed correctly");
    TEST_ASSERT(item.status == ASSIGNMENT_STATUS_PENDING, "Status PENDING parsed correctly");
}

/* ========================================================================= */
/* Test Group 5: Capacity Overflow Rejection on Load (> 100 Pending)         */
/* ========================================================================= */
static void test_load_capacity_overflow(void) {
    TEST_SECTION("Storage Test 5: Rejecting Capacity Overflow on Load (> 100)");

    const char *path = TEST_TMP_DIR "/overflow_load.csv";
    remove(path);

    FILE *fp = fopen(path, "w");
    TEST_ASSERT(fp != NULL, "Created overflow test file");

    fprintf(fp, "%s\n", STORAGE_HEADER);
    /* Write 101 pending assignments */
    for (int i = 1; i <= 101; i++) {
        fprintf(fp, "%d,Task %d,SUB,2026-10-20,LOW,%d,PENDING\n", i, i, i);
    }
    fclose(fp);

    PriorityQueue pq;
    AssignmentHistory hist;
    pq_init(&pq);
    history_init(&hist);

    SAPM_Status st = storage_load(path, &pq, &hist, NULL);
    TEST_ASSERT(st == SAPM_ERR_QUEUE_FULL, "Loading 101 pending assignments returns SAPM_ERR_QUEUE_FULL");
    TEST_ASSERT(pq_size(&pq) == 0, "Active queue remained empty (did not corrupt state)");
}

/* ========================================================================= */
/* Test Group 6: Rejecting Corrupted and Malformed Records Safely            */
/* ========================================================================= */
static void test_reject_corrupt_records(void) {
    TEST_SECTION("Storage Test 6: Safe Rejection of Corrupt / Malformed Records");

    PriorityQueue pq;
    AssignmentHistory hist;
    pq_init(&pq);
    history_init(&hist);

    /* 1. Invalid Leap Year Date in CSV (2023-02-29) */
    const char *bad_date_file = TEST_TMP_DIR "/bad_date.csv";
    FILE *fp = fopen(bad_date_file, "w");
    fprintf(fp, "%s\n", STORAGE_HEADER);
    fprintf(fp, "1,Task,SUB,2023-02-29,HIGH,1,PENDING\n");
    fclose(fp);

    SAPM_Status st = storage_load(bad_date_file, &pq, &hist, NULL);
    TEST_ASSERT(st == SAPM_ERR_INVALID_DATE, "File with invalid leap date (2023-02-29) rejected with INVALID_DATE");

    /* 2. Invalid Priority */
    const char *bad_prio_file = TEST_TMP_DIR "/bad_prio.csv";
    fp = fopen(bad_prio_file, "w");
    fprintf(fp, "%s\n", STORAGE_HEADER);
    fprintf(fp, "1,Task,SUB,2026-10-20,SUPER_HIGH,1,PENDING\n");
    fclose(fp);

    st = storage_load(bad_prio_file, &pq, &hist, NULL);
    TEST_ASSERT(st == SAPM_ERR_INVALID_PRIORITY, "File with invalid priority rejected");

    /* 3. Invalid Status */
    const char *bad_status_file = TEST_TMP_DIR "/bad_status.csv";
    fp = fopen(bad_status_file, "w");
    fprintf(fp, "%s\n", STORAGE_HEADER);
    fprintf(fp, "1,Task,SUB,2026-10-20,HIGH,1,IN_PROGRESS\n");
    fclose(fp);

    st = storage_load(bad_status_file, &pq, &hist, NULL);
    TEST_ASSERT(st == SAPM_ERR_INVALID_STATUS, "File with invalid status rejected");

    /* 4. Malformed CSV Line (too few columns) */
    const char *truncated_file = TEST_TMP_DIR "/truncated.csv";
    fp = fopen(truncated_file, "w");
    fprintf(fp, "%s\n", STORAGE_HEADER);
    fprintf(fp, "1,Task,SUB,2026-10-20\n");
    fclose(fp);

    st = storage_load(truncated_file, &pq, &hist, NULL);
    TEST_ASSERT(st == SAPM_ERR_CORRUPT_RECORD, "File with truncated columns rejected");

    /* 5. Non-numeric ID */
    const char *bad_id_file = TEST_TMP_DIR "/bad_id.csv";
    fp = fopen(bad_id_file, "w");
    fprintf(fp, "%s\n", STORAGE_HEADER);
    fprintf(fp, "NOT_AN_ID,Task,SUB,2026-10-20,HIGH,1,PENDING\n");
    fclose(fp);

    st = storage_load(bad_id_file, &pq, &hist, NULL);
    TEST_ASSERT(st == SAPM_ERR_CORRUPT_RECORD, "File with non-numeric ID rejected");

    /* 6. Corrupt Header */
    const char *bad_hdr_file = TEST_TMP_DIR "/bad_hdr.csv";
    fp = fopen(bad_hdr_file, "w");
    fprintf(fp, "foo,bar,baz\n");
    fclose(fp);

    st = storage_load(bad_hdr_file, &pq, &hist, NULL);
    TEST_ASSERT(st == SAPM_ERR_CORRUPT_RECORD, "File with missing standard header rejected");
}

/* ========================================================================= */
/* Test Group 7: Duplicate ID Rejection on Load                              */
/* ========================================================================= */
static void test_load_duplicate_id(void) {
    TEST_SECTION("Storage Test 7: Duplicate ID Rejection on Load");

    const char *path = TEST_TMP_DIR "/dup_id.csv";
    FILE *fp = fopen(path, "w");
    fprintf(fp, "%s\n", STORAGE_HEADER);
    fprintf(fp, "5,Task A,SUB,2026-10-20,HIGH,1,PENDING\n");
    fprintf(fp, "5,Task B,SUB,2026-10-21,LOW,2,PENDING\n");
    fclose(fp);

    PriorityQueue pq;
    AssignmentHistory hist;
    pq_init(&pq);
    history_init(&hist);

    SAPM_Status st = storage_load(path, &pq, &hist, NULL);
    TEST_ASSERT(st == SAPM_ERR_DUPLICATE_ID, "Duplicate ID within file rejected with SAPM_ERR_DUPLICATE_ID");
    TEST_ASSERT(pq_size(&pq) == 0, "Queue remained empty after rejected load");
}

/* ========================================================================= */
/* Test Group 8: Transactional Integrity (Never Silently Reset on Failure)   */
/* ========================================================================= */
static void test_transactional_in_memory_preservation(void) {
    TEST_SECTION("Storage Test 8: Transactional Staging (Never Reset on Failure)");

    PriorityQueue pq;
    AssignmentHistory hist;
    pq_init(&pq);
    history_init(&hist);

    Date d = { 2026, 10, 20 };

    /* Seed active memory with valid assignments */
    pq_add(&pq, 10, "Existing Task 10", "MATH", &d, PRIORITY_HIGH);
    pq_add(&pq, 20, "Existing Task 20", "PHYS", &d, PRIORITY_LOW);

    Assignment comp;
    assignment_create(30, "Completed Task 30", "CHEM", &d, PRIORITY_MEDIUM, &comp);
    history_add(&hist, &comp);

    TEST_ASSERT(pq_size(&pq) == 2, "Active queue has 2 items");
    TEST_ASSERT(history_size(&hist) == 1, "Active history has 1 item");

    /* Attempt to load a corrupted file */
    const char *corrupted_file = TEST_TMP_DIR "/bad_data.csv";
    FILE *fp = fopen(corrupted_file, "w");
    fprintf(fp, "%s\n", STORAGE_HEADER);
    fprintf(fp, "99,Corrupt Task,SUB,2023-02-29,HIGH,1,PENDING\n"); /* Invalid date */
    fclose(fp);

    SAPM_Status st = storage_load(corrupted_file, &pq, &hist, NULL);
    TEST_ASSERT(st != SAPM_SUCCESS, "Loading corrupt file fails");

    /* Active queue and history MUST NOT BE RESET! */
    TEST_ASSERT(pq_size(&pq) == 2, "Active queue size is still 2");
    TEST_ASSERT(history_size(&hist) == 1, "Active history size is still 1");

    Assignment check;
    TEST_ASSERT(pq_search_by_id(&pq, 10, &check) == SAPM_SUCCESS, "Existing item 10 is intact");
    TEST_ASSERT(pq_search_by_id(&pq, 20, &check) == SAPM_SUCCESS, "Existing item 20 is intact");
    TEST_ASSERT(history_get_by_id(&hist, 30, &check) == SAPM_SUCCESS, "Existing history item 30 is intact");
}

/* ========================================================================= */
/* Test Group 9: Atomic Replacement & Failed Write Safety                    */
/* ========================================================================= */
static void test_atomic_save_safety(void) {
    TEST_SECTION("Storage Test 9: Atomic Replacement & Write Error Handling");

    PriorityQueue pq;
    AssignmentHistory hist;
    pq_init(&pq);
    history_init(&hist);

    Date d = { 2026, 10, 20 };
    pq_add(&pq, 1, "Valid Task", "SUB", &d, PRIORITY_HIGH);

    /* 1. Attempt save to non-writable directory */
    const char *unwritable_path = "/non_existent_dir_12345/data.csv";
    SAPM_Status st = storage_save(unwritable_path, &pq, &hist);
    TEST_ASSERT(st == SAPM_ERR_FILE_OPEN, "Save to non-writable path returns SAPM_ERR_FILE_OPEN");

    /* 2. Save to valid path */
    const char *valid_path = TEST_TMP_DIR "/atomic_target.csv";
    st = storage_save(valid_path, &pq, &hist);
    TEST_ASSERT(st == SAPM_SUCCESS, "Save to valid path succeeds");

    /* Verify temporary file .tmp was removed/renamed */
    char tmp_path[512];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", valid_path);
    TEST_ASSERT(access(tmp_path, F_OK) != 0, "Temporary file was cleaned up / renamed");
    TEST_ASSERT(access(valid_path, F_OK) == 0, "Target file exists");
}

/* ========================================================================= */
/* Main Runner                                                               */
/* ========================================================================= */
int main(void) {
    printf("=================================================================\n");
    printf("  STUDENT ASSIGNMENT PRIORITY MANAGER - STORAGE TEST SUITE        \n");
    printf("=================================================================\n");

    setup_test_dir();

    test_first_run_missing_file();
    test_save_and_load_roundtrip();
    test_sequence_continuity();
    test_csv_escaping();
    test_csv_whitespace_resilience();
    test_load_capacity_overflow();
    test_reject_corrupt_records();
    test_load_duplicate_id();
    test_transactional_in_memory_preservation();
    test_atomic_save_safety();

    cleanup_test_dir();

    printf("\n=================================================================\n");
    printf("STORAGE TEST SUMMARY: Total: %d | Passed: %d | Failed: %d\n",
           tests_run, tests_passed, tests_failed);
    printf("=================================================================\n");

    return (tests_failed == 0) ? 0 : 1;
}
