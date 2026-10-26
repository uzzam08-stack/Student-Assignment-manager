# Student Assignment Priority Manager (SAPM) — Flask REST API

This document details the Flask REST API bridge to the high-performance C Priority Engine for the Student Assignment Priority Manager.

---

## 1. Architectural Design & C-Bridge Integration

The API wraps the existing C engine without reimplementing priority logic or queue data structures in Python:
- **Foreign Function Interface**: Python `ctypes` directly loads `c_engine/bin/libc_engine.dylib` (macOS) or `libc_engine.so` (Linux).
- **Exact Binary Layout**: The ctypes structures (`CDate`, `CAssignment`, `CPriorityQueue`, `CAssignmentHistory`) match the 64-bit C struct memory layout, alignment, and padding byte-for-byte.
- **Authoritative Comparator**: All priority decisions (`process-next`, priority-sorted view, peeking) are executed inside compiled C using `assignment_compare_priority`. The frontend/API never duplicates comparator logic.
- **Transactional In-Memory Rollback**: State-mutating operations (`POST`, `PATCH`, `DELETE`, `process-next`) snapshot in-memory C structures prior to mutation. If persistence fails (disk error, unwritable path), in-memory state is rolled back cleanly.
- **Separation of History**: Completed assignments are removed from the pending queue (capped at 100) and preserved in `CAssignmentHistory`.

---

## 2. API Endpoints Specification

### Unified Response Format
- **Success Responses**: Return standard HTTP status (`200 OK`, `201 Created`) with a JSON envelope:
  ```json
  {
    "status": "success",
    "data": { ... }
  }
  ```
- **Error Responses**: Return appropriate HTTP status (`400 Bad Request`, `404 Not Found`, `409 Conflict`, `500 Internal Server Error`) with a standardized JSON envelope:
  ```json
  {
    "status": "error",
    "code": "ERROR_CODE",
    "message": "Human readable error description",
    "details": ["Optional array of specific validation issues"]
  }
  ```

---

### Endpoint Reference

#### `GET /api/v1/health`
Checks server status, C engine library status, and current queue sizes.
- **Response `200 OK`**:
  ```json
  {
    "status": "healthy",
    "c_engine_loaded": true,
    "c_lib_path": "/path/to/c_engine/bin/libc_engine.dylib",
    "persistence_enabled": true,
    "data_filepath": "/path/to/assignments_data.csv",
    "pending_count": 3,
    "completed_count": 5
  }
  ```

---

#### `GET /api/v1/assignments`
Retrieves pending assignments.
- **Query Parameters**:
  - `sorted` (boolean, default `false`): If `true`, returns assignments in strict C-engine priority order (`URGENT` > `HIGH` > `MEDIUM` > `LOW`, with earlier deadlines and FIFO tie-breaking). If `false`, returns arrival/queue order.
  - `subject` (string, optional): Filters assignments by subject.
  - `priority` (string or int, optional): Filters by priority (e.g. `HIGH`, `3`).
  - `search` (string, optional): Filters assignments containing title substring.
- **Response `200 OK`**:
  ```json
  {
    "status": "success",
    "count": 1,
    "sorted_by_priority": true,
    "data": [
      {
        "id": 1,
        "title": "Algorithms Lab 1",
        "subject": "CS201",
        "deadline": "2026-10-25",
        "priority": "HIGH",
        "priority_level": 3,
        "sequence_num": 1,
        "status": "PENDING"
      }
    ]
  }
  ```

---

#### `POST /api/v1/assignments`
Creates and enqueues a new assignment.
- **Request Body**:
  ```json
  {
    "id": 10,
    "title": "Operating Systems Quiz",
    "subject": "CS301",
    "deadline": "2026-10-30",
    "priority": "HIGH"
  }
  ```
- **Validation Rules**:
  - `id`: Positive integer (`> 0`), unique across pending and history.
  - `title`: Non-empty string, maximum 127 characters.
  - `subject`: Non-empty string, maximum 63 characters.
  - `deadline`: Valid calendar date `YYYY-MM-DD` (evaluated against Gregorian leap year rules).
  - `priority`: `1` or `LOW`, `2` or `MEDIUM`, `3` or `HIGH`, `4` or `URGENT`.
- **Response `201 Created`**:
  ```json
  {
    "status": "success",
    "message": "Assignment created successfully",
    "data": { ... }
  }
  ```
- **Errors**:
  - `400 Bad Request` (`VALIDATION_ERROR` or `QUEUE_FULL` if 100 limit exceeded)
  - `409 Conflict` (`DUPLICATE_ID` if ID already exists)

---

#### `GET /api/v1/assignments/{id}`
Retrieves a specific pending assignment by ID.
- **Response `200 OK`**:
  ```json
  {
    "status": "success",
    "data": { ... }
  }
  ```
- **Response `404 Not Found`**:
  ```json
  {
    "status": "error",
    "code": "NOT_FOUND",
    "message": "Assignment with ID 10 not found in pending queue"
  }
  ```

---

#### `PATCH /api/v1/assignments/{id}`
Updates priority and/or deadline of a pending assignment.
- **Request Body**:
  ```json
  {
    "priority": "URGENT",
    "deadline": "2026-10-28"
  }
  ```
- **Response `200 OK`**:
  ```json
  {
    "status": "success",
    "message": "Assignment updated successfully",
    "data": { ... }
  }
  ```
- **Errors**: `400 Bad Request` (validation failure), `404 Not Found`.

---

#### `DELETE /api/v1/assignments/{id}`
Removes an assignment from the pending queue without completing it.
- **Response `200 OK`**:
  ```json
  {
    "status": "success",
    "message": "Assignment deleted successfully",
    "data": { ... }
  }
  ```
- **Errors**: `404 Not Found`.

---

#### `POST /api/v1/assignments/process-next`
Pops and completes the highest-priority assignment as evaluated strictly by the compiled C comparator (`assignment_compare_priority`), moving it into completion history.
- **Response `200 OK`**:
  ```json
  {
    "status": "success",
    "message": "Highest-priority assignment processed successfully",
    "data": {
      "id": 10,
      "title": "Operating Systems Quiz",
      "subject": "CS301",
      "deadline": "2026-10-28",
      "priority": "URGENT",
      "priority_level": 4,
      "sequence_num": 1,
      "status": "COMPLETED"
    }
  }
  ```
- **Response `400 Bad Request`** (when queue is empty):
  ```json
  {
    "status": "error",
    "code": "QUEUE_EMPTY",
    "message": "No pending assignments to process (queue is empty)"
  }
  ```

---

#### `GET /api/v1/history`
Retrieves all archived completed assignments.
- **Response `200 OK`**:
  ```json
  {
    "status": "success",
    "count": 1,
    "data": [
      {
        "id": 10,
        "title": "Operating Systems Quiz",
        "subject": "CS301",
        "deadline": "2026-10-28",
        "priority": "URGENT",
        "priority_level": 4,
        "sequence_num": 1,
        "status": "COMPLETED"
      }
    ]
  }
  ```

---

#### `GET /api/v1/statistics`
Retrieves workload analytics and dashboard metrics.
- **Response `200 OK`**:
  ```json
  {
    "status": "success",
    "data": {
      "total_pending": 3,
      "total_completed": 8,
      "capacity": 100,
      "capacity_available": 97,
      "capacity_used_percentage": 3.0,
      "priority_breakdown": {
        "LOW": 1,
        "MEDIUM": 0,
        "HIGH": 1,
        "URGENT": 1
      },
      "next_highest_assignment": {
        "id": 12,
        "title": "Midterm Exam",
        "priority": "URGENT"
      }
    }
  }
  ```

---

## 3. Setup and Execution Commands

### Prerequisites
- Clang or GCC compiler with standard C11 support.
- Python 3.10+ (with virtual environment).

### 1. Build the C Shared Library
```bash
make -C c_engine shared_lib
```
This builds `c_engine/bin/libc_engine.dylib` (macOS) or `libc_engine.so` (Linux).

### 2. Install Python Dependencies
```bash
.venv/bin/pip install -r backend/requirements.txt
```

### 3. Run Backend API Tests
```bash
.venv/bin/pytest backend/tests/test_api.py -v
```

### 4. Run C Core Engine & Storage Tests
```bash
make -C c_engine test
```

### 5. Launch the REST API Server
```bash
.venv/bin/python3 backend/app.py
```
By default, the server listens at `http://127.0.0.1:5000`.

---

## 4. Frontend Integration Guide

1. **Focus Queue / Next Task**:
   Call `POST /api/v1/assignments/process-next` to complete the top priority task. Never sort locally on the client.
2. **Dashboard Overview**:
   Call `GET /api/v1/assignments?sorted=true` to render assignments in true priority sequence.
3. **Workload Indicators**:
   Call `GET /api/v1/statistics` to retrieve capacity usage and priority distribution for progress bars or charts.
4. **History Log**:
   Call `GET /api/v1/history` to render the completed assignments feed.

---

## 5. Limitations & Caveats

- **Capacity**: Pending assignments are strictly bounded to 100 elements. Requests exceeding 100 return HTTP 400 (`QUEUE_FULL`).
- **History Capacity**: Completion history is bounded to 500 records.
- **Concurrency**: In-memory C queue access is synchronized with a reentrant lock (`threading.RLock`). While multiple threads can safely interact, multi-process deployments (e.g. multi-worker gunicorn) require single-worker mode or an external lock since the C memory is process-local.
