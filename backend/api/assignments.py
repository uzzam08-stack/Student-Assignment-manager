from flask import Blueprint, current_app, jsonify, request
from backend.services.validation import (
    validate_create_assignment_payload,
    validate_update_assignment_payload,
)

assignments_bp = Blueprint("assignments", __name__)


def get_service():
    """Retrieves the AssignmentService singleton from application context."""
    return current_app.extensions["assignment_service"]


@assignments_bp.route("/assignments", methods=["GET"])
def list_assignments():
    """
    GET /api/v1/assignments
    Query parameters:
      - sorted: bool (if true, ordered strictly by C engine authoritative priority)
      - subject: filter by subject name
      - priority: filter by priority (1-4 or LOW, MEDIUM, HIGH, URGENT)
      - search: filter by title substring
    """
    service = get_service()

    sorted_param = request.args.get("sorted", "false").lower() in ("true", "1", "yes")
    subject_param = request.args.get("subject")
    search_param = request.args.get("search")

    priority_param = None
    raw_priority = request.args.get("priority")
    if raw_priority:
        priority_param = service.bridge.parse_priority(raw_priority)
        if priority_param is None:
            return jsonify({
                "status": "error",
                "code": "INVALID_QUERY_PARAM",
                "message": f"Invalid priority query parameter '{raw_priority}'",
            }), 400

    assignments = service.get_assignments(
        sorted_by_priority=sorted_param,
        subject=subject_param,
        priority=priority_param,
        search=search_param,
    )

    return jsonify({
        "status": "success",
        "count": len(assignments),
        "sorted_by_priority": sorted_param,
        "data": assignments,
    }), 200


@assignments_bp.route("/assignments", methods=["POST"])
def create_assignment():
    """
    POST /api/v1/assignments
    Creates and enqueues a new assignment.
    """
    service = get_service()
    payload = request.get_json(silent=True)
    if payload is None:
        return jsonify({
            "status": "error",
            "code": "INVALID_JSON",
            "message": "Request body must be valid JSON",
        }), 400

    assignment_id, title, subject, c_date, priority_val = validate_create_assignment_payload(
        payload, service.bridge
    )

    created = service.create_assignment(assignment_id, title, subject, c_date, priority_val)

    return jsonify({
        "status": "success",
        "message": "Assignment created successfully",
        "data": created,
    }), 201


@assignments_bp.route("/assignments/<int:assignment_id>", methods=["GET"])
def get_assignment(assignment_id: int):
    """
    GET /api/v1/assignments/{id}
    Retrieves a specific pending assignment by ID.
    """
    service = get_service()
    assignment = service.get_assignment_by_id(assignment_id)
    return jsonify({
        "status": "success",
        "data": assignment,
    }), 200


@assignments_bp.route("/assignments/<int:assignment_id>", methods=["PATCH"])
def update_assignment(assignment_id: int):
    """
    PATCH /api/v1/assignments/{id}
    Updates priority and/or deadline of an existing assignment.
    """
    service = get_service()
    payload = request.get_json(silent=True)
    if payload is None:
        return jsonify({
            "status": "error",
            "code": "INVALID_JSON",
            "message": "Request body must be valid JSON",
        }), 400

    priority_val, c_date = validate_update_assignment_payload(payload, service.bridge)

    updated = service.update_assignment(assignment_id, priority=priority_val, deadline=c_date)

    return jsonify({
        "status": "success",
        "message": "Assignment updated successfully",
        "data": updated,
    }), 200


@assignments_bp.route("/assignments/<int:assignment_id>", methods=["DELETE"])
def delete_assignment(assignment_id: int):
    """
    DELETE /api/v1/assignments/{id}
    Deletes a pending assignment from the queue.
    """
    service = get_service()
    deleted = service.delete_assignment(assignment_id)
    return jsonify({
        "status": "success",
        "message": "Assignment deleted successfully",
        "data": deleted,
    }), 200


@assignments_bp.route("/assignments/process-next", methods=["POST"])
def process_next_assignment():
    """
    POST /api/v1/assignments/process-next
    The C engine selects the highest-priority assignment, marks it completed,
    and moves it to history.
    """
    service = get_service()
    processed = service.process_next_assignment()
    return jsonify({
        "status": "success",
        "message": "Highest-priority assignment processed successfully",
        "data": processed,
    }), 200


@assignments_bp.route("/assignments/<int:assignment_id>/process", methods=["POST"])
def process_assignment(assignment_id: int):
    """
    POST /api/v1/assignments/{id}/process
    Processes a specific assignment by ID via C engine, marks it completed,
    and moves it to history.
    """
    service = get_service()
    processed = service.process_assignment_by_id(assignment_id)
    return jsonify({
        "status": "success",
        "message": f"Assignment {assignment_id} processed successfully",
        "data": processed,
    }), 200

