#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assignment.h"
#include "date_utils.h"
#include "priority_queue.h"

/* Test harness macros and counters */
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

/* ========================================================================= */
/* Test Group 1: Calendar Dates and Leap Year Validation                     */
/* ========================================================================= */
static void test_date_validation(void) {
    TEST_SECTION("Test Group 1: Calendar Dates & Leap Year Validation");

    /* Leap year checks */
    TEST_ASSERT(date_is_leap_year(2024) == true, "2024 is a leap year (divisible by 4)");
    TEST_ASSERT(date_is_leap_year(2020) == true, "2020 is a leap year (divisible by 4)");
    TEST_ASSERT(date_is_leap_year(2000) == true, "2000 is a leap year (divisible by 400)");
    TEST_ASSERT(date_is_leap_year(2400) == true, "2400 is a leap year (divisible by 400)");
    TEST_ASSERT(date_is_leap_year(1900) == false, "1900 is NOT a leap year (divisible by 100, not 400)");
    TEST_ASSERT(date_is_leap_year(2100) == false, "2100 is NOT a leap year (divisible by 100, not 400)");
    TEST_ASSERT(date_is_leap_year(2023) == false, "2023 is NOT a leap year");
    TEST_ASSERT(date_is_leap_year(2025) == false, "2025 is NOT a leap year");
    TEST_ASSERT(date_is_leap_year(0) == false, "Year 0 is invalid/not leap");
    TEST_ASSERT(date_is_leap_year(-4) == false, "Negative year is invalid/not leap");

    /* Days in month */
    TEST_ASSERT(date_days_in_month(2024, 2) == 29, "Feb 2024 has 29 days");
    TEST_ASSERT(date_days_in_month(2023, 2) == 28, "Feb 2023 has 28 days");
    TEST_ASSERT(date_days_in_month(1900, 2) == 28, "Feb 1900 has 28 days");
    TEST_ASSERT(date_days_in_month(2000, 2) == 29, "Feb 2000 has 29 days");
    TEST_ASSERT(date_days_in_month(2024, 4) == 30, "April has 30 days");
    TEST_ASSERT(date_days_in_month(2024, 1) == 31, "January has 31 days");
    TEST_ASSERT(date_days_in_month(2024, 0) == -1, "Month 0 returns -1");
    TEST_ASSERT(date_days_in_month(2024, 13) == -1, "Month 13 returns -1");

    /* Valid Date structures */
    Date d;
    TEST_ASSERT(date_create(2024, 2, 29, &d) == 0 && d.day == 29, "2024-02-29 is valid");
    TEST_ASSERT(date_create(2000, 2, 29, &d) == 0 && d.day == 29, "2000-02-29 is valid");
    TEST_ASSERT(date_create(2023, 2, 28, &d) == 0 && d.day == 28, "2023-02-28 is valid");
    TEST_ASSERT(date_create(2024, 12, 31, &d) == 0 && d.day == 31, "2024-12-31 is valid");
    TEST_ASSERT(date_create(2024, 4, 30, &d) == 0 && d.day == 30, "2024-04-30 is valid");

    /* Invalid Date structures */
    TEST_ASSERT(date_create(2023, 2, 29, &d) < 0, "2023-02-29 rejected (non-leap year)");
    TEST_ASSERT(date_create(1900, 2, 29, &d) < 0, "1900-02-29 rejected (century non-leap)");
    TEST_ASSERT(date_create(2100, 2, 29, &d) < 0, "2100-02-29 rejected (century non-leap)");
    TEST_ASSERT(date_create(2024, 4, 31, &d) < 0, "2024-04-31 rejected (April has 30 days)");
    TEST_ASSERT(date_create(2024, 6, 31, &d) < 0, "2024-06-31 rejected (June has 30 days)");
    TEST_ASSERT(date_create(2024, 9, 31, &d) < 0, "2024-09-31 rejected (September has 30 days)");
    TEST_ASSERT(date_create(2024, 11, 31, &d) < 0, "2024-11-31 rejected (November has 30 days)");
    TEST_ASSERT(date_create(2024, 1, 0, &d) < 0, "Day 0 rejected");
    TEST_ASSERT(date_create(2024, 1, 32, &d) < 0, "Day 32 rejected");
    TEST_ASSERT(date_create(2024, 0, 15, &d) < 0, "Month 0 rejected");
    TEST_ASSERT(date_create(2024, 13, 15, &d) < 0, "Month 13 rejected");
    TEST_ASSERT(date_create(0, 1, 1, &d) < 0, "Year 0 rejected");
    TEST_ASSERT(date_create(-2024, 1, 1, &d) < 0, "Negative year rejected");
    TEST_ASSERT(date_create(2024, 1, 1, NULL) < 0, "NULL out_date rejected");

    /* Date comparison */
    Date d1 = { 2026, 10, 15 };
    Date d2 = { 2026, 10, 20 };
    Date d3 = { 2026, 11, 1 };
    Date d4 = { 2027, 1, 1 };
    Date d1_copy = { 2026, 10, 15 };

    TEST_ASSERT(date_compare(&d1, &d2) < 0, "d1 (Oct 15) is earlier than d2 (Oct 20)");
    TEST_ASSERT(date_compare(&d2, &d1) > 0, "d2 (Oct 20) is later than d1 (Oct 15)");
    TEST_ASSERT(date_compare(&d1, &d1_copy) == 0, "Identical dates compare equal (0)");
    TEST_ASSERT(date_compare(&d2, &d3) < 0, "Different months compared correctly");
    TEST_ASSERT(date_compare(&d3, &d4) < 0, "Different years compared correctly");

    /* Date parsing and formatting */
    char buf[32];
    TEST_ASSERT(date_format(&d1, buf, sizeof(buf)) == 10, "Format writes 10 characters");
    TEST_ASSERT(strcmp(buf, "2026-10-15") == 0, "Formatted output is 2026-10-15");

    char small_buf[5];
    TEST_ASSERT(date_format(&d1, small_buf, sizeof(small_buf)) < 0, "Small buffer rejected safely");

    Date parsed;
    TEST_ASSERT(date_parse("2026-10-15", &parsed) == 0, "Parsed valid date string");
    TEST_ASSERT(date_compare(&d1, &parsed) == 0, "Parsed date matches expected");

    TEST_ASSERT(date_parse("2023-02-29", &parsed) < 0, "Parse rejects invalid leap date");
    TEST_ASSERT(date_parse("invalid", &parsed) < 0, "Parse rejects non-date string");
    TEST_ASSERT(date_parse("2026-13-01", &parsed) < 0, "Parse rejects invalid month");
    TEST_ASSERT(date_parse("2026-10-32", &parsed) < 0, "Parse rejects invalid day");
    TEST_ASSERT(date_parse(NULL, &parsed) < 0, "Parse rejects NULL string");
}

/* ========================================================================= */
/* Test Group 2: Priority Parsing and Levels                                 */
/* ========================================================================= */
static void test_priority_levels(void) {
    TEST_SECTION("Test Group 2: Priority Levels & Parsing");

    TEST_ASSERT(priority_is_valid(PRIORITY_LOW), "PRIORITY_LOW is valid");
    TEST_ASSERT(priority_is_valid(PRIORITY_MEDIUM), "PRIORITY_MEDIUM is valid");
    TEST_ASSERT(priority_is_valid(PRIORITY_HIGH), "PRIORITY_HIGH is valid");
    TEST_ASSERT(priority_is_valid(PRIORITY_URGENT), "PRIORITY_URGENT is valid");
    TEST_ASSERT(!priority_is_valid(PRIORITY_INVALID), "PRIORITY_INVALID is not valid");
    TEST_ASSERT(!priority_is_valid((Priority)0), "Priority 0 is invalid");
    TEST_ASSERT(!priority_is_valid((Priority)5), "Priority 5 is invalid");

    TEST_ASSERT(strcmp(priority_to_string(PRIORITY_LOW), "LOW") == 0, "LOW string");
    TEST_ASSERT(strcmp(priority_to_string(PRIORITY_MEDIUM), "MEDIUM") == 0, "MEDIUM string");
    TEST_ASSERT(strcmp(priority_to_string(PRIORITY_HIGH), "HIGH") == 0, "HIGH string");
    TEST_ASSERT(strcmp(priority_to_string(PRIORITY_URGENT), "URGENT") == 0, "URGENT string");

    Priority p;
    TEST_ASSERT(priority_from_string("low", &p) == SAPM_SUCCESS && p == PRIORITY_LOW, "Parse 'low'");
    TEST_ASSERT(priority_from_string("LOW", &p) == SAPM_SUCCESS && p == PRIORITY_LOW, "Parse 'LOW'");
    TEST_ASSERT(priority_from_string("1", &p) == SAPM_SUCCESS && p == PRIORITY_LOW, "Parse '1'");
    TEST_ASSERT(priority_from_string("medium", &p) == SAPM_SUCCESS && p == PRIORITY_MEDIUM, "Parse 'medium'");
    TEST_ASSERT(priority_from_string("2", &p) == SAPM_SUCCESS && p == PRIORITY_MEDIUM, "Parse '2'");
    TEST_ASSERT(priority_from_string("high", &p) == SAPM_SUCCESS && p == PRIORITY_HIGH, "Parse 'high'");
    TEST_ASSERT(priority_from_string("3", &p) == SAPM_SUCCESS && p == PRIORITY_HIGH, "Parse '3'");
    TEST_ASSERT(priority_from_string("urgent", &p) == SAPM_SUCCESS && p == PRIORITY_URGENT, "Parse 'urgent'");
    TEST_ASSERT(priority_from_string("4", &p) == SAPM_SUCCESS && p == PRIORITY_URGENT, "Parse '4'");
    TEST_ASSERT(priority_from_string("invalid", &p) == SAPM_ERR_INVALID_PRIORITY, "Parse invalid string fails");
    TEST_ASSERT(priority_from_string("0", &p) == SAPM_ERR_INVALID_PRIORITY, "Parse '0' fails");
    TEST_ASSERT(priority_from_string("5", &p) == SAPM_ERR_INVALID_PRIORITY, "Parse '5' fails");
    TEST_ASSERT(priority_from_string(NULL, &p) == SAPM_ERR_NULL_PARAM, "Parse NULL string fails");
}

/* ========================================================================= */
/* Test Group 3: Assignment Creation and Validation                          */
/* ========================================================================= */
static void test_assignment_validation(void) {
    TEST_SECTION("Test Group 3: Assignment Creation & Input Validation");

    Assignment a;
    Date d = { 2026, 10, 25 };

    /* Valid creation */
    SAPM_Status st = assignment_create(1, "Data Structures Lab", "CS201", &d, PRIORITY_HIGH, &a);
    TEST_ASSERT(st == SAPM_SUCCESS, "Valid assignment creation succeeds");
    TEST_ASSERT(a.id == 1, "Assignment ID matches");
    TEST_ASSERT(strcmp(a.title, "Data Structures Lab") == 0, "Assignment title matches");
    TEST_ASSERT(strcmp(a.subject, "CS201") == 0, "Assignment subject matches");
    TEST_ASSERT(a.priority == PRIORITY_HIGH, "Assignment priority matches");
    TEST_ASSERT(a.status == ASSIGNMENT_STATUS_PENDING, "Default status is PENDING");

    /* Trimming whitespace */
    st = assignment_create(2, "   Operating Systems Quiz   ", "  CS301  ", &d, PRIORITY_MEDIUM, &a);
    TEST_ASSERT(st == SAPM_SUCCESS, "Whitespace trimming succeeds");
    TEST_ASSERT(strcmp(a.title, "Operating Systems Quiz") == 0, "Title whitespace trimmed");
    TEST_ASSERT(strcmp(a.subject, "CS301") == 0, "Subject whitespace trimmed");

    /* Invalid inputs */
    TEST_ASSERT(assignment_create(0, "Title", "Sub", &d, PRIORITY_HIGH, &a) == SAPM_ERR_INVALID_ID, "ID 0 rejected");
    TEST_ASSERT(assignment_create(-5, "Title", "Sub", &d, PRIORITY_HIGH, &a) == SAPM_ERR_INVALID_ID, "Negative ID rejected");
    TEST_ASSERT(assignment_create(1, NULL, "Sub", &d, PRIORITY_HIGH, &a) == SAPM_ERR_INVALID_TITLE, "NULL title rejected");
    TEST_ASSERT(assignment_create(1, "", "Sub", &d, PRIORITY_HIGH, &a) == SAPM_ERR_INVALID_TITLE, "Empty title rejected");
    TEST_ASSERT(assignment_create(1, "   ", "Sub", &d, PRIORITY_HIGH, &a) == SAPM_ERR_INVALID_TITLE, "Whitespace-only title rejected");
    TEST_ASSERT(assignment_create(1, "Title", NULL, &d, PRIORITY_HIGH, &a) == SAPM_ERR_INVALID_SUBJECT, "NULL subject rejected");
    TEST_ASSERT(assignment_create(1, "Title", "", &d, PRIORITY_HIGH, &a) == SAPM_ERR_INVALID_SUBJECT, "Empty subject rejected");
    TEST_ASSERT(assignment_create(1, "Title", "   ", &d, PRIORITY_HIGH, &a) == SAPM_ERR_INVALID_SUBJECT, "Whitespace-only subject rejected");
    TEST_ASSERT(assignment_create(1, "Title", "Sub", NULL, PRIORITY_HIGH, &a) == SAPM_ERR_INVALID_DATE, "NULL deadline rejected");

    Date bad_date = { 2023, 2, 29 };
    TEST_ASSERT(assignment_create(1, "Title", "Sub", &bad_date, PRIORITY_HIGH, &a) == SAPM_ERR_INVALID_DATE, "Invalid date rejected");
    TEST_ASSERT(assignment_create(1, "Title", "Sub", &d, (Priority)0, &a) == SAPM_ERR_INVALID_PRIORITY, "Invalid priority rejected");
    TEST_ASSERT(assignment_create(1, "Title", "Sub", &d, (Priority)10, &a) == SAPM_ERR_INVALID_PRIORITY, "Out-of-range priority rejected");
    TEST_ASSERT(assignment_create(1, "Title", "Sub", &d, PRIORITY_HIGH, NULL) == SAPM_ERR_NULL_PARAM, "NULL out_assignment rejected");

    /* Formatting test */
    char fbuf[256];
    TEST_ASSERT(assignment_format(&a, fbuf, sizeof(fbuf)) == SAPM_SUCCESS, "Formatting assignment succeeds");
    TEST_ASSERT(strstr(fbuf, "[ID: 2]") != NULL, "Formatted output contains ID");
    TEST_ASSERT(strstr(fbuf, "Operating Systems Quiz") != NULL, "Formatted output contains title");

    char tiny_buf[10];
    TEST_ASSERT(assignment_format(&a, tiny_buf, sizeof(tiny_buf)) == SAPM_ERR_BUFFER_TOO_SMALL, "Formatting with tiny buffer fails safely");
}

/* ========================================================================= */
/* Test Group 4: Empty Queue Operations                                      */
/* ========================================================================= */
static void test_empty_queue(void) {
    TEST_SECTION("Test Group 4: Empty Queue Operations");

    PriorityQueue pq;
    TEST_ASSERT(pq_init(&pq) == SAPM_SUCCESS, "pq_init succeeds");
    TEST_ASSERT(pq_is_empty(&pq) == true, "New queue is empty");
    TEST_ASSERT(pq_is_full(&pq) == false, "New queue is not full");
    TEST_ASSERT(pq_size(&pq) == 0, "New queue size is 0");

    Assignment out_item;
    TEST_ASSERT(pq_peek_highest(&pq, &out_item) == SAPM_ERR_QUEUE_EMPTY, "Peek on empty queue returns SAPM_ERR_QUEUE_EMPTY");
    TEST_ASSERT(pq_process_highest(&pq, &out_item) == SAPM_ERR_QUEUE_EMPTY, "Process on empty queue returns SAPM_ERR_QUEUE_EMPTY");
    TEST_ASSERT(pq_process_by_id(&pq, 1, &out_item) == SAPM_ERR_QUEUE_EMPTY, "Process by ID on empty queue returns SAPM_ERR_QUEUE_EMPTY");
    TEST_ASSERT(pq_delete_by_id(&pq, 1, &out_item) == SAPM_ERR_QUEUE_EMPTY, "Delete by ID on empty queue returns SAPM_ERR_QUEUE_EMPTY");
    TEST_ASSERT(pq_search_by_id(&pq, 1, &out_item) == SAPM_ERR_NOT_FOUND, "Search ID on empty queue returns SAPM_ERR_NOT_FOUND");
    TEST_ASSERT(pq_search_by_title(&pq, "Any", &out_item) == SAPM_ERR_NOT_FOUND, "Search title on empty queue returns SAPM_ERR_NOT_FOUND");

    Assignment list[10];
    size_t count = 0;
    TEST_ASSERT(pq_get_all(&pq, list, 10, &count) == SAPM_SUCCESS && count == 0, "pq_get_all on empty queue returns 0 items");
}

/* ========================================================================= */
/* Test Group 5: All Priority Levels and Ordering                            */
/* ========================================================================= */
static void test_all_priority_levels(void) {
    TEST_SECTION("Test Group 5: All Priority Levels & Ordering");

    PriorityQueue pq;
    pq_init(&pq);

    Date d = { 2026, 10, 20 };

    /* Add assignments across all 4 priority levels in scrambled order */
    TEST_ASSERT(pq_add(&pq, 101, "Low Priority Task", "SUB1", &d, PRIORITY_LOW) == SAPM_SUCCESS, "Add LOW priority");
    TEST_ASSERT(pq_add(&pq, 102, "High Priority Task", "SUB2", &d, PRIORITY_HIGH) == SAPM_SUCCESS, "Add HIGH priority");
    TEST_ASSERT(pq_add(&pq, 103, "Urgent Priority Task", "SUB3", &d, PRIORITY_URGENT) == SAPM_SUCCESS, "Add URGENT priority");
    TEST_ASSERT(pq_add(&pq, 104, "Medium Priority Task", "SUB4", &d, PRIORITY_MEDIUM) == SAPM_SUCCESS, "Add MEDIUM priority");

    TEST_ASSERT(pq_size(&pq) == 4, "Queue size is 4");

    Assignment item;

    /* 1st highest must be URGENT (ID 103) */
    TEST_ASSERT(pq_peek_highest(&pq, &item) == SAPM_SUCCESS && item.id == 103, "Peek identifies URGENT task");
    TEST_ASSERT(pq_process_highest(&pq, &item) == SAPM_SUCCESS && item.id == 103, "Process extracts URGENT task");
    TEST_ASSERT(item.status == ASSIGNMENT_STATUS_COMPLETED, "Processed item status marked COMPLETED");

    /* 2nd highest must be HIGH (ID 102) */
    TEST_ASSERT(pq_peek_highest(&pq, &item) == SAPM_SUCCESS && item.id == 102, "Next is HIGH task");
    TEST_ASSERT(pq_process_highest(&pq, &item) == SAPM_SUCCESS && item.id == 102, "Process extracts HIGH task");

    /* 3rd highest must be MEDIUM (ID 104) */
    TEST_ASSERT(pq_peek_highest(&pq, &item) == SAPM_SUCCESS && item.id == 104, "Next is MEDIUM task");
    TEST_ASSERT(pq_process_highest(&pq, &item) == SAPM_SUCCESS && item.id == 104, "Process extracts MEDIUM task");

    /* 4th highest must be LOW (ID 101) */
    TEST_ASSERT(pq_peek_highest(&pq, &item) == SAPM_SUCCESS && item.id == 101, "Next is LOW task");
    TEST_ASSERT(pq_process_highest(&pq, &item) == SAPM_SUCCESS && item.id == 101, "Process extracts LOW task");

    /* Queue should now be empty */
    TEST_ASSERT(pq_is_empty(&pq) == true, "Queue empty after processing all items");
}

/* ========================================================================= */
/* Test Group 6: Deadline Urgency Ordering                                   */
/* ========================================================================= */
static void test_deadline_urgency(void) {
    TEST_SECTION("Test Group 6: Deadline Urgency Ordering");

    PriorityQueue pq;
    pq_init(&pq);

    Date due_late   = { 2026, 11, 20 };
    Date due_early  = { 2026, 10, 15 };
    Date due_middle = { 2026, 10, 30 };

    /* All have same priority (HIGH), so earlier deadline must take precedence */
    pq_add(&pq, 1, "Due Late", "MATH", &due_late, PRIORITY_HIGH);
    pq_add(&pq, 2, "Due Early", "MATH", &due_early, PRIORITY_HIGH);
    pq_add(&pq, 3, "Due Middle", "MATH", &due_middle, PRIORITY_HIGH);

    Assignment item;

    /* 1st must be Due Early (2026-10-15) */
    TEST_ASSERT(pq_peek_highest(&pq, &item) == SAPM_SUCCESS && item.id == 2, "Earlier deadline has highest priority");
    pq_process_highest(&pq, &item);
    TEST_ASSERT(item.id == 2, "Processed item is Due Early");

    /* 2nd must be Due Middle (2026-10-30) */
    TEST_ASSERT(pq_peek_highest(&pq, &item) == SAPM_SUCCESS && item.id == 3, "Middle deadline is next");
    pq_process_highest(&pq, &item);
    TEST_ASSERT(item.id == 3, "Processed item is Due Middle");

    /* 3rd must be Due Late (2026-11-20) */
    TEST_ASSERT(pq_peek_highest(&pq, &item) == SAPM_SUCCESS && item.id == 1, "Late deadline is last");
    pq_process_highest(&pq, &item);
    TEST_ASSERT(item.id == 1, "Processed item is Due Late");

    TEST_ASSERT(pq_is_empty(&pq) == true, "Queue is empty");
}

/* ========================================================================= */
/* Test Group 7: Tie-breaking (Equal Priority & Equal Deadline)              */
/* ========================================================================= */
static void test_tie_breaking(void) {
    TEST_SECTION("Test Group 7: Tie-Breaking (FIFO Insertion Order)");

    PriorityQueue pq;
    pq_init(&pq);

    Date deadline = { 2026, 10, 25 };

    /* Insert 3 assignments with IDENTICAL priority and IDENTICAL deadline */
    TEST_ASSERT(pq_add(&pq, 10, "First Inserted", "CS101", &deadline, PRIORITY_HIGH) == SAPM_SUCCESS, "Insert 1st");
    TEST_ASSERT(pq_add(&pq, 20, "Second Inserted", "CS101", &deadline, PRIORITY_HIGH) == SAPM_SUCCESS, "Insert 2nd");
    TEST_ASSERT(pq_add(&pq, 30, "Third Inserted", "CS101", &deadline, PRIORITY_HIGH) == SAPM_SUCCESS, "Insert 3rd");

    Assignment item;

    /* Insertion order must be strictly preserved: 10, then 20, then 30 */
    TEST_ASSERT(pq_process_highest(&pq, &item) == SAPM_SUCCESS && item.id == 10, "1st inserted is processed first (ID 10)");
    TEST_ASSERT(pq_process_highest(&pq, &item) == SAPM_SUCCESS && item.id == 20, "2nd inserted is processed second (ID 20)");
    TEST_ASSERT(pq_process_highest(&pq, &item) == SAPM_SUCCESS && item.id == 30, "3rd inserted is processed third (ID 30)");

    TEST_ASSERT(pq_is_empty(&pq) == true, "Queue is empty after tie-breaking extraction");
}

/* ========================================================================= */
/* Test Group 8: Priority Level Dominates Deadline Proximity                 */
/* ========================================================================= */
static void test_priority_dominance(void) {
    TEST_SECTION("Test Group 8: Priority Level Dominance Over Deadline");

    PriorityQueue pq;
    pq_init(&pq);

    Date tomorrow  = { 2026, 10, 18 };
    Date next_week = { 2026, 10, 25 };

    /* Task 1: Low priority, due tomorrow */
    pq_add(&pq, 1, "Low Urgent Date", "HIST", &tomorrow, PRIORITY_LOW);

    /* Task 2: High priority, due next week */
    pq_add(&pq, 2, "High Later Date", "MATH", &next_week, PRIORITY_HIGH);

    Assignment item;
    /* High priority must win even though its deadline is later */
    TEST_ASSERT(pq_peek_highest(&pq, &item) == SAPM_SUCCESS && item.id == 2,
                "HIGH priority task wins over LOW priority even if LOW deadline is earlier");

    pq_process_highest(&pq, &item);
    TEST_ASSERT(item.id == 2, "Extracted item is Task 2");

    pq_process_highest(&pq, &item);
    TEST_ASSERT(item.id == 1, "Extracted item is Task 1");
}

/* ========================================================================= */
/* Test Group 9: Duplicate ID Rejection                                      */
/* ========================================================================= */
static void test_duplicate_id_rejection(void) {
    TEST_SECTION("Test Group 9: Duplicate ID Rejection");

    PriorityQueue pq;
    pq_init(&pq);

    Date d = { 2026, 10, 20 };
    TEST_ASSERT(pq_add(&pq, 50, "Assignment Alpha", "BIO", &d, PRIORITY_MEDIUM) == SAPM_SUCCESS, "Insert initial ID 50");
    TEST_ASSERT(pq_size(&pq) == 1, "Queue size is 1");

    /* Attempt to insert duplicate ID 50 with different details */
    SAPM_Status st = pq_add(&pq, 50, "Assignment Beta", "CHEM", &d, PRIORITY_HIGH);
    TEST_ASSERT(st == SAPM_ERR_DUPLICATE_ID, "Inserting duplicate ID 50 returns SAPM_ERR_DUPLICATE_ID");
    TEST_ASSERT(pq_size(&pq) == 1, "Queue size remains 1 after rejected duplicate");

    /* Verify original assignment is unchanged */
    Assignment item;
    TEST_ASSERT(pq_search_by_id(&pq, 50, &item) == SAPM_SUCCESS, "Find original assignment");
    TEST_ASSERT(strcmp(item.title, "Assignment Alpha") == 0, "Original assignment unchanged");
}

/* ========================================================================= */
/* Test Group 10: Capacity Overflow (Full Queue - 100 Items)                 */
/* ========================================================================= */
static void test_capacity_overflow(void) {
    TEST_SECTION("Test Group 10: Queue Capacity & Overflow (Capacity 100)");

    PriorityQueue pq;
    pq_init(&pq);

    Date d = { 2026, 10, 20 };

    /* Fill queue to exactly capacity (100) */
    for (int i = 1; i <= PQ_CAPACITY; i++) {
        char title[32];
        snprintf(title, sizeof(title), "Task #%d", i);
        SAPM_Status st = pq_add(&pq, i, title, "COURSE", &d, PRIORITY_LOW);
        TEST_ASSERT(st == SAPM_SUCCESS, "Insert item within capacity");
    }

    TEST_ASSERT(pq_size(&pq) == PQ_CAPACITY, "Queue size is exactly 100");
    TEST_ASSERT(pq_is_full(&pq) == true, "pq_is_full reports true");

    /* Attempt 101st insertion */
    SAPM_Status overflow_st = pq_add(&pq, 101, "Overflow Task", "COURSE", &d, PRIORITY_HIGH);
    TEST_ASSERT(overflow_st == SAPM_ERR_QUEUE_FULL, "101st insertion returns SAPM_ERR_QUEUE_FULL");
    TEST_ASSERT(pq_size(&pq) == PQ_CAPACITY, "Queue size remains 100 after overflow attempt");

    /* Process one item, opening up space */
    Assignment processed;
    TEST_ASSERT(pq_process_highest(&pq, &processed) == SAPM_SUCCESS, "Process item from full queue");
    TEST_ASSERT(pq_size(&pq) == PQ_CAPACITY - 1, "Queue size is now 99");
    TEST_ASSERT(pq_is_full(&pq) == false, "pq_is_full reports false");

    /* Now insertion of 101st item should succeed */
    SAPM_Status retry_st = pq_add(&pq, 101, "Overflow Task", "COURSE", &d, PRIORITY_HIGH);
    TEST_ASSERT(retry_st == SAPM_SUCCESS, "Insertion succeeds after freeing slot");
    TEST_ASSERT(pq_size(&pq) == PQ_CAPACITY, "Queue size is back to 100");
    TEST_ASSERT(pq_is_full(&pq) == true, "pq_is_full reports true again");
}

/* ========================================================================= */
/* Test Group 11: Search by ID and Title                                     */
/* ========================================================================= */
static void test_search(void) {
    TEST_SECTION("Test Group 11: Search by ID and Title");

    PriorityQueue pq;
    pq_init(&pq);

    Date d1 = { 2026, 10, 20 };
    Date d2 = { 2026, 10, 25 };

    pq_add(&pq, 10, "Physics Lab Report", "PHYS101", &d1, PRIORITY_HIGH);
    pq_add(&pq, 20, "Chemistry Lab Notebook", "CHEM101", &d2, PRIORITY_MEDIUM);
    pq_add(&pq, 30, "Calculus Problem Set", "MATH201", &d1, PRIORITY_URGENT);

    /* Search by ID */
    Assignment res;
    TEST_ASSERT(pq_search_by_id(&pq, 20, &res) == SAPM_SUCCESS, "Search ID 20 succeeds");
    TEST_ASSERT(strcmp(res.title, "Chemistry Lab Notebook") == 0, "Found correct title for ID 20");
    TEST_ASSERT(res.priority == PRIORITY_MEDIUM, "Found correct priority for ID 20");

    TEST_ASSERT(pq_search_by_id(&pq, 999, &res) == SAPM_ERR_NOT_FOUND, "Search non-existent ID returns NOT_FOUND");
    TEST_ASSERT(pq_search_by_id(&pq, 0, &res) == SAPM_ERR_INVALID_ID, "Search ID 0 returns INVALID_ID");
    TEST_ASSERT(pq_search_by_id(&pq, -5, &res) == SAPM_ERR_INVALID_ID, "Search negative ID returns INVALID_ID");

    /* Search by Title (Exact & Case-Insensitive) */
    TEST_ASSERT(pq_search_by_title(&pq, "Calculus Problem Set", &res) == SAPM_SUCCESS, "Search exact title succeeds");
    TEST_ASSERT(res.id == 30, "Matched assignment ID 30");

    TEST_ASSERT(pq_search_by_title(&pq, "calculus problem set", &res) == SAPM_SUCCESS, "Search lowercase title succeeds");
    TEST_ASSERT(res.id == 30, "Matched assignment ID 30");

    /* Substring Search */
    TEST_ASSERT(pq_search_by_title(&pq, "Physics", &res) == SAPM_SUCCESS && res.id == 10, "Substring title 'Physics' matches");
    TEST_ASSERT(pq_search_by_title(&pq, "NonExistentTitle", &res) == SAPM_ERR_NOT_FOUND, "Non-existent title returns NOT_FOUND");
    TEST_ASSERT(pq_search_by_title(&pq, "", &res) == SAPM_ERR_INVALID_TITLE, "Empty search title returns INVALID_TITLE");

    /* Search All Matching Title */
    Assignment matches[5];
    size_t count = 0;
    SAPM_Status st = pq_search_by_title_all(&pq, "Lab", matches, 5, &count);
    TEST_ASSERT(st == SAPM_SUCCESS && count == 2, "Search all 'Lab' finds 2 matching assignments");
    TEST_ASSERT((matches[0].id == 10 && matches[1].id == 20) || (matches[0].id == 20 && matches[1].id == 10),
                "Matching IDs are 10 and 20");
}

/* ========================================================================= */
/* Test Group 12: Update Priority and Deadline                               */
/* ========================================================================= */
static void test_updates(void) {
    TEST_SECTION("Test Group 12: Update Priority & Deadline");

    PriorityQueue pq;
    pq_init(&pq);

    Date d_orig = { 2026, 11, 15 };
    Date d_new  = { 2026, 10, 10 };

    pq_add(&pq, 1, "Original Task 1", "ENG", &d_orig, PRIORITY_LOW);
    pq_add(&pq, 2, "Original Task 2", "MATH", &d_orig, PRIORITY_MEDIUM);

    /* Initially Task 2 has higher priority than Task 1 */
    Assignment top;
    pq_peek_highest(&pq, &top);
    TEST_ASSERT(top.id == 2, "Initially Task 2 is highest");

    /* Update Task 1 priority from LOW to URGENT */
    TEST_ASSERT(pq_update_priority(&pq, 1, PRIORITY_URGENT) == SAPM_SUCCESS, "Update priority succeeds");
    pq_peek_highest(&pq, &top);
    TEST_ASSERT(top.id == 1, "Task 1 is now highest after priority escalation to URGENT");

    /* Update Task 2 deadline to earlier date */
    TEST_ASSERT(pq_update_deadline(&pq, 2, &d_new) == SAPM_SUCCESS, "Update deadline succeeds");
    Assignment updated2;
    pq_search_by_id(&pq, 2, &updated2);
    TEST_ASSERT(updated2.deadline.day == 10 && updated2.deadline.month == 10, "Task 2 deadline updated");

    /* Update non-existent ID */
    TEST_ASSERT(pq_update_priority(&pq, 999, PRIORITY_HIGH) == SAPM_ERR_NOT_FOUND, "Update priority for missing ID returns NOT_FOUND");
    TEST_ASSERT(pq_update_deadline(&pq, 999, &d_new) == SAPM_ERR_NOT_FOUND, "Update deadline for missing ID returns NOT_FOUND");

    /* Update with invalid values */
    TEST_ASSERT(pq_update_priority(&pq, 1, (Priority)0) == SAPM_ERR_INVALID_PRIORITY, "Update with invalid priority rejected");
    TEST_ASSERT(pq_update_priority(&pq, 1, (Priority)99) == SAPM_ERR_INVALID_PRIORITY, "Update with priority 99 rejected");

    Date invalid_d = { 2023, 2, 29 };
    TEST_ASSERT(pq_update_deadline(&pq, 1, &invalid_d) == SAPM_ERR_INVALID_DATE, "Update with invalid leap day rejected");

    /* Atomic update of both */
    Date atomic_d = { 2026, 12, 1 };
    TEST_ASSERT(pq_update_priority_and_deadline(&pq, 1, PRIORITY_HIGH, &atomic_d) == SAPM_SUCCESS, "Atomic update succeeds");

    /* Atomic update fails safely if any parameter is invalid */
    TEST_ASSERT(pq_update_priority_and_deadline(&pq, 1, PRIORITY_URGENT, &invalid_d) == SAPM_ERR_INVALID_DATE, "Atomic update rejects invalid date");
    Assignment check;
    pq_search_by_id(&pq, 1, &check);
    TEST_ASSERT(check.priority == PRIORITY_HIGH, "Priority was NOT mutated after failed atomic update");
}

/* ========================================================================= */
/* Test Group 13: Delete a Pending Assignment                                */
/* ========================================================================= */
static void test_delete_assignment(void) {
    TEST_SECTION("Test Group 13: Delete a Pending Assignment");

    PriorityQueue pq;
    pq_init(&pq);

    Date d = { 2026, 10, 20 };
    pq_add(&pq, 1, "Task 1", "SUB", &d, PRIORITY_LOW);
    pq_add(&pq, 2, "Task 2", "SUB", &d, PRIORITY_MEDIUM);
    pq_add(&pq, 3, "Task 3", "SUB", &d, PRIORITY_HIGH);

    TEST_ASSERT(pq_size(&pq) == 3, "Size is 3");

    /* Delete middle element (ID 2) */
    Assignment deleted;
    TEST_ASSERT(pq_delete_by_id(&pq, 2, &deleted) == SAPM_SUCCESS, "Delete ID 2 succeeds");
    TEST_ASSERT(deleted.id == 2, "Deleted assignment has ID 2");
    TEST_ASSERT(pq_size(&pq) == 2, "Size is now 2");

    /* Verify ID 2 cannot be found */
    Assignment dummy;
    TEST_ASSERT(pq_search_by_id(&pq, 2, &dummy) == SAPM_ERR_NOT_FOUND, "Deleted ID 2 is no longer found");

    /* Delete missing ID */
    TEST_ASSERT(pq_delete_by_id(&pq, 999, &deleted) == SAPM_ERR_NOT_FOUND, "Delete missing ID returns NOT_FOUND");

    /* Delete remaining items */
    TEST_ASSERT(pq_delete_by_id(&pq, 1, NULL) == SAPM_SUCCESS, "Delete ID 1 with NULL out_deleted succeeds");
    TEST_ASSERT(pq_delete_by_id(&pq, 3, NULL) == SAPM_SUCCESS, "Delete ID 3 succeeds");
    TEST_ASSERT(pq_is_empty(&pq) == true, "Queue is empty after deleting all items");
}

/* ========================================================================= */
/* Test Group 14: Process Selected Assignment                                */
/* ========================================================================= */
static void test_process_assignment(void) {
    TEST_SECTION("Test Group 14: Process Selected Assignment (Highest & by ID)");

    PriorityQueue pq;
    pq_init(&pq);

    Date d = { 2026, 10, 20 };
    pq_add(&pq, 10, "Task 10", "SUB", &d, PRIORITY_LOW);
    pq_add(&pq, 20, "Task 20", "SUB", &d, PRIORITY_HIGH);
    pq_add(&pq, 30, "Task 30", "SUB", &d, PRIORITY_MEDIUM);

    /* Process by ID specifically */
    Assignment processed;
    TEST_ASSERT(pq_process_by_id(&pq, 30, &processed) == SAPM_SUCCESS, "Process specific ID 30 succeeds");
    TEST_ASSERT(processed.id == 30, "Processed ID is 30");
    TEST_ASSERT(processed.status == ASSIGNMENT_STATUS_COMPLETED, "Status is marked COMPLETED");
    TEST_ASSERT(pq_size(&pq) == 2, "Queue size reduced to 2");

    /* Process highest */
    TEST_ASSERT(pq_process_highest(&pq, &processed) == SAPM_SUCCESS, "Process highest succeeds");
    TEST_ASSERT(processed.id == 20, "Processed highest is ID 20 (HIGH priority)");
    TEST_ASSERT(processed.status == ASSIGNMENT_STATUS_COMPLETED, "Status is marked COMPLETED");
    TEST_ASSERT(pq_size(&pq) == 1, "Queue size reduced to 1");

    /* Process missing ID */
    TEST_ASSERT(pq_process_by_id(&pq, 999, &processed) == SAPM_ERR_NOT_FOUND, "Process missing ID returns NOT_FOUND");
}

/* ========================================================================= */
/* Test Group 15: Display and Sorted Snapshot Views                          */
/* ========================================================================= */
static void test_display_and_views(void) {
    TEST_SECTION("Test Group 15: Display & Sorted Snapshot Views");

    PriorityQueue pq;
    pq_init(&pq);

    Date d1 = { 2026, 10, 15 };
    Date d2 = { 2026, 10, 25 };

    pq_add(&pq, 1, "Low Due Early", "SUB", &d1, PRIORITY_LOW);
    pq_add(&pq, 2, "High Due Late", "SUB", &d2, PRIORITY_HIGH);
    pq_add(&pq, 3, "Medium Due Early", "SUB", &d1, PRIORITY_MEDIUM);

    /* Test display to /dev/null */
    FILE *null_stream = fopen("/dev/null", "w");
    if (null_stream != NULL) {
        TEST_ASSERT(pq_display(&pq, null_stream) == SAPM_SUCCESS, "pq_display to stream succeeds");
        fclose(null_stream);
    }

    /* Test sorted view without altering queue */
    Assignment sorted[5];
    size_t count = 0;
    TEST_ASSERT(pq_get_sorted_view(&pq, sorted, 5, &count) == SAPM_SUCCESS, "pq_get_sorted_view succeeds");
    TEST_ASSERT(count == 3, "Sorted view count is 3");
    TEST_ASSERT(sorted[0].id == 2, "First in sorted view is ID 2 (HIGH)");
    TEST_ASSERT(sorted[1].id == 3, "Second in sorted view is ID 3 (MEDIUM)");
    TEST_ASSERT(sorted[2].id == 1, "Third in sorted view is ID 1 (LOW)");

    /* Verify underlying queue remains in original unsorted array order */
    Assignment orig[5];
    pq_get_all(&pq, orig, 5, &count);
    TEST_ASSERT(orig[0].id == 1, "Underlying array position 0 is still ID 1 (unsorted)");
    TEST_ASSERT(orig[1].id == 2, "Underlying array position 1 is still ID 2 (unsorted)");
    TEST_ASSERT(orig[2].id == 3, "Underlying array position 2 is still ID 3 (unsorted)");
}

/* ========================================================================= */
/* Main Test Runner                                                          */
/* ========================================================================= */
int main(void) {
    printf("=================================================================\n");
    printf("  STUDENT ASSIGNMENT PRIORITY MANAGER - C ENGINE TEST SUITE       \n");
    printf("=================================================================\n");

    test_date_validation();
    test_priority_levels();
    test_assignment_validation();
    test_empty_queue();
    test_all_priority_levels();
    test_deadline_urgency();
    test_tie_breaking();
    test_priority_dominance();
    test_duplicate_id_rejection();
    test_capacity_overflow();
    test_search();
    test_updates();
    test_delete_assignment();
    test_process_assignment();
    test_display_and_views();

    printf("\n=================================================================\n");
    printf("TEST SUMMARY: Total: %d | Passed: %d | Failed: %d\n",
           tests_run, tests_passed, tests_failed);
    printf("=================================================================\n");

    return (tests_failed == 0) ? 0 : 1;
}
