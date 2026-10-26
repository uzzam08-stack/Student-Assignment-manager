import ctypes
from ctypes import (
    POINTER, byref, c_bool, c_char_p, c_int, c_size_t, c_uint64, create_string_buffer
)
import os
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


class CEngineBridge:
    """Wrapper managing ctypes foreign function interface to libc_engine."""

    def __init__(self, lib_path: str):
        self.lib_path = lib_path
        self._lib = self._load_library(lib_path)
        self._setup_function_signatures()

    def _load_library(self, lib_path: str) -> ctypes.CDLL:
        if not os.path.exists(lib_path):
            # Try alternate extension or location
            candidates = [
                lib_path,
                lib_path.replace(".dylib", ".so"),
                lib_path.replace(".so", ".dylib"),
                os.path.join(os.path.dirname(lib_path), "libc_engine.dylib"),
                os.path.join(os.path.dirname(lib_path), "libc_engine.so"),
            ]
            for cand in candidates:
                if os.path.exists(cand):
                    lib_path = cand
                    break

        if not os.path.exists(lib_path):
            raise CEngineLoadError(
                f"C engine shared library not found at '{lib_path}'. "
                f"Please compile the C engine first using: 'make -C c_engine shared_lib'"
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
        return msg.decode("utf-8") if msg else f"Unknown error ({code})"

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
