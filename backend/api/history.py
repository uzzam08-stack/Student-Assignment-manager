from flask import Blueprint, current_app, jsonify

history_bp = Blueprint("history", __name__)


def get_service():
    """Retrieves the AssignmentService singleton from application context."""
    return current_app.extensions["assignment_service"]


@history_bp.route("/history", methods=["GET"])
def list_history():
    """
    GET /api/v1/history
    Retrieves all archived completed assignment records.
    """
    service = get_service()
    history_items = service.get_history()

    return jsonify({
        "status": "success",
        "count": len(history_items),
        "data": history_items,
    }), 200
