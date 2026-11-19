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

# Attempt compilation if missing (e.g. if build step didn't bundle)
c_lib_so = ROOT_DIR / "c_engine" / "bin" / "libc_engine.so"
c_lib_dylib = ROOT_DIR / "c_engine" / "bin" / "libc_engine.dylib"
backend_so = ROOT_DIR / "backend" / "libc_engine.so"
if not c_lib_so.exists() and not c_lib_dylib.exists() and not backend_so.exists():
    import subprocess
    try:
        subprocess.run(["make", "-C", str(ROOT_DIR / "c_engine"), "shared_lib"], check=False)
    except Exception:
        pass

try:
    from backend.app import create_app
    app = create_app()
    handler = app
except Exception as e:
    import traceback
    err_tb = traceback.format_exc()
    from flask import Flask, jsonify
    err_app = Flask(__name__)
    handler = err_app

    @err_app.route("/api/v1/health")
    @err_app.route("/api/<path:path>", methods=["GET", "POST", "PATCH", "DELETE"])
    def debug_handler(path=""):
        c_engine_dir = ROOT_DIR / "c_engine"
        bin_dir = c_engine_dir / "bin"
        backend_dir = ROOT_DIR / "backend"
        return jsonify({
            "status": "error",
            "code": "C_ENGINE_INIT_ERROR",
            "message": str(e),
            "traceback": err_tb.splitlines(),
            "cwd": os.getcwd(),
            "root_dir": str(ROOT_DIR),
            "c_engine_exists": c_engine_dir.exists(),
            "bin_exists": bin_dir.exists(),
            "bin_files": [f.name for f in bin_dir.iterdir()] if bin_dir.exists() else [],
            "backend_so_exists": (backend_dir / "libc_engine.so").exists(),
        }), 500

    app = err_app

if __name__ == "__main__":
    port = int(os.environ.get("PORT", 5000))
    app.run(host="0.0.0.0", port=port)
