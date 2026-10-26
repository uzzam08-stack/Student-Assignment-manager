import ctypes
from ctypes import byref, c_bool, c_size_t
import os
import threading
from typing import Any, Callable, Dict, List, Optional, Tuple

from backend.services.c_engine_bridge import (
    CAssignment,
    CAssignmentHistory,
    CDate,
    CEngineBridge,
    CPriorityQueue,
    HISTORY_CAPACITY,
    PQ_CAPACITY,
    PRIORITY_NAMES,
    SAPM_ERR_DUPLICATE_ID,
    SAPM_ERR_HISTORY_FULL,
    SAPM_ERR_INVALID_DATE,
    SAPM_ERR_INVALID_ID,
    SAPM_ERR_INVALID_PRIORITY,
    SAPM_ERR_INVALID_SUBJECT,
    SAPM_ERR_INVALID_TITLE,
    SAPM_ERR_NOT_FOUND,
    SAPM_ERR_QUEUE_EMPTY,
    SAPM_ERR_QUEUE_FULL,
    SAPM_SUCCESS,
    SAPM_SUCCESS_FIRST_RUN,
)


class ServiceError(Exception):
    """Base domain service exception."""
    def __init__(self, message: str, code: str = "INTERNAL_ERROR", status_code: int = 500):
        super().__init__(message)
        self.message = message
        self.code = code
        self.status_code = status_code


class NotFoundError(ServiceError):
    def __init__(self, message: str):
        super().__init__(message, code="NOT_FOUND", status_code=404)


class DuplicateIdError(ServiceError):
    def __init__(self, message: str):
        super().__init__(message, code="DUPLICATE_ID", status_code=409)


class QueueFullError(ServiceError):
    def __init__(self, message: str):
        super().__init__(message, code="QUEUE_FULL", status_code=400)


class QueueEmptyError(ServiceError):
    def __init__(self, message: str):
        super().__init__(message, code="QUEUE_EMPTY", status_code=400)


class StorageError(ServiceError):
    def __init__(self, message: str):
        super().__init__(message, code="STORAGE_ERROR", status_code=500)


class AssignmentService:
    """
    Thread-safe domain service coordinating the C Priority Queue and Completion History.
    Enforces transactional persistence and strict error mapping.
    """

    def __init__(self, bridge: CEngineBridge, data_filepath: str, persistence_enabled: bool = True):
        self.bridge = bridge
        self.data_filepath = data_filepath
        self.persistence_enabled = persistence_enabled
        self._lock = threading.RLock()

        self._pq = CPriorityQueue()
        self._hist = CAssignmentHistory()

        self._initialize()

    def _initialize(self) -> None:
        """Initializes C data structures and loads persisted state."""
        with self._lock:
            self.bridge._lib.pq_init(byref(self._pq))
            self.bridge._lib.history_init(byref(self._hist))

            if self.persistence_enabled and self.data_filepath:
                first_run = c_bool(False)
                res = self.bridge._lib.storage_load(
                    self.data_filepath.encode("utf-8"),
                    byref(self._pq),
                    byref(self._hist),
                    byref(first_run)
                )
                if res not in (SAPM_SUCCESS, SAPM_SUCCESS_FIRST_RUN):
                    err_msg = self.bridge.status_to_string(res)
                    raise StorageError(f"Failed to load assignment data from '{self.data_filepath}': {err_msg}")

    def _snapshot_state(self) -> Tuple[bytes, bytes]:
        """Creates binary memory snapshots of the C structs for rollback."""
        pq_bytes = ctypes.string_at(ctypes.addressof(self._pq), ctypes.sizeof(CPriorityQueue))
        hist_bytes = ctypes.string_at(ctypes.addressof(self._hist), ctypes.sizeof(CAssignmentHistory))
        return pq_bytes, hist_bytes

    def _restore_state(self, snapshot: Tuple[bytes, bytes]) -> None:
        """Restores in-memory C structs from snapshot."""
        pq_bytes, hist_bytes = snapshot
        ctypes.memmove(ctypes.addressof(self._pq), pq_bytes, len(pq_bytes))
        ctypes.memmove(ctypes.addressof(self._hist), hist_bytes, len(hist_bytes))

    def _execute_transactional(self, action: Callable[[], Any]) -> Any:
        """
        Executes an in-memory mutation action transactionally.
        If persistence fails or action raises an exception, in-memory state is rolled back.
        """
        with self._lock:
            snapshot = self._snapshot_state()
            try:
                result = action()
                if self.persistence_enabled and self.data_filepath:
                    save_res = self.bridge._lib.storage_save(
                        self.data_filepath.encode("utf-8"),
                        byref(self._pq),
                        byref(self._hist)
                    )
                    if save_res != SAPM_SUCCESS:
                        err_msg = self.bridge.status_to_string(save_res)
                        raise StorageError(f"Failed to persist state changes: {err_msg}")
                return result
            except Exception:
                # Rollback in-memory state on any failure
                self._restore_state(snapshot)
                raise

    def get_assignments(
        self,
        sorted_by_priority: bool = False,
        subject: Optional[str] = None,
        priority: Optional[int] = None,
        search: Optional[str] = None,
    ) -> List[Dict[str, Any]]:
        """
        Retrieves pending assignments.
        If sorted_by_priority is True, the C engine determines optimal priority order.
        """
        with self._lock:
            buf = (CAssignment * PQ_CAPACITY)()
            count = c_size_t(0)

            if sorted_by_priority:
                st = self.bridge._lib.pq_get_sorted_view(byref(self._pq), buf, PQ_CAPACITY, byref(count))
            else:
                st = self.bridge._lib.pq_get_all(byref(self._pq), buf, PQ_CAPACITY, byref(count))

            if st != SAPM_SUCCESS:
                raise ServiceError(self.bridge.status_to_string(st))

            results: List[Dict[str, Any]] = []
            for i in range(count.value):
                item = buf[i].to_dict()

                # Filter by subject
                if subject and item["subject"].lower() != subject.strip().lower():
                    continue

                # Filter by priority
                if priority is not None and item["priority_level"] != priority:
                    continue

                # Filter by title search substring
                if search and search.strip().lower() not in item["title"].lower():
                    continue

                results.append(item)

            return results

    def get_assignment_by_id(self, assignment_id: int) -> Dict[str, Any]:
        """Retrieves a specific pending assignment by ID."""
        with self._lock:
            item = CAssignment()
            st = self.bridge._lib.pq_search_by_id(byref(self._pq), assignment_id, byref(item))
            if st == SAPM_ERR_NOT_FOUND:
                raise NotFoundError(f"Assignment with ID {assignment_id} not found in pending queue")
            if st != SAPM_SUCCESS:
                raise ServiceError(self.bridge.status_to_string(st))

            return item.to_dict()

    def create_assignment(
        self, assignment_id: int, title: str, subject: str, deadline: CDate, priority: int
    ) -> Dict[str, Any]:
        """Creates and enqueues a new assignment transactionally."""
        def _action() -> Dict[str, Any]:
            st = self.bridge._lib.pq_add(
                byref(self._pq),
                assignment_id,
                title.encode("utf-8"),
                subject.encode("utf-8"),
                byref(deadline),
                priority,
            )
            if st == SAPM_ERR_DUPLICATE_ID:
                raise DuplicateIdError(f"Assignment with ID {assignment_id} already exists")
            if st == SAPM_ERR_QUEUE_FULL:
                raise QueueFullError("Priority queue capacity reached (maximum 100 assignments)")
            if st in (
                SAPM_ERR_INVALID_ID,
                SAPM_ERR_INVALID_TITLE,
                SAPM_ERR_INVALID_SUBJECT,
                SAPM_ERR_INVALID_DATE,
                SAPM_ERR_INVALID_PRIORITY,
            ):
                raise ServiceError(self.bridge.status_to_string(st), code="VALIDATION_ERROR", status_code=400)
            if st != SAPM_SUCCESS:
                raise ServiceError(self.bridge.status_to_string(st))

            # Fetch the newly created assignment from queue
            created = CAssignment()
            self.bridge._lib.pq_search_by_id(byref(self._pq), assignment_id, byref(created))
            return created.to_dict()

        return self._execute_transactional(_action)

    def update_assignment(
        self, assignment_id: int, priority: Optional[int] = None, deadline: Optional[CDate] = None
    ) -> Dict[str, Any]:
        """Updates priority and/or deadline of an existing assignment transactionally."""
        def _action() -> Dict[str, Any]:
            if priority is not None and deadline is not None:
                st = self.bridge._lib.pq_update_priority_and_deadline(
                    byref(self._pq), assignment_id, priority, byref(deadline)
                )
            elif priority is not None:
                st = self.bridge._lib.pq_update_priority(byref(self._pq), assignment_id, priority)
            elif deadline is not None:
                st = self.bridge._lib.pq_update_deadline(byref(self._pq), assignment_id, byref(deadline))
            else:
                raise ServiceError("No update fields provided", code="BAD_REQUEST", status_code=400)

            if st == SAPM_ERR_NOT_FOUND:
                raise NotFoundError(f"Assignment with ID {assignment_id} not found to update")
            if st == SAPM_ERR_INVALID_PRIORITY:
                raise ServiceError("Invalid priority value", code="VALIDATION_ERROR", status_code=400)
            if st == SAPM_ERR_INVALID_DATE:
                raise ServiceError("Invalid calendar date", code="VALIDATION_ERROR", status_code=400)
            if st != SAPM_SUCCESS:
                raise ServiceError(self.bridge.status_to_string(st))

            updated = CAssignment()
            self.bridge._lib.pq_search_by_id(byref(self._pq), assignment_id, byref(updated))
            return updated.to_dict()

        return self._execute_transactional(_action)

    def delete_assignment(self, assignment_id: int) -> Dict[str, Any]:
        """Deletes a pending assignment by ID transactionally."""
        def _action() -> Dict[str, Any]:
            deleted = CAssignment()
            st = self.bridge._lib.pq_delete_by_id(byref(self._pq), assignment_id, byref(deleted))
            if st in (SAPM_ERR_NOT_FOUND, SAPM_ERR_QUEUE_EMPTY):
                raise NotFoundError(f"Assignment with ID {assignment_id} not found to delete")
            if st != SAPM_SUCCESS:
                raise ServiceError(self.bridge.status_to_string(st))

            return deleted.to_dict()

        return self._execute_transactional(_action)

    def process_next_assignment(self) -> Dict[str, Any]:
        """
        Extracts the highest-priority assignment as decided strictly by the C engine,
        marks it COMPLETED, and archives it into completion history transactionally.
        """
        def _action() -> Dict[str, Any]:
            processed = CAssignment()
            st = self.bridge._lib.pq_process_highest(byref(self._pq), byref(processed))
            if st == SAPM_ERR_QUEUE_EMPTY:
                raise QueueEmptyError("No pending assignments to process (queue is empty)")
            if st != SAPM_SUCCESS:
                raise ServiceError(self.bridge.status_to_string(st))

            # Store in completion history
            hist_st = self.bridge._lib.history_add(byref(self._hist), byref(processed))
            if hist_st == SAPM_ERR_HISTORY_FULL:
                raise ServiceError("Completion history storage is full", code="HISTORY_FULL", status_code=507)
            if hist_st != SAPM_SUCCESS:
                raise ServiceError(self.bridge.status_to_string(hist_st))

            return processed.to_dict()

        return self._execute_transactional(_action)

    def process_assignment_by_id(self, assignment_id: int) -> Dict[str, Any]:
        """
        Processes a specific assignment by ID using C engine's pq_process_by_id,
        marks it COMPLETED, and archives it into completion history transactionally.
        """
        def _action() -> Dict[str, Any]:
            processed = CAssignment()
            st = self.bridge._lib.pq_process_by_id(byref(self._pq), assignment_id, byref(processed))
            if st in (SAPM_ERR_NOT_FOUND, SAPM_ERR_QUEUE_EMPTY):
                raise NotFoundError(f"Assignment with ID {assignment_id} not found to process")
            if st != SAPM_SUCCESS:
                raise ServiceError(self.bridge.status_to_string(st))

            # Store in completion history
            hist_st = self.bridge._lib.history_add(byref(self._hist), byref(processed))
            if hist_st == SAPM_ERR_HISTORY_FULL:
                raise ServiceError("Completion history storage is full", code="HISTORY_FULL", status_code=507)
            if hist_st != SAPM_SUCCESS:
                raise ServiceError(self.bridge.status_to_string(hist_st))

            return processed.to_dict()

        return self._execute_transactional(_action)

    def get_history(self) -> List[Dict[str, Any]]:
        """Retrieves all completed assignment records from history."""
        with self._lock:
            buf = (CAssignment * HISTORY_CAPACITY)()
            count = c_size_t(0)
            st = self.bridge._lib.history_get_all(byref(self._hist), buf, HISTORY_CAPACITY, byref(count))
            if st != SAPM_SUCCESS:
                raise ServiceError(self.bridge.status_to_string(st))

            results: List[Dict[str, Any]] = []
            for i in range(count.value):
                results.append(buf[i].to_dict())
            return results

    def get_statistics(self) -> Dict[str, Any]:
        """Computes summary statistics and priority distribution."""
        with self._lock:
            pending_count = int(self.bridge._lib.pq_size(byref(self._pq)))
            history_count = int(self.bridge._lib.history_size(byref(self._hist)))

            # Priority breakdown of pending items
            breakdown = {name: 0 for name in PRIORITY_NAMES.values()}
            buf = (CAssignment * PQ_CAPACITY)()
            count = c_size_t(0)
            st = self.bridge._lib.pq_get_all(byref(self._pq), buf, PQ_CAPACITY, byref(count))

            if st == SAPM_SUCCESS:
                for i in range(count.value):
                    pname = PRIORITY_NAMES.get(buf[i].priority, "UNKNOWN")
                    if pname in breakdown:
                        breakdown[pname] += 1

            # Peek highest priority without removing
            next_assignment: Optional[Dict[str, Any]] = None
            if pending_count > 0:
                top = CAssignment()
                if self.bridge._lib.pq_peek_highest(byref(self._pq), byref(top)) == SAPM_SUCCESS:
                    next_assignment = top.to_dict()

            capacity_used_pct = round((pending_count / PQ_CAPACITY) * 100, 1)

            return {
                "total_pending": pending_count,
                "total_completed": history_count,
                "capacity": PQ_CAPACITY,
                "capacity_available": PQ_CAPACITY - pending_count,
                "capacity_used_percentage": capacity_used_pct,
                "priority_breakdown": breakdown,
                "next_highest_assignment": next_assignment,
            }
