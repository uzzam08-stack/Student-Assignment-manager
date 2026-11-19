import os
import sys
from pathlib import Path

# Add project root to sys.path so backend and c_engine are accessible
ROOT_DIR = Path(__file__).resolve().parent.parent
if str(ROOT_DIR) not in sys.path:
    sys.path.insert(0, str(ROOT_DIR))

# Mark environment as Vercel serverless
os.environ.setdefault("VERCEL", "1")
if "SAPM_DATA_FILEPATH" not in os.environ:
    os.environ["SAPM_DATA_FILEPATH"] = "/tmp/assignments_data.csv"

from backend.app import create_app

app = create_app()


class VercelPathMiddleware:
    """Ensures PATH_INFO matches the original client request path on Vercel."""

    def __init__(self, wsgi_app):
        self.wsgi_app = wsgi_app

    def __call__(self, environ, start_response):
        path = environ.get("PATH_INFO", "")
        if path.startswith("/api/index/"):
            environ["PATH_INFO"] = "/api/" + path[len("/api/index/") :]
        elif path == "/api/index":
            environ["PATH_INFO"] = "/api"
        return self.wsgi_app(environ, start_response)


# Apply middleware to Flask WSGI stack
app.wsgi_app = VercelPathMiddleware(app.wsgi_app)

if __name__ == "__main__":
    port = int(os.environ.get("PORT", 5000))
    app.run(host="0.0.0.0", port=port)
