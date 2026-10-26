import os
import sys
from pathlib import Path
from typing import Any, Dict, Optional

# Ensure project root is on sys.path
BASE_DIR = Path(__file__).resolve().parent.parent
if str(BASE_DIR) not in sys.path:
    sys.path.insert(0, str(BASE_DIR))

from flask import Flask, jsonify, request, send_file
from backend.config import Config, get_config
from backend.services.c_engine_bridge import CEngineBridge, CEngineLoadError
from backend.services.assignment_service import (
    AssignmentService,
    DuplicateIdError,
    NotFoundError,
    QueueEmptyError,
    QueueFullError,
    ServiceError,
    StorageError,
)
from backend.services.validation import ValidationError
from backend.api import assignments_bp, history_bp, statistics_bp


def create_app(config_object: Optional[Any] = None) -> Flask:
    """Application factory for Flask REST API."""
    app = Flask(__name__)

    # Load configuration
    if config_object is None:
        cfg_class = get_config()
        app.config.from_object(cfg_class)
    elif isinstance(config_object, type) or isinstance(config_object, Config):
        app.config.from_object(config_object)
    elif isinstance(config_object, dict):
        app.config.update(config_object)

    # Initialize C Engine Bridge and Assignment Service
    c_lib_path = app.config.get("C_LIB_PATH")
    if not c_lib_path:
        raise CEngineLoadError("Configuration missing 'C_LIB_PATH'")

    bridge = CEngineBridge(c_lib_path)
    data_filepath = app.config.get("DATA_FILEPATH", "assignments_data.csv")
    persistence_enabled = app.config.get("PERSISTENCE_ENABLED", True)

    assignment_service = AssignmentService(
        bridge=bridge,
        data_filepath=data_filepath,
        persistence_enabled=persistence_enabled,
    )

    # Store service instance in application extensions
    app.extensions["assignment_service"] = assignment_service
    app.extensions["c_bridge"] = bridge

    # Register Blueprints with unified API prefix
    app.register_blueprint(assignments_bp, url_prefix="/api/v1")
    app.register_blueprint(history_bp, url_prefix="/api/v1")
    app.register_blueprint(statistics_bp, url_prefix="/api/v1")

    # Health check endpoint
    @app.route("/api/v1/health", methods=["GET"])
    def health_check():
        stats = assignment_service.get_statistics()
        return jsonify({
            "status": "healthy",
            "c_engine_loaded": True,
            "c_lib_path": c_lib_path,
            "persistence_enabled": persistence_enabled,
            "data_filepath": data_filepath,
            "pending_count": stats["total_pending"],
            "completed_count": stats["total_completed"],
        }), 200

    # Frontend root route
    @app.route("/", methods=["GET"])
    @app.route("/index.html", methods=["GET"])
    def serve_frontend():
        index_path = BASE_DIR / "index.html"
        if index_path.exists():
            return send_file(index_path, mimetype="text/html")
        return jsonify({"message": "Student Assignment Priority Manager"}), 200


    # Error Handlers
    @app.errorhandler(ValidationError)
    def handle_validation_error(err: ValidationError):
        return jsonify({
            "status": "error",
            "code": "VALIDATION_ERROR",
            "message": err.message,
            "details": err.details,
        }), 400

    @app.errorhandler(DuplicateIdError)
    def handle_duplicate_id_error(err: DuplicateIdError):
        return jsonify({
            "status": "error",
            "code": err.code,
            "message": err.message,
        }), 409

    @app.errorhandler(NotFoundError)
    def handle_not_found_error(err: NotFoundError):
        return jsonify({
            "status": "error",
            "code": err.code,
            "message": err.message,
        }), 404

    @app.errorhandler(QueueFullError)
    def handle_queue_full_error(err: QueueFullError):
        return jsonify({
            "status": "error",
            "code": err.code,
            "message": err.message,
        }), 400

    @app.errorhandler(QueueEmptyError)
    def handle_queue_empty_error(err: QueueEmptyError):
        return jsonify({
            "status": "error",
            "code": err.code,
            "message": err.message,
        }), 400

    @app.errorhandler(StorageError)
    def handle_storage_error(err: StorageError):
        return jsonify({
            "status": "error",
            "code": err.code,
            "message": err.message,
        }), 500

    @app.errorhandler(ServiceError)
    def handle_service_error(err: ServiceError):
        return jsonify({
            "status": "error",
            "code": err.code,
            "message": err.message,
        }), err.status_code

    @app.errorhandler(CEngineLoadError)
    def handle_c_load_error(err: CEngineLoadError):
        return jsonify({
            "status": "error",
            "code": "C_ENGINE_LOAD_ERROR",
            "message": str(err),
        }), 500

    @app.errorhandler(404)
    def handle_http_404(err):
        return jsonify({
            "status": "error",
            "code": "ENDPOINT_NOT_FOUND",
            "message": f"Endpoint not found: {request.method} {request.path}",
        }), 404

    @app.errorhandler(405)
    def handle_http_405(err):
        return jsonify({
            "status": "error",
            "code": "METHOD_NOT_ALLOWED",
            "message": f"Method not allowed for {request.path}",
        }), 405

    @app.errorhandler(500)
    def handle_http_500(err):
        return jsonify({
            "status": "error",
            "code": "INTERNAL_SERVER_ERROR",
            "message": "An unexpected server error occurred",
        }), 500

    return app


if __name__ == "__main__":
    app = create_app()
    host = os.environ.get("FLASK_HOST", os.environ.get("HOST", "0.0.0.0"))
    port = int(os.environ.get("PORT", os.environ.get("FLASK_PORT", 5000)))
    print(f"Starting Student Assignment Priority Manager on http://{host}:{port}")
    app.run(host=host, port=port, debug=False)

