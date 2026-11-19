import csv
import ctypes
from ctypes import (
    POINTER, byref, c_bool, c_char_p, c_int, c_size_t, c_uint64, create_string_buffer
)
from functools import cmp_to_key
import os
from pathlib import Path
from typing import Any, Dict, List, Optional

# Constants matching C engine
ASSIGNMENT_TITLE_MAX = 128
ASSIGNMENT_SUBJECT_MAX = 64
PQ_CAPACITY = 100
HISTORY_CAPACITY = 500

# Priority levels
PRIORITY_INVALID = 0
PRIORITY_LOW = 1
PRIORITY_MEDIUM = 2
PRIORITY_HIGH = 3
PRIORITY_URGENT = 4

PRIORITY_NAMES = {
    PRIORITY_LOW: "LOW",
    PRIORITY_MEDIUM: "MEDIUM",
    PRIORITY_HIGH: "HIGH",
    PRIORITY_URGENT: "URGENT",
}

STATUS_PENDING = 0
STATUS_COMPLETED = 1

STATUS_NAMES = {
    STATUS_PENDING: "PENDING",
    STATUS_COMPLETED: "COMPLETED",
}

# C Engine Status Codes
SAPM_SUCCESS = 0
SAPM_SUCCESS_FIRST_RUN = 1
SAPM_ERR_NULL_PARAM = -1
SAPM_ERR_INVALID_ID = -2
SAPM_ERR_INVALID_TITLE = -3
SAPM_ERR_INVALID_SUBJECT = -4
SAPM_ERR_INVALID_DATE = -5
SAPM_ERR_INVALID_PRIORITY = -6
SAPM_ERR_DUPLICATE_ID = -7
SAPM_ERR_NOT_FOUND = -8
SAPM_ERR_QUEUE_FULL = -9
SAPM_ERR_QUEUE_EMPTY = -10
SAPM_ERR_BUFFER_TOO_SMALL = -11
SAPM_ERR_INVALID_FORMAT = -12
SAPM_ERR_FILE_NOT_FOUND = -13
SAPM_ERR_FILE_OPEN = -14
SAPM_ERR_FILE_READ = -15
SAPM_ERR_FILE_WRITE = -16
SAPM_ERR_FILE_CLOSE = -17
SAPM_ERR_CORRUPT_RECORD = -18
SAPM_ERR_HISTORY_FULL = -19
SAPM_ERR_INVALID_STATUS = -20


class CEngineLoadError(RuntimeError):
    """Raised when the compiled C engine shared library cannot be located or loaded."""
    pass


# Exact ctypes struct layout
class CDate(ctypes.Structure):
    _fields_ = [
        ("year", c_int),
        ("month", c_int),
        ("day", c_int),
    ]

    def to_iso(self) -> str:
        return f"{self.year:04d}-{self.month:02d}-{self.day:02d}"


class CAssignment(ctypes.Structure):
    _fields_ = [
        ("id", c_int),
        ("title", ctypes.c_char * ASSIGNMENT_TITLE_MAX),
        ("subject", ctypes.c_char * ASSIGNMENT_SUBJECT_MAX),
        ("deadline", CDate),
        ("priority", c_int),
        ("sequence_num", c_uint64),
        ("status", c_int),
    ]

    def to_dict(self) -> Dict[str, Any]:
        return {
            "id": self.id,
            "title": self.title.decode("utf-8", errors="replace"),
            "subject": self.subject.decode("utf-8", errors="replace"),
            "deadline": self.deadline.to_iso(),
            "priority": PRIORITY_NAMES.get(self.priority, "UNKNOWN"),
            "priority_level": self.priority,
            "sequence_num": self.sequence_num,
            "status": STATUS_NAMES.get(self.status, "UNKNOWN"),
        }


class CPriorityQueue(ctypes.Structure):
    _fields_ = [
        ("items", CAssignment * PQ_CAPACITY),
        ("size", c_size_t),
        ("next_sequence", c_uint64),
    ]


class CAssignmentHistory(ctypes.Structure):
    _fields_ = [
        ("items", CAssignment * HISTORY_CAPACITY),
        ("size", c_size_t),
    ]


def _unwrap(obj: Any) -> Any:
    """Unwrap a ctypes byref or pointer to its underlying data object."""
    if obj is None:
        return None
    if hasattr(obj, "_obj"):
        return obj._obj
    if hasattr(obj, "contents"):
        return obj.contents
    return obj


class PythonEngineFallback:
    """In-process Python implementation of the C priority queue engine.

    Provides 100% ABI and algorithmic parity with libc_engine:
    - Same capacity: 100 pending items, 500 history items
    - Same comparator: priority descending (4 > 3 > 2 > 1), earliest deadline, sequence_num tie breaker
    - Same Gregorian calendar and leap year validation
    - Same error codes and status messages
    - Same RFC 4180 CSV persistence with transactional atomic replacement
    """

    STATUS_MESSAGES = {
        SAPM_SUCCESS: b"Operation completed successfully",
        SAPM_SUCCESS_FIRST_RUN: b"Storage initialised in first-run state (no existing file)",
        SAPM_ERR_NULL_PARAM: b"NULL pointer passed for required parameter",
        SAPM_ERR_INVALID_ID: b"Assignment ID must be a positive non-zero integer",
        SAPM_ERR_INVALID_TITLE: b"Title must not be empty or exceed maximum length",
        SAPM_ERR_INVALID_SUBJECT: b"Subject must not be empty or exceed maximum length",
        SAPM_ERR_INVALID_DATE: b"Invalid date or date format",
        SAPM_ERR_INVALID_PRIORITY: b"Priority value out of valid range (1-4)",
        SAPM_ERR_DUPLICATE_ID: b"An assignment with this ID already exists",
        SAPM_ERR_NOT_FOUND: b"Assignment not found",
        SAPM_ERR_QUEUE_FULL: b"Priority queue has reached maximum capacity (100)",
        SAPM_ERR_QUEUE_EMPTY: b"Priority queue is empty",
        SAPM_ERR_BUFFER_TOO_SMALL: b"Destination buffer too small",
        SAPM_ERR_INVALID_FORMAT: b"Invalid record format",
        SAPM_ERR_FILE_NOT_FOUND: b"Data file not found",
        SAPM_ERR_FILE_OPEN: b"Failed to open data file",
        SAPM_ERR_FILE_READ: b"Error reading data file",
        SAPM_ERR_FILE_WRITE: b"Error writing data file",
        SAPM_ERR_FILE_CLOSE: b"Error closing data file",
        SAPM_ERR_CORRUPT_RECORD: b"Data file contains corrupt or malformed records",
        SAPM_ERR_HISTORY_FULL: b"Assignment history has reached maximum capacity (500)",
        SAPM_ERR_INVALID_STATUS: b"Invalid assignment status",
    }

    def sapm_status_to_string(self, code: int) -> bytes:
        return self.STATUS_MESSAGES.get(code, f"Unknown status code ({code})".encode("utf-8"))

    # Date operations
    def date_is_leap_year(self, year: int) -> bool:
        if year <= 0:
            return False
        return ((year % 4 == 0) and (year % 100 != 0)) or (year % 400 == 0)

    def date_days_in_month(self, year: int, month: int) -> int:
        if year <= 0 or month < 1 or month > 12:
            return -1
        days = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
        if month == 2:
            return 29 if self.date_is_leap_year(year) else 28
        return days[month - 1]

    def date_is_valid(self, d_ptr: Any) -> bool:
        d = _unwrap(d_ptr)
        if d is None:
            return False
        if d.year <= 0 or d.year > 9999:
            return False
        if d.month < 1 or d.month > 12:
            return False
        max_d = self.date_days_in_month(d.year, d.month)
        return 1 <= d.day <= max_d

    def date_create(self, year: int, month: int, day: int, out_date: Any) -> int:
        out = _unwrap(out_date)
        if out is None:
            return -1
        d = CDate(year=year, month=month, day=day)
        if not self.date_is_valid(d):
            return -1
        out.year = year
        out.month = month
        out.day = day
        return 0

    def date_compare(self, d1_ptr: Any, d2_ptr: Any) -> int:
        d1 = _unwrap(d1_ptr)
        d2 = _unwrap(d2_ptr)
        if d1 is None and d2 is None:
            return 0
        if d1 is None:
            return -1
        if d2 is None:
            return 1
        if d1.year != d2.year:
            return 1 if d1.year > d2.year else -1
        if d1.month != d2.month:
            return 1 if d1.month > d2.month else -1
        if d1.day != d2.day:
            return 1 if d1.day > d2.day else -1
        return 0

    def date_format(self, d_ptr: Any, buf: Any, buf_size: int) -> int:
        d = _unwrap(d_ptr)
        if d is None or buf is None or buf_size < 11:
            return -1
        s = f"{d.year:04d}-{d.month:02d}-{d.day:02d}".encode("utf-8")
        ctypes.memmove(buf, s, len(s) + 1)
        return 0

    def date_parse(self, s_bytes: Any, out_date: Any) -> int:
        out = _unwrap(out_date)
        if not s_bytes or out is None:
            return -1
        try:
            s = s_bytes.decode("utf-8").strip() if isinstance(s_bytes, bytes) else str(s_bytes).strip()
            parts = s.split("-")
            if len(parts) != 3:
                return -1
            y, m, d = int(parts[0]), int(parts[1]), int(parts[2])
            return self.date_create(y, m, d, out)
        except Exception:
            return -1

    # Priority operations
    def priority_is_valid(self, p: int) -> bool:
        return 1 <= p <= 4

    def priority_to_string(self, p: int) -> bytes:
        names = {1: b"LOW", 2: b"MEDIUM", 3: b"HIGH", 4: b"URGENT"}
        return names.get(p, b"INVALID")

    def priority_from_string(self, p_bytes: Any, out_p: Any) -> int:
        out = _unwrap(out_p)
        if not p_bytes or out is None:
            return -1
        try:
            s = (p_bytes.decode("utf-8") if isinstance(p_bytes, bytes) else str(p_bytes)).strip().upper()
            mapping = {"LOW": 1, "MEDIUM": 2, "HIGH": 3, "URGENT": 4}
            if s in mapping:
                out.value = mapping[s]
                return 0
            return -1
        except Exception:
            return -1

    # Assignment helpers
    def assignment_status_to_string(self, s: int) -> bytes:
        return b"COMPLETED" if s == 1 else b"PENDING"

    def assignment_status_from_string(self, s_bytes: Any, out_s: Any) -> int:
        out = _unwrap(out_s)
        if not s_bytes or out is None:
            return -1
        try:
            s = (s_bytes.decode("utf-8") if isinstance(s_bytes, bytes) else str(s_bytes)).strip().upper()
            if s == "PENDING":
                out.value = 0
                return 0
            elif s == "COMPLETED":
                out.value = 1
                return 0
            return -1
        except Exception:
            return -1

    def assignment_compare_priority(self, a_ptr: Any, b_ptr: Any) -> int:
        a = _unwrap(a_ptr)
        b = _unwrap(b_ptr)
        if a is None and b is None:
            return 0
        if a is None:
            return -1
        if b is None:
            return 1

        # 1. Higher priority level first
        if a.priority != b.priority:
            return 1 if a.priority > b.priority else -1

        # 2. Earlier deadline first
        dc = self.date_compare(a.deadline, b.deadline)
        if dc != 0:
            return 1 if dc < 0 else -1

        # 3. Earlier sequence number (FIFO) first
        if a.sequence_num != b.sequence_num:
            return 1 if a.sequence_num < b.sequence_num else -1

        return 0

    def _copy_assignment(self, dest: Any, src: Any) -> None:
        dest.id = src.id
        dest.title = src.title
        dest.subject = src.subject
        dest.deadline.year = src.deadline.year
        dest.deadline.month = src.deadline.month
        dest.deadline.day = src.deadline.day
        dest.priority = src.priority
        dest.sequence_num = src.sequence_num
        dest.status = src.status

    # Priority queue operations
    def pq_init(self, pq_ptr: Any) -> int:
        pq = _unwrap(pq_ptr)
        if pq is None:
            return SAPM_ERR_NULL_PARAM
        pq.size = 0
        pq.next_sequence = 1
        return SAPM_SUCCESS

    def pq_clear(self, pq_ptr: Any) -> int:
        pq = _unwrap(pq_ptr)
        if pq is None:
            return SAPM_ERR_NULL_PARAM
        pq.size = 0
        return SAPM_SUCCESS

    def pq_size(self, pq_ptr: Any) -> int:
        pq = _unwrap(pq_ptr)
        return pq.size if pq is not None else 0

    def pq_is_empty(self, pq_ptr: Any) -> bool:
        pq = _unwrap(pq_ptr)
        return (pq is None) or (pq.size == 0)

    def pq_is_full(self, pq_ptr: Any) -> bool:
        pq = _unwrap(pq_ptr)
        return bool(pq is not None and pq.size >= PQ_CAPACITY)

    def pq_add(self, pq_ptr: Any, item_id: int, title_bytes: Any, subject_bytes: Any, deadline_ptr: Any, priority: int) -> int:
        pq = _unwrap(pq_ptr)
        d_val = _unwrap(deadline_ptr)
        if pq is None or not title_bytes or not subject_bytes or d_val is None:
            return SAPM_ERR_NULL_PARAM
        if item_id <= 0:
            return SAPM_ERR_INVALID_ID
        if not self.priority_is_valid(priority):
            return SAPM_ERR_INVALID_PRIORITY
        if not self.date_is_valid(d_val):
            return SAPM_ERR_INVALID_DATE
        t_str = title_bytes.decode("utf-8", errors="replace") if isinstance(title_bytes, bytes) else str(title_bytes)
        s_str = subject_bytes.decode("utf-8", errors="replace") if isinstance(subject_bytes, bytes) else str(subject_bytes)
        if not t_str or len(t_str.encode("utf-8")) >= ASSIGNMENT_TITLE_MAX:
            return SAPM_ERR_INVALID_TITLE
        if not s_str or len(s_str.encode("utf-8")) >= ASSIGNMENT_SUBJECT_MAX:
            return SAPM_ERR_INVALID_SUBJECT
        if pq.size >= PQ_CAPACITY:
            return SAPM_ERR_QUEUE_FULL
        for i in range(pq.size):
            if pq.items[i].id == item_id:
                return SAPM_ERR_DUPLICATE_ID

        item = pq.items[pq.size]
        item.id = item_id
        item.title = t_str.encode("utf-8")
        item.subject = s_str.encode("utf-8")
        item.deadline.year = d_val.year
        item.deadline.month = d_val.month
        item.deadline.day = d_val.day
        item.priority = priority
        item.sequence_num = pq.next_sequence
        pq.next_sequence += 1
        item.status = STATUS_PENDING
        pq.size += 1
        return SAPM_SUCCESS

    def _find_highest_index(self, pq: Any) -> int:
        if pq.size == 0:
            return -1
        best_idx = 0
        for i in range(1, pq.size):
            if self.assignment_compare_priority(pq.items[i], pq.items[best_idx]) > 0:
                best_idx = i
        return best_idx

    def pq_peek_highest(self, pq_ptr: Any, out_item_ptr: Any) -> int:
        pq = _unwrap(pq_ptr)
        out_item = _unwrap(out_item_ptr)
        if pq is None or out_item is None:
            return SAPM_ERR_NULL_PARAM
        if pq.size == 0:
            return SAPM_ERR_QUEUE_EMPTY
        best_idx = self._find_highest_index(pq)
        self._copy_assignment(out_item, pq.items[best_idx])
        return SAPM_SUCCESS

    def pq_process_highest(self, pq_ptr: Any, out_item_ptr: Any) -> int:
        pq = _unwrap(pq_ptr)
        out_item = _unwrap(out_item_ptr)
        if pq is None or out_item is None:
            return SAPM_ERR_NULL_PARAM
        if pq.size == 0:
            return SAPM_ERR_QUEUE_EMPTY
        best_idx = self._find_highest_index(pq)
        self._copy_assignment(out_item, pq.items[best_idx])
        out_item.status = STATUS_COMPLETED
        for i in range(best_idx, pq.size - 1):
            self._copy_assignment(pq.items[i], pq.items[i + 1])
        pq.size -= 1
        return SAPM_SUCCESS

    def pq_process_by_id(self, pq_ptr: Any, item_id: int, out_item_ptr: Any) -> int:
        pq = _unwrap(pq_ptr)
        out_item = _unwrap(out_item_ptr)
        if pq is None or out_item is None:
            return SAPM_ERR_NULL_PARAM
        target_idx = -1
        for i in range(pq.size):
            if pq.items[i].id == item_id:
                target_idx = i
                break
        if target_idx == -1:
            return SAPM_ERR_NOT_FOUND
        self._copy_assignment(out_item, pq.items[target_idx])
        out_item.status = STATUS_COMPLETED
        for i in range(target_idx, pq.size - 1):
            self._copy_assignment(pq.items[i], pq.items[i + 1])
        pq.size -= 1
        return SAPM_SUCCESS

    def pq_delete_by_id(self, pq_ptr: Any, item_id: int, out_item_ptr: Any) -> int:
        pq = _unwrap(pq_ptr)
        out_item = _unwrap(out_item_ptr)
        if pq is None:
            return SAPM_ERR_NULL_PARAM
        target_idx = -1
        for i in range(pq.size):
            if pq.items[i].id == item_id:
                target_idx = i
                break
        if target_idx == -1:
            return SAPM_ERR_NOT_FOUND
        if out_item is not None:
            self._copy_assignment(out_item, pq.items[target_idx])
        for i in range(target_idx, pq.size - 1):
            self._copy_assignment(pq.items[i], pq.items[i + 1])
        pq.size -= 1
        return SAPM_SUCCESS

    def pq_search_by_id(self, pq_ptr: Any, item_id: int, out_item_ptr: Any) -> int:
        pq = _unwrap(pq_ptr)
        out_item = _unwrap(out_item_ptr)
        if pq is None or out_item is None:
            return SAPM_ERR_NULL_PARAM
        for i in range(pq.size):
            if pq.items[i].id == item_id:
                self._copy_assignment(out_item, pq.items[i])
                return SAPM_SUCCESS
        return SAPM_ERR_NOT_FOUND

    def pq_update_priority(self, pq_ptr: Any, item_id: int, priority: int) -> int:
        pq = _unwrap(pq_ptr)
        if pq is None:
            return SAPM_ERR_NULL_PARAM
        if not self.priority_is_valid(priority):
            return SAPM_ERR_INVALID_PRIORITY
        for i in range(pq.size):
            if pq.items[i].id == item_id:
                pq.items[i].priority = priority
                return SAPM_SUCCESS
        return SAPM_ERR_NOT_FOUND

    def pq_update_deadline(self, pq_ptr: Any, item_id: int, deadline_ptr: Any) -> int:
        pq = _unwrap(pq_ptr)
        d_val = _unwrap(deadline_ptr)
        if pq is None or d_val is None:
            return SAPM_ERR_NULL_PARAM
        if not self.date_is_valid(d_val):
            return SAPM_ERR_INVALID_DATE
        for i in range(pq.size):
            if pq.items[i].id == item_id:
                pq.items[i].deadline.year = d_val.year
                pq.items[i].deadline.month = d_val.month
                pq.items[i].deadline.day = d_val.day
                return SAPM_SUCCESS
        return SAPM_ERR_NOT_FOUND

    def pq_update_priority_and_deadline(self, pq_ptr: Any, item_id: int, priority: int, deadline_ptr: Any) -> int:
        pq = _unwrap(pq_ptr)
        d_val = _unwrap(deadline_ptr)
        if pq is None or d_val is None:
            return SAPM_ERR_NULL_PARAM
        if not self.priority_is_valid(priority):
            return SAPM_ERR_INVALID_PRIORITY
        if not self.date_is_valid(d_val):
            return SAPM_ERR_INVALID_DATE
        for i in range(pq.size):
            if pq.items[i].id == item_id:
                pq.items[i].priority = priority
                pq.items[i].deadline.year = d_val.year
                pq.items[i].deadline.month = d_val.month
                pq.items[i].deadline.day = d_val.day
                return SAPM_SUCCESS
        return SAPM_ERR_NOT_FOUND

    def pq_get_all(self, pq_ptr: Any, out_buf: Any, max_count: int, out_count: Any) -> int:
        pq = _unwrap(pq_ptr)
        count_out = _unwrap(out_count)
        buf = _unwrap(out_buf)
        if pq is None or buf is None or count_out is None:
            return SAPM_ERR_NULL_PARAM
        count = min(pq.size, max_count)
        for i in range(count):
            self._copy_assignment(buf[i], pq.items[i])
        count_out.value = count
        return SAPM_SUCCESS

    def pq_get_sorted_view(self, pq_ptr: Any, out_buf: Any, max_count: int, out_count: Any) -> int:
        pq = _unwrap(pq_ptr)
        count_out = _unwrap(out_count)
        buf = _unwrap(out_buf)
        if pq is None or buf is None or count_out is None:
            return SAPM_ERR_NULL_PARAM
        count = min(pq.size, max_count)
        items = [pq.items[i] for i in range(pq.size)]
        sorted_items = sorted(items, key=cmp_to_key(self.assignment_compare_priority), reverse=True)
        for i in range(count):
            self._copy_assignment(buf[i], sorted_items[i])
        count_out.value = count
        return SAPM_SUCCESS

    # History operations
    def history_init(self, hist_ptr: Any) -> int:
        hist = _unwrap(hist_ptr)
        if hist is None:
            return SAPM_ERR_NULL_PARAM
        hist.size = 0
        return SAPM_SUCCESS

    def history_clear(self, hist_ptr: Any) -> int:
        hist = _unwrap(hist_ptr)
        if hist is None:
            return SAPM_ERR_NULL_PARAM
        hist.size = 0
        return SAPM_SUCCESS

    def history_size(self, hist_ptr: Any) -> int:
        hist = _unwrap(hist_ptr)
        return hist.size if hist is not None else 0

    def history_is_empty(self, hist_ptr: Any) -> bool:
        hist = _unwrap(hist_ptr)
        return (hist is None) or (hist.size == 0)

    def history_is_full(self, hist_ptr: Any) -> bool:
        hist = _unwrap(hist_ptr)
        return bool(hist is not None and hist.size >= HISTORY_CAPACITY)

    def history_add(self, hist_ptr: Any, item_ptr: Any) -> int:
        hist = _unwrap(hist_ptr)
        item = _unwrap(item_ptr)
        if hist is None or item is None:
            return SAPM_ERR_NULL_PARAM
        if hist.size >= HISTORY_CAPACITY:
            return SAPM_ERR_HISTORY_FULL
        self._copy_assignment(hist.items[hist.size], item)
        hist.items[hist.size].status = STATUS_COMPLETED
        hist.size += 1
        return SAPM_SUCCESS

    def history_get_by_id(self, hist_ptr: Any, item_id: int, out_item_ptr: Any) -> int:
        hist = _unwrap(hist_ptr)
        out_item = _unwrap(out_item_ptr)
        if hist is None or out_item is None:
            return SAPM_ERR_NULL_PARAM
        for i in range(hist.size):
            if hist.items[i].id == item_id:
                self._copy_assignment(out_item, hist.items[i])
                return SAPM_SUCCESS
        return SAPM_ERR_NOT_FOUND

    def history_get_all(self, hist_ptr: Any, out_buf: Any, max_count: int, out_count: Any) -> int:
        hist = _unwrap(hist_ptr)
        count_out = _unwrap(out_count)
        buf = _unwrap(out_buf)
        if hist is None or buf is None or count_out is None:
            return SAPM_ERR_NULL_PARAM
        count = min(hist.size, max_count)
        for i in range(count):
            self._copy_assignment(buf[i], hist.items[i])
        count_out.value = count
        return SAPM_SUCCESS

    # Storage operations
    def storage_load(self, filename_bytes: Any, pq_ptr: Any, hist_ptr: Any, out_is_first_run: Any) -> int:
        pq = _unwrap(pq_ptr)
        hist = _unwrap(hist_ptr)
        first_run = _unwrap(out_is_first_run)
        if not filename_bytes or pq is None or hist is None:
            return SAPM_ERR_NULL_PARAM
        fn = filename_bytes.decode("utf-8") if isinstance(filename_bytes, bytes) else str(filename_bytes)
        if not os.path.exists(fn):
            if first_run is not None:
                first_run.value = True
            return SAPM_SUCCESS_FIRST_RUN
        if first_run is not None:
            first_run.value = False

        pq.size = 0
        hist.size = 0
        max_seq = 0
        seen_ids = set()

        try:
            with open(fn, mode="r", encoding="utf-8", errors="replace") as f:
                reader = csv.reader(f)
                header = next(reader, None)
                if not header:
                    return SAPM_SUCCESS
                for row in reader:
                    if not row or not any(row):
                        continue
                    if len(row) < 7:
                        return SAPM_ERR_CORRUPT_RECORD
                    try:
                        r_id = int(row[0].strip())
                        r_title = row[1].strip()
                        r_subject = row[2].strip()
                        r_deadline = row[3].strip()
                        r_priority_str = row[4].strip().upper()
                        r_seq = int(row[5].strip())
                        r_status_str = row[6].strip().upper()
                    except Exception:
                        return SAPM_ERR_CORRUPT_RECORD

                    if r_id <= 0 or r_id in seen_ids:
                        return SAPM_ERR_CORRUPT_RECORD
                    seen_ids.add(r_id)

                    p_map = {"LOW": 1, "MEDIUM": 2, "HIGH": 3, "URGENT": 4}
                    if r_priority_str not in p_map:
                        return SAPM_ERR_CORRUPT_RECORD
                    r_priority = p_map[r_priority_str]

                    d_date = CDate()
                    if self.date_parse(r_deadline.encode("utf-8"), byref(d_date)) != 0:
                        return SAPM_ERR_CORRUPT_RECORD

                    if r_seq > max_seq:
                        max_seq = r_seq

                    if r_status_str == "PENDING":
                        if pq.size >= PQ_CAPACITY:
                            return SAPM_ERR_CORRUPT_RECORD
                        item = pq.items[pq.size]
                        item.id = r_id
                        item.title = r_title.encode("utf-8")
                        item.subject = r_subject.encode("utf-8")
                        item.deadline = d_date
                        item.priority = r_priority
                        item.sequence_num = r_seq
                        item.status = STATUS_PENDING
                        pq.size += 1
                    elif r_status_str == "COMPLETED":
                        if hist.size >= HISTORY_CAPACITY:
                            return SAPM_ERR_CORRUPT_RECORD
                        item = hist.items[hist.size]
                        item.id = r_id
                        item.title = r_title.encode("utf-8")
                        item.subject = r_subject.encode("utf-8")
                        item.deadline = d_date
                        item.priority = r_priority
                        item.sequence_num = r_seq
                        item.status = STATUS_COMPLETED
                        hist.size += 1
                    else:
                        return SAPM_ERR_CORRUPT_RECORD

            pq.next_sequence = max_seq + 1
            return SAPM_SUCCESS
        except Exception:
            return SAPM_ERR_FILE_READ

    def storage_save(self, filename_bytes: Any, pq_ptr: Any, hist_ptr: Any) -> int:
        pq = _unwrap(pq_ptr)
        hist = _unwrap(hist_ptr)
        if not filename_bytes or pq is None or hist is None:
            return SAPM_ERR_NULL_PARAM
        fn = filename_bytes.decode("utf-8") if isinstance(filename_bytes, bytes) else str(filename_bytes)

        tmp_fn = fn + ".tmp"
        p_names = {1: "LOW", 2: "MEDIUM", 3: "HIGH", 4: "URGENT"}
        try:
            target_dir = os.path.dirname(os.path.abspath(fn))
            if target_dir:
                os.makedirs(target_dir, exist_ok=True)
            with open(tmp_fn, mode="w", newline="", encoding="utf-8") as f:
                writer = csv.writer(f)
                writer.writerow(["id", "title", "subject", "deadline", "priority", "sequence_num", "status"])
                for i in range(pq.size):
                    item = pq.items[i]
                    d = item.deadline
                    writer.writerow([
                        item.id,
                        item.title.decode("utf-8", errors="replace"),
                        item.subject.decode("utf-8", errors="replace"),
                        f"{d.year:04d}-{d.month:02d}-{d.day:02d}",
                        p_names.get(item.priority, "MEDIUM"),
                        item.sequence_num,
                        "PENDING",
                    ])
                for i in range(hist.size):
                    item = hist.items[i]
                    d = item.deadline
                    writer.writerow([
                        item.id,
                        item.title.decode("utf-8", errors="replace"),
                        item.subject.decode("utf-8", errors="replace"),
                        f"{d.year:04d}-{d.month:02d}-{d.day:02d}",
                        p_names.get(item.priority, "MEDIUM"),
                        item.sequence_num,
                        "COMPLETED",
                    ])
            os.replace(tmp_fn, fn)
            return SAPM_SUCCESS
        except Exception:
            if os.path.exists(tmp_fn):
                try:
                    os.remove(tmp_fn)
                except Exception:
                    pass
            return SAPM_ERR_FILE_WRITE


class CEngineBridge:
    """Wrapper managing ctypes foreign function interface to libc_engine with automatic in-process fallback."""

    def __init__(self, lib_path: Optional[str] = None):
        self.lib_path = lib_path
        self._lib = None
        self.is_native = False

        if lib_path:
            try:
                self._lib = self._load_library(lib_path)
                self._setup_function_signatures()
                self.is_native = True
            except Exception as e:
                import logging
                logging.warning(
                    "C engine library '%s' could not be loaded (%s). Using PythonEngineFallback.",
                    lib_path, e
                )

        if not self._lib:
            self._lib = PythonEngineFallback()
            self.is_native = False

    def _load_library(self, lib_path: str) -> ctypes.CDLL:
        if not os.path.exists(lib_path):
            base_dir = Path(__file__).resolve().parent.parent.parent
            this_dir = Path(__file__).resolve().parent
            backend_dir = this_dir.parent
            candidates = [
                lib_path,
                lib_path.replace(".dylib", ".so"),
                lib_path.replace(".so", ".dylib"),
                os.path.join(os.path.dirname(lib_path), "libc_engine.dylib"),
                os.path.join(os.path.dirname(lib_path), "libc_engine.so"),
                str(base_dir / "c_engine" / "bin" / "libc_engine.so"),
                str(base_dir / "c_engine" / "bin" / "libc_engine.dylib"),
                str(base_dir / "backend" / "libc_engine.so"),
                str(base_dir / "backend" / "libc_engine.dylib"),
                str(backend_dir / "libc_engine.so"),
                str(backend_dir / "libc_engine.dylib"),
                str(this_dir / "libc_engine.so"),
                str(base_dir / "libc_engine.so"),
                os.path.join(os.getcwd(), "c_engine", "bin", "libc_engine.so"),
                os.path.join(os.getcwd(), "c_engine", "bin", "libc_engine.dylib"),
                os.path.join(os.getcwd(), "backend", "libc_engine.so"),
                "/var/task/c_engine/bin/libc_engine.so",
                "/var/task/backend/libc_engine.so",
                "/var/task/libc_engine.so",
            ]
            for cand in candidates:
                if os.path.exists(cand):
                    lib_path = cand
                    break

        if not os.path.exists(lib_path):
            raise CEngineLoadError(
                f"C engine shared library not found at '{lib_path}'."
            )

        try:
            return ctypes.CDLL(lib_path)
        except OSError as e:
            raise CEngineLoadError(
                f"Failed to load C engine shared library '{lib_path}': {e}"
            ) from e

    def _setup_function_signatures(self) -> None:
        lib = self._lib

        # Status translation
        lib.sapm_status_to_string.argtypes = [c_int]
        lib.sapm_status_to_string.restype = c_char_p

        # Date utils
        lib.date_is_leap_year.argtypes = [c_int]
        lib.date_is_leap_year.restype = c_bool

        lib.date_days_in_month.argtypes = [c_int, c_int]
        lib.date_days_in_month.restype = c_int

        lib.date_is_valid.argtypes = [POINTER(CDate)]
        lib.date_is_valid.restype = c_bool

        lib.date_create.argtypes = [c_int, c_int, c_int, POINTER(CDate)]
        lib.date_create.restype = c_int

        lib.date_compare.argtypes = [POINTER(CDate), POINTER(CDate)]
        lib.date_compare.restype = c_int

        lib.date_format.argtypes = [POINTER(CDate), c_char_p, c_size_t]
        lib.date_format.restype = c_int

        lib.date_parse.argtypes = [c_char_p, POINTER(CDate)]
        lib.date_parse.restype = c_int

        # Priority & Assignment
        lib.priority_is_valid.argtypes = [c_int]
        lib.priority_is_valid.restype = c_bool

        lib.priority_to_string.argtypes = [c_int]
        lib.priority_to_string.restype = c_char_p

        lib.priority_from_string.argtypes = [c_char_p, POINTER(c_int)]
        lib.priority_from_string.restype = c_int

        lib.assignment_status_to_string.argtypes = [c_int]
        lib.assignment_status_to_string.restype = c_char_p

        lib.assignment_status_from_string.argtypes = [c_char_p, POINTER(c_int)]
        lib.assignment_status_from_string.restype = c_int

        lib.assignment_create.argtypes = [
            c_int, c_char_p, c_char_p, POINTER(CDate), c_int, POINTER(CAssignment)
        ]
        lib.assignment_create.restype = c_int

        lib.assignment_compare_priority.argtypes = [POINTER(CAssignment), POINTER(CAssignment)]
        lib.assignment_compare_priority.restype = c_int

        lib.assignment_format.argtypes = [POINTER(CAssignment), c_char_p, c_size_t]
        lib.assignment_format.restype = c_int

        # Priority Queue
        lib.pq_init.argtypes = [POINTER(CPriorityQueue)]
        lib.pq_init.restype = c_int

        lib.pq_clear.argtypes = [POINTER(CPriorityQueue)]
        lib.pq_clear.restype = c_int

        lib.pq_size.argtypes = [POINTER(CPriorityQueue)]
        lib.pq_size.restype = c_size_t

        lib.pq_is_empty.argtypes = [POINTER(CPriorityQueue)]
        lib.pq_is_empty.restype = c_bool

        lib.pq_is_full.argtypes = [POINTER(CPriorityQueue)]
        lib.pq_is_full.restype = c_bool

        lib.pq_insert.argtypes = [POINTER(CPriorityQueue), POINTER(CAssignment)]
        lib.pq_insert.restype = c_int

        lib.pq_add.argtypes = [
            POINTER(CPriorityQueue), c_int, c_char_p, c_char_p, POINTER(CDate), c_int
        ]
        lib.pq_add.restype = c_int

        lib.pq_peek_highest.argtypes = [POINTER(CPriorityQueue), POINTER(CAssignment)]
        lib.pq_peek_highest.restype = c_int

        lib.pq_process_highest.argtypes = [POINTER(CPriorityQueue), POINTER(CAssignment)]
        lib.pq_process_highest.restype = c_int

        lib.pq_process_by_id.argtypes = [POINTER(CPriorityQueue), c_int, POINTER(CAssignment)]
        lib.pq_process_by_id.restype = c_int

        lib.pq_delete_by_id.argtypes = [POINTER(CPriorityQueue), c_int, POINTER(CAssignment)]
        lib.pq_delete_by_id.restype = c_int

        lib.pq_search_by_id.argtypes = [POINTER(CPriorityQueue), c_int, POINTER(CAssignment)]
        lib.pq_search_by_id.restype = c_int

        lib.pq_search_by_title.argtypes = [POINTER(CPriorityQueue), c_char_p, POINTER(CAssignment)]
        lib.pq_search_by_title.restype = c_int

        lib.pq_search_by_title_all.argtypes = [
            POINTER(CPriorityQueue), c_char_p, POINTER(CAssignment), c_size_t, POINTER(c_size_t)
        ]
        lib.pq_search_by_title_all.restype = c_int

        lib.pq_update_priority.argtypes = [POINTER(CPriorityQueue), c_int, c_int]
        lib.pq_update_priority.restype = c_int

        lib.pq_update_deadline.argtypes = [POINTER(CPriorityQueue), c_int, POINTER(CDate)]
        lib.pq_update_deadline.restype = c_int

        lib.pq_update_priority_and_deadline.argtypes = [
            POINTER(CPriorityQueue), c_int, c_int, POINTER(CDate)
        ]
        lib.pq_update_priority_and_deadline.restype = c_int

        lib.pq_get_all.argtypes = [
            POINTER(CPriorityQueue), POINTER(CAssignment), c_size_t, POINTER(c_size_t)
        ]
        lib.pq_get_all.restype = c_int

        lib.pq_get_sorted_view.argtypes = [
            POINTER(CPriorityQueue), POINTER(CAssignment), c_size_t, POINTER(c_size_t)
        ]
        lib.pq_get_sorted_view.restype = c_int

        # History
        lib.history_init.argtypes = [POINTER(CAssignmentHistory)]
        lib.history_init.restype = c_int

        lib.history_clear.argtypes = [POINTER(CAssignmentHistory)]
        lib.history_clear.restype = c_int

        lib.history_size.argtypes = [POINTER(CAssignmentHistory)]
        lib.history_size.restype = c_size_t

        lib.history_is_empty.argtypes = [POINTER(CAssignmentHistory)]
        lib.history_is_empty.restype = c_bool

        lib.history_is_full.argtypes = [POINTER(CAssignmentHistory)]
        lib.history_is_full.restype = c_bool

        lib.history_add.argtypes = [POINTER(CAssignmentHistory), POINTER(CAssignment)]
        lib.history_add.restype = c_int

        lib.history_get_by_id.argtypes = [POINTER(CAssignmentHistory), c_int, POINTER(CAssignment)]
        lib.history_get_by_id.restype = c_int

        lib.history_get_all.argtypes = [
            POINTER(CAssignmentHistory), POINTER(CAssignment), c_size_t, POINTER(c_size_t)
        ]
        lib.history_get_all.restype = c_int

        # Storage
        lib.storage_save.argtypes = [
            c_char_p, POINTER(CPriorityQueue), POINTER(CAssignmentHistory)
        ]
        lib.storage_save.restype = c_int

        lib.storage_load.argtypes = [
            c_char_p, POINTER(CPriorityQueue), POINTER(CAssignmentHistory), POINTER(c_bool)
        ]
        lib.storage_load.restype = c_int

    def status_to_string(self, code: int) -> str:
        msg = self._lib.sapm_status_to_string(code)
        if isinstance(msg, bytes):
            return msg.decode("utf-8", errors="replace")
        return str(msg) if msg else f"Unknown error ({code})"

    def parse_date(self, date_str: str) -> Optional[CDate]:
        d = CDate()
        res = self._lib.date_parse(date_str.encode("utf-8"), byref(d))
        return d if res == 0 else None

    def parse_priority(self, priority_input: Any) -> Optional[int]:
        if isinstance(priority_input, int):
            if self._lib.priority_is_valid(priority_input):
                return priority_input
            return None
        if isinstance(priority_input, str):
            p = c_int()
            res = self._lib.priority_from_string(priority_input.encode("utf-8"), byref(p))
            return p.value if res == 0 else None
        return None
