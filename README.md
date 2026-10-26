# Student Assignment Priority Manager (SAPM)

A robust, dual-mode academic task priority manager combining a high-performance **C11 Core Engine** with an optional **Python Flask REST API** bridged via foreign function interface (`ctypes`).

---

## 1. Architecture Overview

```
                          ┌───────────────────────────┐
                          │   Frontend / Client UI    │
                          │   (React / Vue / Mobile)  │
                          └─────────────┬─────────────┘
                                        │ HTTP / JSON
                                        ▼
                          ┌───────────────────────────┐
                          │      Flask REST API       │
                          │  (backend/app.py - Py3)   │
                          └─────────────┬─────────────┘
                                        │ ctypes FFI
                                        ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                            C CORE ENGINE                                    │
│                                                                             │
│   ┌──────────────────────────┐             ┌────────────────────────────┐   │
│   │  Unsorted Priority Queue │             │     Completion History     │   │
│   │   (Capacity 100 Array)   │             │       (Capacity 500)       │   │
│   └─────────────┬────────────┘             └─────────────┬──────────────┘   │
│                 │                                        │                  │
│                 └───────────────────┬────────────────────┘                  │
│                                     ▼                                       │
│                       ┌────────────────────────────┐                        │
│                       │  Authoritative Comparator  │                        │
│                       │ Priority > Date > Arrival  │                        │
│                       └─────────────┬──────────────┘                        │
│                                     ▼                                       │
│                       ┌────────────────────────────┐                        │
│                       │    Transactional Storage   │                        │
│                       │   (Atomic Rename CSV I/O)  │                        │
│                       └─────────────┬──────────────┘                        │
└─────────────────────────────────────┼───────────────────────────────────────┘
                                      ▼
                        ┌────────────────────────────┐
                        │    assignments_data.csv    │
                        └────────────────────────────┘
```

### Core Design Rules
- **No Heap**: Uses an array of structures with an unsorted array-based priority queue of capacity 100.
- **Single Authoritative Comparator**: Evaluates priority level (`URGENT` > `HIGH` > `MEDIUM` > `LOW`), deadline chronological proximity, and insertion order (`sequence_num`) for tie-breaking.
- **Atomic File Persistence**: Saves both pending and completed assignments via a staging temp file (`.tmp`) followed by POSIX atomic `rename()`. Never truncates or damages existing data files on failed writes.
- **Transactional Rollback**: When persistence fails during service mutations, in-memory C data structures roll back to previous snapshots.
- **Decoupled Console & API**: The data structures perform zero console I/O; clean status codes (`SAPM_Status`) are returned.

---

## 2. Directory Structure

```
.
├── API_CONTRACT.md                    # REST API specifications and schemas
├── FRONTEND_INTEGRATION.md            # Client integration guide and code snippets
├── README.md                          # Project documentation and instructions
├── pyproject.toml                     # Pytest configuration
├── assignments_data.csv               # Default persisted CSV store
├── backend/                           # Optional Python Flask REST API
│   ├── app.py                         # Application factory & error handlers
│   ├── config.py                      # Development, Testing, Production profiles
│   ├── requirements.txt               # Dependencies (Flask, Pytest)
│   ├── api/                           # Flask blueprints
│   │   ├── assignments.py             # CRUD and process-next routes
│   │   ├── history.py                 # Completion history route
│   │   └── statistics.py              # Workload analytics route
│   ├── services/
│   │   ├── assignment_service.py      # Thread-safe service with transactional rollback
│   │   ├── c_engine_bridge.py         # Ctypes ABI structure mappings and loader
│   │   └── validation.py              # Payload schema and C-calendar validation
│   └── tests/
│       └── test_api.py                # End-to-end integration test suite
└── c_engine/                          # Mandatory C11 Core Engine
    ├── Makefile                       # Strict build rules (-Wall -Wextra -Werror -pedantic)
    ├── include/
    │   ├── assignment.h               # Assignment struct, Priority enum, error codes
    │   ├── date_utils.h               # Gregorian leap year and calendar validation
    │   ├── history.h                  # Completed records archive structure
    │   ├── priority_queue.h           # Unsorted array priority queue (capacity 100)
    │   └── storage.h                  # Atomic CSV file persistence
    ├── src/
    │   ├── assignment.c               # Creation, validation, comparator, formatting
    │   ├── date_utils.c               # Leap year rules, date parsing and formatting
    │   ├── history.c                  # History buffer management
    │   ├── main.c                     # Standalone interactive CLI console app
    │   ├── priority_queue.c           # Unsorted array priority queue operations
    │   └── storage.c                  # RFC 4180 CSV serialization and staging loader
    └── tests/
        ├── test_engine.c              # Core DSA unit tests (15 test groups)
        └── test_storage.c             # Storage, persistence, and corruption tests
```

---

## 3. Quickstart & Build Instructions

### Prerequisites
- Clang or GCC compiler with C11 support.
- GNU Make 3.81+.
- Python 3.10+ (for Flask backend).

### 1. Compile C Engine & Shared Library
```bash
make -C c_engine all
```
*Compiles all native binaries and the shared library (`c_engine/bin/libc_engine.dylib` on macOS / `.so` on Linux).*

### 2. Run Native C Unit Tests
```bash
make -C c_engine test
```
*Executes both the Core Engine test runner and the Storage/Persistence test runner (382 assertions).*

### 3. Setup Python Virtual Environment
```bash
python3 -m venv .venv
.venv/bin/pip install -r backend/requirements.txt
```

### 4. Run Backend API Tests
```bash
.venv/bin/pytest backend/tests/test_api.py -v
```
*Executes all 16 Flask integration tests against the live C shared library.*

---

## 4. Running the Applications

### Option A: Interactive Native C Console Application
The C console application operates independently without requiring Python:
```bash
./c_engine/bin/sapm_cli
```
Provides an interactive terminal menu:
- Add, search, update, delete coursework
- Inspect highest-priority assignment
- Process next assignment (moves to completion history)
- View completed assignment history
- Save and auto-save on exit

### Option B: Flask REST API Server
```bash
.venv/bin/python3 backend/app.py
```
*Server starts at `http://127.0.0.1:5000`.*  
Verify health:
```bash
curl http://127.0.0.1:5000/api/v1/health
```

---

## 5. Verification Checklist & Known Limitations

### Verified Capabilities
- [x] Clean compilation under `-std=c11 -Wall -Wextra -Werror -pedantic -Wconversion -Wshadow -Wundef -Wstrict-prototypes`.
- [x] 100% test pass rate across native C and Flask API test suites (398 automated test assertions total).
- [x] AddressSanitizer and UndefinedBehaviorSanitizer verification clean (0 leaks, 0 memory errors).
- [x] Unsorted array-based priority queue strictly respects `PQ_CAPACITY = 100`.
- [x] Gregorian leap year calendar validation strictly enforced (rejects `2023-02-29`, `1900-02-29`; accepts `2024-02-29`, `2000-02-29`).
- [x] Authoritative comparator tie-breaking preserved across restarts via persistent sequence numbers.
- [x] Transactional rollback verified on persistence failure across all mutations.

### Known Limitations
1. **Concurrency Across Multiple Processes**:
   The C engine maintains in-memory structures synchronized via thread locks (`threading.RLock`). Running behind a multi-process WSGI server (e.g. `gunicorn -w 4`) requires single-worker mode (`-w 1`) or an external coordinator, because each worker process possesses its own distinct C memory space.
2. **Fixed Capacity Bounds**:
   Pending assignments are bounded to 100 elements, and completed history is bounded to 500 elements. Capacity expansion requires adjusting `PQ_CAPACITY` or `HISTORY_CAPACITY` and recompiling the C engine.
3. **Multi-line Text in CSV**:
   While RFC 4180 quotes with internal commas and quotes are supported, titles or subjects containing literal raw newline characters (`\n`) are truncated to single lines for terminal and parser clarity.
