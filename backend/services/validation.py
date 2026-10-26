from typing import Any, Dict, List, Optional, Tuple
from backend.services.c_engine_bridge import CEngineBridge, CDate


class ValidationError(Exception):
    """Raised when request payload fails validation checks."""
    def __init__(self, message: str, details: Optional[List[str]] = None):
        super().__init__(message)
        self.message = message
        self.details = details or [message]


def validate_create_assignment_payload(
    data: Any, bridge: CEngineBridge
) -> Tuple[int, str, str, CDate, int]:
    """
    Validates payload for creating a new assignment.
    Returns (id, title, subject, c_date, priority_int) or raises ValidationError.
    """
    if not isinstance(data, dict):
        raise ValidationError("Request body must be a JSON object")

    errors: List[str] = []

    # 1. Validate ID
    raw_id = data.get("id")
    if raw_id is None:
        errors.append("Field 'id' is required")
        assignment_id = 0
    elif not isinstance(raw_id, int) or raw_id <= 0:
        errors.append("Field 'id' must be a positive integer > 0")
        assignment_id = 0
    else:
        assignment_id = raw_id

    # 2. Validate Title
    raw_title = data.get("title")
    if raw_title is None:
        errors.append("Field 'title' is required")
        title = ""
    elif not isinstance(raw_title, str) or not raw_title.strip():
        errors.append("Field 'title' must be a non-empty string")
        title = ""
    elif len(raw_title.strip()) >= 128:
        errors.append("Field 'title' must be less than 128 characters")
        title = ""
    else:
        title = raw_title.strip()

    # 3. Validate Subject
    raw_subject = data.get("subject")
    if raw_subject is None:
        errors.append("Field 'subject' is required")
        subject = ""
    elif not isinstance(raw_subject, str) or not raw_subject.strip():
        errors.append("Field 'subject' must be a non-empty string")
        subject = ""
    elif len(raw_subject.strip()) >= 64:
        errors.append("Field 'subject' must be less than 64 characters")
        subject = ""
    else:
        subject = raw_subject.strip()

    # 4. Validate Deadline
    raw_deadline = data.get("deadline")
    c_date: Optional[CDate] = None
    if raw_deadline is None:
        errors.append("Field 'deadline' is required (YYYY-MM-DD)")
    elif not isinstance(raw_deadline, str):
        errors.append("Field 'deadline' must be a string in YYYY-MM-DD format")
    else:
        c_date = bridge.parse_date(raw_deadline.strip())
        if c_date is None:
            errors.append(
                f"Field 'deadline' '{raw_deadline}' is invalid. "
                "Must be a valid calendar date in YYYY-MM-DD format (including leap year rules)"
            )

    # 5. Validate Priority
    raw_priority = data.get("priority")
    priority_val: Optional[int] = None
    if raw_priority is None:
        errors.append("Field 'priority' is required (1-4 or LOW, MEDIUM, HIGH, URGENT)")
    else:
        priority_val = bridge.parse_priority(raw_priority)
        if priority_val is None:
            errors.append(
                f"Field 'priority' '{raw_priority}' is invalid. "
                "Must be 1 (LOW), 2 (MEDIUM), 3 (HIGH), or 4 (URGENT)"
            )

    if errors:
        raise ValidationError("Invalid assignment payload", errors)

    assert c_date is not None
    assert priority_val is not None
    return assignment_id, title, subject, c_date, priority_val


def validate_update_assignment_payload(
    data: Any, bridge: CEngineBridge
) -> Tuple[Optional[int], Optional[CDate]]:
    """
    Validates payload for updating an assignment.
    At least one of 'priority' or 'deadline' must be provided.
    Returns (priority_int, c_date) or raises ValidationError.
    """
    if not isinstance(data, dict):
        raise ValidationError("Request body must be a JSON object")

    errors: List[str] = []
    has_update = False

    priority_val: Optional[int] = None
    if "priority" in data:
        has_update = True
        raw_priority = data["priority"]
        priority_val = bridge.parse_priority(raw_priority)
        if priority_val is None:
            errors.append(
                f"Field 'priority' '{raw_priority}' is invalid. "
                "Must be 1 (LOW), 2 (MEDIUM), 3 (HIGH), or 4 (URGENT)"
            )

    c_date: Optional[CDate] = None
    if "deadline" in data:
        has_update = True
        raw_deadline = data["deadline"]
        if not isinstance(raw_deadline, str):
            errors.append("Field 'deadline' must be a string in YYYY-MM-DD format")
        else:
            c_date = bridge.parse_date(raw_deadline.strip())
            if c_date is None:
                errors.append(
                    f"Field 'deadline' '{raw_deadline}' is invalid. "
                    "Must be a valid calendar date in YYYY-MM-DD format"
                )

    if not has_update:
        errors.append("At least one field ('priority' or 'deadline') must be provided to update")

    if errors:
        raise ValidationError("Invalid update payload", errors)

    return priority_val, c_date
