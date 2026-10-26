"""API package for Student Assignment Priority Manager."""
from backend.api.assignments import assignments_bp
from backend.api.history import history_bp
from backend.api.statistics import statistics_bp

__all__ = ["assignments_bp", "history_bp", "statistics_bp"]
