# Student Assignment Priority Manager (SAPM) — API Contract

**Version:** 1.0.0  
**Base Path:** `/api/v1`  
**Protocol:** HTTP/1.1 REST  
**Data Format:** JSON (`application/json; charset=utf-8`)

---

## 1. Global Specifications

### Standard Envelope
Every response from the SAPM API uses a standardized JSON envelope.

#### Success Envelope
```json
{
  "status": "success",
  "data": {},
  "message": "Optional human-readable confirmation"
}
```

#### Error Envelope
```json
{
  "status": "error",
  "code": "ERROR_CODE_STRING",
  "message": "Human-readable description of what went wrong",
  "details": ["Optional list of specific field issues"]
}
```

### HTTP Status Codes
| HTTP Status | Meaning | Usage |
|---|---|---|
| `200 OK` | Request succeeded | `GET`, `PATCH`, `DELETE`, and `process-next` operations |
| `201 Created` | Resource created | `POST /api/v1/assignments` |
| `400 Bad Request` | Client error | Validation error, queue full, queue empty on process |
| `404 Not Found` | Resource not found | Targeted assignment ID not found |
| `409 Conflict` | Unique constraint violated | Assignment ID already exists |
| `500 Internal Error` | Server/Storage failure | C library load failure, disk persistence failure |

---

## 2. Domain Data Types & Validation Rules

| Field | Type | Required | Constraints |
|---|---|---|---|
| `id` | Integer | Yes (on Create) | Positive integer (`id > 0`), strictly unique across active queue and history. |
| `title` | String | Yes (on Create) | 1 to 127 characters. Leading/trailing whitespace trimmed. Cannot be only whitespace. |
| `subject` | String | Yes (on Create) | 1 to 63 characters. Leading/trailing whitespace trimmed. Cannot be only whitespace. |
| `deadline` | String | Yes (on Create) | Gregorian calendar date in `YYYY-MM-DD` format. Validated against leap year rules (e.g., `2024-02-29` is valid, `2023-02-29` is rejected). |
| `priority` | String / Integer | Yes (on Create) | Allowed strings: `"LOW"`, `"MEDIUM"`, `"HIGH"`, `"URGENT"` (case-insensitive). Allowed integers: `1` (LOW), `2` (MEDIUM), `3` (HIGH), `4` (URGENT). |
| `sequence_num` | Integer (uint64) | System-generated | Monotonically increasing arrival counter used for FIFO tie-breaking of identical priorities and deadlines. |
| `status` | String | System-generated | `"PENDING"` for queue items, `"COMPLETED"` for archived items. |

---

## 3. Endpoints

### 3.1 Health Check
**Endpoint:** `GET /api/v1/health`  
**Description:** Reports system health, C engine shared library connection status, persistence state, and counts.

#### Response `200 OK`
```json
{
  "status": "healthy",
  "c_engine_loaded": true,
  "c_lib_path": "/path/to/libc_engine.dylib",
  "persistence_enabled": true,
  "data_filepath": "assignments_data.csv",
  "pending_count": 5,
  "completed_count": 12
}
```

---

### 3.2 List Pending Assignments
**Endpoint:** `GET /api/v1/assignments`  
**Description:** Retrieves pending assignments from the unsorted priority queue.

#### Query Parameters
- `sorted` (boolean, optional, default: `false`): If `true`, returns assignments in strict C-engine priority order (`URGENT` > `HIGH` > `MEDIUM` > `LOW`, with earlier deadlines taking precedence, and creation arrival order breaking ties). If `false`, returns assignments in array storage/arrival order.
- `subject` (string, optional): Filters assignments matching subject (case-insensitive).
- `priority` (string or integer, optional): Filters assignments by priority level (e.g. `HIGH` or `3`).
- `search` (string, optional): Case-insensitive substring search on assignment title.

#### Response `200 OK`
```json
{
  "status": "success",
  "count": 2,
  "sorted_by_priority": true,
  "data": [
    {
      "id": 101,
      "title": "Machine Learning Term Paper",
      "subject": "CS480",
      "deadline": "2026-10-25",
      "priority": "URGENT",
      "priority_level": 4,
      "sequence_num": 3,
      "status": "PENDING"
    },
    {
      "id": 102,
      "title": "Linear Algebra Homework 4",
      "subject": "MATH210",
      "deadline": "2026-10-28",
      "priority": "HIGH",
      "priority_level": 3,
      "sequence_num": 1,
      "status": "PENDING"
    }
  ]
}
```

---

### 3.3 Create Assignment
**Endpoint:** `POST /api/v1/assignments`  
**Description:** Validates and enqueues a new assignment into the C priority queue. Capacity cannot exceed 100 pending assignments.

#### Request Headers
`Content-Type: application/json`

#### Request Body
```json
{
  "id": 105,
  "title": "Database Schema Design",
  "subject": "CS330",
  "deadline": "2026-10-30",
  "priority": "HIGH"
}
```

#### Response `201 Created`
```json
{
  "status": "success",
  "message": "Assignment created successfully",
  "data": {
    "id": 105,
    "title": "Database Schema Design",
    "subject": "CS330",
    "deadline": "2026-10-30",
    "priority": "HIGH",
    "priority_level": 3,
    "sequence_num": 4,
    "status": "PENDING"
  }
}
```

#### Error Responses
- `400 Bad Request` (`VALIDATION_ERROR`): Missing or invalid fields, invalid date/leap year.
- `400 Bad Request` (`QUEUE_FULL`): Queue capacity (100) exceeded.
- `409 Conflict` (`DUPLICATE_ID`): An assignment with this ID already exists.

---

### 3.4 Get Assignment by ID
**Endpoint:** `GET /api/v1/assignments/{id}`  
**Description:** Retrieves a specific pending assignment by its unique positive ID.

#### Response `200 OK`
```json
{
  "status": "success",
  "data": {
    "id": 105,
    "title": "Database Schema Design",
    "subject": "CS330",
    "deadline": "2026-10-30",
    "priority": "HIGH",
    "priority_level": 3,
    "sequence_num": 4,
    "status": "PENDING"
  }
}
```

#### Error Responses
- `404 Not Found` (`NOT_FOUND`): Assignment not present in the pending queue.

---

### 3.5 Update Assignment
**Endpoint:** `PATCH /api/v1/assignments/{id}`  
**Description:** Updates priority, deadline, or both for an existing pending assignment. State update is transactional.

#### Request Body (Partial Update Supported)
```json
{
  "priority": "URGENT",
  "deadline": "2026-10-27"
}
```

#### Response `200 OK`
```json
{
  "status": "success",
  "message": "Assignment updated successfully",
  "data": {
    "id": 105,
    "title": "Database Schema Design",
    "subject": "CS330",
    "deadline": "2026-10-27",
    "priority": "URGENT",
    "priority_level": 4,
    "sequence_num": 4,
    "status": "PENDING"
  }
}
```

#### Error Responses
- `400 Bad Request` (`VALIDATION_ERROR`): Invalid priority or invalid date.
- `404 Not Found` (`NOT_FOUND`): Assignment ID does not exist.

---

### 3.6 Delete Assignment
**Endpoint:** `DELETE /api/v1/assignments/{id}`  
**Description:** Removes a pending assignment without completing it. Transactional in-memory and disk update.

#### Response `200 OK`
```json
{
  "status": "success",
  "message": "Assignment deleted successfully",
  "data": {
    "id": 105,
    "title": "Database Schema Design",
    "subject": "CS330",
    "deadline": "2026-10-27",
    "priority": "URGENT",
    "priority_level": 4,
    "sequence_num": 4,
    "status": "PENDING"
  }
}
```

#### Error Responses
- `404 Not Found` (`NOT_FOUND`): Assignment ID does not exist in pending queue.

---

### 3.7 Process Next Assignment
**Endpoint:** `POST /api/v1/assignments/process-next`  
**Description:** **The compiled C engine selects the highest-priority assignment** using the authoritative comparator. The assignment is popped from the pending queue, status set to `COMPLETED`, and stored in completion history.

#### Request Body
None.

#### Response `200 OK`
```json
{
  "status": "success",
  "message": "Highest-priority assignment processed successfully",
  "data": {
    "id": 105,
    "title": "Database Schema Design",
    "subject": "CS330",
    "deadline": "2026-10-27",
    "priority": "URGENT",
    "priority_level": 4,
    "sequence_num": 4,
    "status": "COMPLETED"
  }
}
```

#### Error Responses
- `400 Bad Request` (`QUEUE_EMPTY`): Queue contains no pending assignments.

---

### 3.8 Get Completion History
**Endpoint:** `GET /api/v1/history`  
**Description:** Retrieves all archived completed assignment records.

#### Response `200 OK`
```json
{
  "status": "success",
  "count": 1,
  "data": [
    {
      "id": 105,
      "title": "Database Schema Design",
      "subject": "CS330",
      "deadline": "2026-10-27",
      "priority": "URGENT",
      "priority_level": 4,
      "sequence_num": 4,
      "status": "COMPLETED"
    }
  ]
}
```

---

### 3.9 Workload Statistics
**Endpoint:** `GET /api/v1/statistics`  
**Description:** Computes workload metrics, capacity utilization, and peeks at the top candidate without modifying the queue.

#### Response `200 OK`
```json
{
  "status": "success",
  "data": {
    "total_pending": 2,
    "total_completed": 1,
    "capacity": 100,
    "capacity_available": 98,
    "capacity_used_percentage": 2.0,
    "priority_breakdown": {
      "LOW": 0,
      "MEDIUM": 0,
      "HIGH": 1,
      "URGENT": 1
    },
    "next_highest_assignment": {
      "id": 101,
      "title": "Machine Learning Term Paper",
      "subject": "CS480",
      "deadline": "2026-10-25",
      "priority": "URGENT",
      "priority_level": 4,
      "sequence_num": 3,
      "status": "PENDING"
    }
  }
}
```
