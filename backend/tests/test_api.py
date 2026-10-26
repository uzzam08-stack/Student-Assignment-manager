import os
import sys
import shutil
import tempfile
from pathlib import Path
import pytest

BASE_DIR = Path(__file__).resolve().parent.parent.parent
if str(BASE_DIR) not in sys.path:
    sys.path.insert(0, str(BASE_DIR))

from backend.app import create_app
from backend.config import TestingConfig, DEFAULT_C_LIB_PATH
from backend.services.c_engine_bridge import CEngineBridge


@pytest.fixture
def test_env():
    """Sets up an isolated temporary data file and Flask test client."""
    temp_dir = tempfile.mkdtemp(prefix="sapm_api_test_")
    data_file = os.path.join(temp_dir, "test_assignments.csv")

    class CustomTestConfig(TestingConfig):
        DATA_FILEPATH = data_file
        C_LIB_PATH = DEFAULT_C_LIB_PATH
        PERSISTENCE_ENABLED = True

    app = create_app(CustomTestConfig)
    client = app.test_client()

    yield {
        "client": client,
        "app": app,
        "data_file": data_file,
        "config": CustomTestConfig,
    }

    # Cleanup temporary directory
    shutil.rmtree(temp_dir, ignore_errors=True)


# =========================================================================
# 1. Health Endpoint Tests
# =========================================================================
def test_health_check(test_env):
    client = test_env["client"]
    resp = client.get("/api/v1/health")
    assert resp.status_code == 200
    data = resp.get_json()
    assert data["status"] == "healthy"
    assert data["c_engine_loaded"] is True
    assert data["pending_count"] == 0
    assert data["completed_count"] == 0


# =========================================================================
# 2. Assignment Creation & Input Validation Tests
# =========================================================================
def test_create_assignment_success(test_env):
    client = test_env["client"]
    payload = {
        "id": 1,
        "title": "Algorithms Homework 1",
        "subject": "CS201",
        "deadline": "2026-10-25",
        "priority": "HIGH",
    }
    resp = client.post("/api/v1/assignments", json=payload)
    assert resp.status_code == 201
    data = resp.get_json()
    assert data["status"] == "success"
    assert data["data"]["id"] == 1
    assert data["data"]["title"] == "Algorithms Homework 1"
    assert data["data"]["subject"] == "CS201"
    assert data["data"]["deadline"] == "2026-10-25"
    assert data["data"]["priority"] == "HIGH"
    assert data["data"]["status"] == "PENDING"


def test_create_assignment_validation_failures(test_env):
    client = test_env["client"]

    # Missing ID
    resp = client.post("/api/v1/assignments", json={
        "title": "Task", "subject": "CS", "deadline": "2026-10-25", "priority": "HIGH"
    })
    assert resp.status_code == 400
    assert resp.get_json()["code"] == "VALIDATION_ERROR"

    # Negative ID
    resp = client.post("/api/v1/assignments", json={
        "id": -5, "title": "Task", "subject": "CS", "deadline": "2026-10-25", "priority": "HIGH"
    })
    assert resp.status_code == 400

    # Empty Title
    resp = client.post("/api/v1/assignments", json={
        "id": 2, "title": "   ", "subject": "CS", "deadline": "2026-10-25", "priority": "HIGH"
    })
    assert resp.status_code == 400

    # Invalid Calendar Date: 2023-02-29 (not a leap year!)
    resp = client.post("/api/v1/assignments", json={
        "id": 3, "title": "Task", "subject": "CS", "deadline": "2023-02-29", "priority": "HIGH"
    })
    assert resp.status_code == 400
    assert "deadline" in str(resp.get_json()["details"]).lower()

    # Valid Leap Year Date: 2024-02-29
    resp = client.post("/api/v1/assignments", json={
        "id": 4, "title": "Leap Task", "subject": "CS", "deadline": "2024-02-29", "priority": "HIGH"
    })
    assert resp.status_code == 201

    # Invalid Priority Level
    resp = client.post("/api/v1/assignments", json={
        "id": 5, "title": "Task", "subject": "CS", "deadline": "2026-10-25", "priority": "SUPER_URGENT"
    })
    assert resp.status_code == 400


def test_duplicate_id_rejection(test_env):
    client = test_env["client"]
    payload = {
        "id": 10,
        "title": "Original Task",
        "subject": "CS101",
        "deadline": "2026-10-25",
        "priority": "LOW",
    }
    resp1 = client.post("/api/v1/assignments", json=payload)
    assert resp1.status_code == 201

    # Duplicate ID 10
    resp2 = client.post("/api/v1/assignments", json={
        "id": 10,
        "title": "Different Task",
        "subject": "MATH",
        "deadline": "2026-11-01",
        "priority": "HIGH",
    })
    assert resp2.status_code == 409
    assert resp2.get_json()["code"] == "DUPLICATE_ID"


# =========================================================================
# 3. Retrieve, Search & Query Tests
# =========================================================================
def test_get_assignment_by_id(test_env):
    client = test_env["client"]
    client.post("/api/v1/assignments", json={
        "id": 42, "title": "Secret Task", "subject": "CS42", "deadline": "2026-10-20", "priority": "URGENT"
    })

    # Found
    resp = client.get("/api/v1/assignments/42")
    assert resp.status_code == 200
    assert resp.get_json()["data"]["id"] == 42
    assert resp.get_json()["data"]["priority"] == "URGENT"

    # Not found
    resp_missing = client.get("/api/v1/assignments/999")
    assert resp_missing.status_code == 404
    assert resp_missing.get_json()["code"] == "NOT_FOUND"


def test_list_assignments_filtering_and_sorting(test_env):
    client = test_env["client"]
    client.post("/api/v1/assignments", json={
        "id": 1, "title": "Physics Lab 1", "subject": "PHYS", "deadline": "2026-10-15", "priority": "LOW"
    })
    client.post("/api/v1/assignments", json={
        "id": 2, "title": "Calculus Midterm", "subject": "MATH", "deadline": "2026-10-20", "priority": "HIGH"
    })
    client.post("/api/v1/assignments", json={
        "id": 3, "title": "Chemistry Lab 2", "subject": "CHEM", "deadline": "2026-10-25", "priority": "URGENT"
    })

    # Subject filter
    resp = client.get("/api/v1/assignments?subject=MATH")
    assert resp.status_code == 200
    data = resp.get_json()["data"]
    assert len(data) == 1
    assert data[0]["id"] == 2

    # Search filter
    resp_search = client.get("/api/v1/assignments?search=Lab")
    assert resp_search.status_code == 200
    data_search = resp_search.get_json()["data"]
    assert len(data_search) == 2

    # Sorted by C engine priority
    resp_sorted = client.get("/api/v1/assignments?sorted=true")
    assert resp_sorted.status_code == 200
    sorted_items = resp_sorted.get_json()["data"]
    assert sorted_items[0]["id"] == 3  # URGENT
    assert sorted_items[1]["id"] == 2  # HIGH
    assert sorted_items[2]["id"] == 1  # LOW


# =========================================================================
# 4. Update & Delete Tests
# =========================================================================
def test_update_assignment(test_env):
    client = test_env["client"]
    client.post("/api/v1/assignments", json={
        "id": 15, "title": "Update Me", "subject": "ENG", "deadline": "2026-11-01", "priority": "LOW"
    })

    # Update priority to URGENT
    resp = client.patch("/api/v1/assignments/15", json={"priority": "URGENT"})
    assert resp.status_code == 200
    assert resp.get_json()["data"]["priority"] == "URGENT"

    # Update deadline
    resp_d = client.patch("/api/v1/assignments/15", json={"deadline": "2026-10-10"})
    assert resp_d.status_code == 200
    assert resp_d.get_json()["data"]["deadline"] == "2026-10-10"

    # Update non-existent ID
    resp_none = client.patch("/api/v1/assignments/999", json={"priority": "HIGH"})
    assert resp_none.status_code == 404


def test_delete_assignment(test_env):
    client = test_env["client"]
    client.post("/api/v1/assignments", json={
        "id": 99, "title": "Delete Me", "subject": "TEST", "deadline": "2026-10-20", "priority": "MEDIUM"
    })

    # Delete
    resp = client.delete("/api/v1/assignments/99")
    assert resp.status_code == 200
    assert resp.get_json()["data"]["id"] == 99

    # Verify deleted
    resp_check = client.get("/api/v1/assignments/99")
    assert resp_check.status_code == 404


# =========================================================================
# 5. C-Engine Priority Processing & Completion History Tests
# =========================================================================
def test_c_engine_decides_process_next(test_env):
    client = test_env["client"]

    # 1. LOW priority, earlier deadline
    client.post("/api/v1/assignments", json={
        "id": 1, "title": "Low Task", "subject": "SUB", "deadline": "2026-10-01", "priority": "LOW"
    })
    # 2. HIGH priority, later deadline
    client.post("/api/v1/assignments", json={
        "id": 2, "title": "High Task Later", "subject": "SUB", "deadline": "2026-10-25", "priority": "HIGH"
    })
    # 3. HIGH priority, earlier deadline
    client.post("/api/v1/assignments", json={
        "id": 3, "title": "High Task Earlier", "subject": "SUB", "deadline": "2026-10-15", "priority": "HIGH"
    })
    # 4. HIGH priority, identical deadline to #3 (inserted later -> tie-breaker test)
    client.post("/api/v1/assignments", json={
        "id": 4, "title": "High Task Tie", "subject": "SUB", "deadline": "2026-10-15", "priority": "HIGH"
    })
    # 5. URGENT priority (should dominate everything)
    client.post("/api/v1/assignments", json={
        "id": 5, "title": "Urgent Task", "subject": "SUB", "deadline": "2026-11-01", "priority": "URGENT"
    })

    # Sequence of process-next must strictly follow C engine comparator rules:
    # 1. ID 5 (URGENT)
    p1 = client.post("/api/v1/assignments/process-next").get_json()["data"]
    assert p1["id"] == 5

    # 2. ID 3 (HIGH, earlier deadline 2026-10-15, arrived before ID 4)
    p2 = client.post("/api/v1/assignments/process-next").get_json()["data"]
    assert p2["id"] == 3

    # 3. ID 4 (HIGH, same deadline 2026-10-15, arrived after ID 3)
    p3 = client.post("/api/v1/assignments/process-next").get_json()["data"]
    assert p3["id"] == 4

    # 4. ID 2 (HIGH, later deadline 2026-10-25)
    p4 = client.post("/api/v1/assignments/process-next").get_json()["data"]
    assert p4["id"] == 2

    # 5. ID 1 (LOW)
    p5 = client.post("/api/v1/assignments/process-next").get_json()["data"]
    assert p5["id"] == 1

    # 6. Now queue is empty
    empty_resp = client.post("/api/v1/assignments/process-next")
    assert empty_resp.status_code == 400
    assert empty_resp.get_json()["code"] == "QUEUE_EMPTY"

    # Verify all 5 are in completion history!
    hist_resp = client.get("/api/v1/history")
    assert hist_resp.status_code == 200
    hist_data = hist_resp.get_json()
    assert hist_data["count"] == 5
    hist_ids = [item["id"] for item in hist_data["data"]]
    assert hist_ids == [5, 3, 4, 2, 1]
    assert all(item["status"] == "COMPLETED" for item in hist_data["data"])


# =========================================================================
# 6. Statistics Endpoint Tests
# =========================================================================
def test_statistics_endpoint(test_env):
    client = test_env["client"]
    client.post("/api/v1/assignments", json={
        "id": 1, "title": "T1", "subject": "S", "deadline": "2026-10-20", "priority": "HIGH"
    })
    client.post("/api/v1/assignments", json={
        "id": 2, "title": "T2", "subject": "S", "deadline": "2026-10-20", "priority": "LOW"
    })

    resp = client.get("/api/v1/statistics")
    assert resp.status_code == 200
    stats = resp.get_json()["data"]
    assert stats["total_pending"] == 2
    assert stats["total_completed"] == 0
    assert stats["capacity"] == 100
    assert stats["capacity_available"] == 98
    assert stats["capacity_used_percentage"] == 2.0
    assert stats["priority_breakdown"]["HIGH"] == 1
    assert stats["priority_breakdown"]["LOW"] == 1
    assert stats["next_highest_assignment"]["id"] == 1


# =========================================================================
# 7. Capacity Limit (100 Pending Items)
# =========================================================================
def test_capacity_overflow_rejection(test_env):
    client = test_env["client"]
    # Fill to capacity
    for i in range(1, 101):
        resp = client.post("/api/v1/assignments", json={
            "id": i, "title": f"Task {i}", "subject": "S", "deadline": "2026-10-20", "priority": "LOW"
        })
        assert resp.status_code == 201

    # 101st item
    overflow_resp = client.post("/api/v1/assignments", json={
        "id": 101, "title": "Overflow Task", "subject": "S", "deadline": "2026-10-20", "priority": "HIGH"
    })
    assert overflow_resp.status_code == 400
    assert overflow_resp.get_json()["code"] == "QUEUE_FULL"


# =========================================================================
# 8. Transactional Rollback on Save Failure
# =========================================================================
def test_transactional_rollback_on_save_failure(test_env):
    client = test_env["client"]
    service = test_env["app"].extensions["assignment_service"]

    # Initial valid item
    client.post("/api/v1/assignments", json={
        "id": 1, "title": "Initial Task", "subject": "CS", "deadline": "2026-10-20", "priority": "LOW"
    })

    # Point data_filepath to an invalid unwritable path to trigger persistence failure
    original_path = service.data_filepath
    service.data_filepath = "/non_existent_directory_99999/data.csv"

    # Attempt to add assignment #2 -> should fail and rollback in-memory state
    resp = client.post("/api/v1/assignments", json={
        "id": 2, "title": "Failing Task", "subject": "CS", "deadline": "2026-10-20", "priority": "HIGH"
    })
    assert resp.status_code == 500
    assert resp.get_json()["code"] == "STORAGE_ERROR"

    # In-memory queue must NOT have assignment #2!
    service.data_filepath = original_path  # restore valid path
    check_resp = client.get("/api/v1/assignments/2")
    assert check_resp.status_code == 404

    # Queue size should still be 1 (only ID 1)
    list_resp = client.get("/api/v1/assignments")
    assert list_resp.get_json()["count"] == 1


# =========================================================================
# 9. Restart Persistence Across App Instances
# =========================================================================
def test_restart_persistence(test_env):
    client = test_env["client"]
    config = test_env["config"]

    # Add 2 items, complete 1
    client.post("/api/v1/assignments", json={
        "id": 100, "title": "Persist Pending", "subject": "CS", "deadline": "2026-10-20", "priority": "HIGH"
    })
    client.post("/api/v1/assignments", json={
        "id": 101, "title": "Persist Completed", "subject": "MATH", "deadline": "2026-10-25", "priority": "URGENT"
    })
    client.post("/api/v1/assignments/process-next")  # ID 101 processed into history

    # Instantiate brand new app instance using same data file
    new_app = create_app(config)
    new_client = new_app.test_client()

    # Verify pending
    pending_resp = new_client.get("/api/v1/assignments")
    assert pending_resp.status_code == 200
    assert pending_resp.get_json()["count"] == 1
    assert pending_resp.get_json()["data"][0]["id"] == 100

    # Verify history
    history_resp = new_client.get("/api/v1/history")
    assert history_resp.status_code == 200
    assert history_resp.get_json()["count"] == 1
    assert history_resp.get_json()["data"][0]["id"] == 101


# =========================================================================
# 10. Update Immediately Affects Next Selected Assignment (Requirement 9)
# =========================================================================
def test_update_immediately_affects_next_selected(test_env):
    client = test_env["client"]

    client.post("/api/v1/assignments", json={
        "id": 1, "title": "Initially Low", "subject": "CS", "deadline": "2026-10-20", "priority": "LOW"
    })
    client.post("/api/v1/assignments", json={
        "id": 2, "title": "Initially Medium", "subject": "CS", "deadline": "2026-10-20", "priority": "MEDIUM"
    })

    # Prior to update, Task 2 is the next highest priority
    stats1 = client.get("/api/v1/statistics").get_json()["data"]
    assert stats1["next_highest_assignment"]["id"] == 2

    # Escalate Task 1 priority to URGENT
    update_resp = client.patch("/api/v1/assignments/1", json={"priority": "URGENT"})
    assert update_resp.status_code == 200

    # Immediately, Task 1 must be the next highest priority
    stats2 = client.get("/api/v1/statistics").get_json()["data"]
    assert stats2["next_highest_assignment"]["id"] == 1

    # Processing next must immediately pop Task 1
    proc_resp = client.post("/api/v1/assignments/process-next")
    assert proc_resp.status_code == 200
    assert proc_resp.get_json()["data"]["id"] == 1


# =========================================================================
# 11. Missing Records & Repeated Completion Attempts (Requirement 11)
# =========================================================================
def test_repeated_completion_and_missing_records(test_env):
    client = test_env["client"]

    # 1. Repeated process-next on empty queue
    for _ in range(3):
        resp = client.post("/api/v1/assignments/process-next")
        assert resp.status_code == 400
        assert resp.get_json()["code"] == "QUEUE_EMPTY"

    # 2. Add and process a single item
    client.post("/api/v1/assignments", json={
        "id": 10, "title": "Solo Task", "subject": "CS", "deadline": "2026-10-20", "priority": "HIGH"
    })
    proc_resp = client.post("/api/v1/assignments/process-next")
    assert proc_resp.status_code == 200
    assert proc_resp.get_json()["data"]["id"] == 10

    # 3. Immediately repeating process-next must fail cleanly
    proc_again = client.post("/api/v1/assignments/process-next")
    assert proc_again.status_code == 400
    assert proc_again.get_json()["code"] == "QUEUE_EMPTY"

    # 4. Repeated deletion of same ID
    client.post("/api/v1/assignments", json={
        "id": 20, "title": "Delete Twice", "subject": "CS", "deadline": "2026-10-20", "priority": "LOW"
    })
    del1 = client.delete("/api/v1/assignments/20")
    assert del1.status_code == 200
    del2 = client.delete("/api/v1/assignments/20")
    assert del2.status_code == 404
    assert del2.get_json()["code"] == "NOT_FOUND"


# =========================================================================
# 12. Transactional Rollback on Process and Delete (Requirement 13)
# =========================================================================
def test_transactional_rollback_on_all_mutations(test_env):
    client = test_env["client"]
    service = test_env["app"].extensions["assignment_service"]

    # Seed 2 items
    client.post("/api/v1/assignments", json={
        "id": 1, "title": "Task 1", "subject": "CS", "deadline": "2026-10-20", "priority": "HIGH"
    })
    client.post("/api/v1/assignments", json={
        "id": 2, "title": "Task 2", "subject": "CS", "deadline": "2026-10-20", "priority": "LOW"
    })

    original_path = service.data_filepath

    # 1. Test Delete rollback when persistence fails
    service.data_filepath = "/non_existent_folder_xyz/data.csv"
    del_resp = client.delete("/api/v1/assignments/1")
    assert del_resp.status_code == 500
    assert del_resp.get_json()["code"] == "STORAGE_ERROR"

    # Verify ID 1 was NOT deleted (in-memory state preserved)
    service.data_filepath = original_path
    check_del = client.get("/api/v1/assignments/1")
    assert check_del.status_code == 200

    # 2. Test Process-Next rollback when persistence fails
    service.data_filepath = "/non_existent_folder_xyz/data.csv"
    proc_resp = client.post("/api/v1/assignments/process-next")
    assert proc_resp.status_code == 500
    assert proc_resp.get_json()["code"] == "STORAGE_ERROR"

    # Verify ID 1 was NOT dequeued and NOT moved to history
    service.data_filepath = original_path
    check_proc = client.get("/api/v1/assignments/1")
    assert check_proc.status_code == 200
    hist_resp = client.get("/api/v1/history")
    assert hist_resp.get_json()["count"] == 0


def test_frontend_index_and_process_by_id(test_env):
    client = test_env["client"]

    # 1. Test GET / serves index.html
    root_resp = client.get("/")
    assert root_resp.status_code == 200
    assert b"Student Assignment Priority Manager" in root_resp.data

    # 2. Add two assignments
    client.post("/api/v1/assignments", json={
        "id": 10, "title": "Low Priority Task", "subject": "CS", "deadline": "2026-11-01", "priority": "LOW"
    })
    client.post("/api/v1/assignments", json={
        "id": 20, "title": "High Priority Task", "subject": "Math", "deadline": "2026-10-30", "priority": "HIGH"
    })

    # 3. Process specific assignment by ID (process ID 10)
    proc_resp = client.post("/api/v1/assignments/10/process")
    assert proc_resp.status_code == 200
    assert proc_resp.get_json()["data"]["id"] == 10
    assert proc_resp.get_json()["data"]["status"] == "COMPLETED"

    # ID 10 should no longer be pending
    check_resp = client.get("/api/v1/assignments/10")
    assert check_resp.status_code == 404

    # ID 20 should still be pending
    check_20 = client.get("/api/v1/assignments/20")
    assert check_20.status_code == 200

    # History should contain ID 10
    hist_resp = client.get("/api/v1/history")
    assert hist_resp.status_code == 200
    assert hist_resp.get_json()["count"] == 1
    assert hist_resp.get_json()["data"][0]["id"] == 10


