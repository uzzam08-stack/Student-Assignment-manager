from flask import Blueprint, current_app, jsonify

statistics_bp = Blueprint("statistics", __name__)


def get_service():
    """Retrieves the AssignmentService singleton from application context."""
    return current_app.extensions["assignment_service"]


@statistics_bp.route("/statistics", methods=["GET"])
def get_statistics():
    """
    GET /api/v1/statistics
    Retrieves workload metrics, capacity utilization, and priority breakdown.
    """
    service = get_service()
    stats = service.get_statistics()

    return jsonify({
        "status": "success",
        "data": stats,
    }), 200
