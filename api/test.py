from http.server import BaseHTTPRequestHandler
import json
import os
import sys
from pathlib import Path

class handler(BaseHTTPRequestHandler):
    def do_GET(self):
        info = {
            "python_version": sys.version,
            "cwd": os.getcwd(),
            "sys_path": sys.path,
            "env_keys": list(os.environ.keys()),
        }
        
        # Test flask import
        try:
            import flask
            info["flask_version"] = flask.__version__
        except Exception as e:
            info["flask_error"] = str(e)
            
        # Check files in /var/task
        try:
            root = Path(os.getcwd())
            info["root_files"] = [p.name for p in root.iterdir()]
            c_engine = root / "c_engine"
            if c_engine.exists():
                info["c_engine_files"] = [p.name for p in c_engine.iterdir()]
                bin_dir = c_engine / "bin"
                if bin_dir.exists():
                    info["bin_files"] = [p.name for p in bin_dir.iterdir()]
            backend = root / "backend"
            if backend.exists():
                info["backend_files"] = [p.name for p in backend.iterdir()]
        except Exception as e:
            info["file_scan_error"] = str(e)

        # Test C library loading
        try:
            import ctypes
            so_paths = [
                "/var/task/c_engine/bin/libc_engine.so",
                "/var/task/backend/libc_engine.so",
                str(Path(os.getcwd()) / "c_engine" / "bin" / "libc_engine.so"),
                str(Path(os.getcwd()) / "backend" / "libc_engine.so"),
            ]
            loaded = False
            for p in so_paths:
                if os.path.exists(p):
                    try:
                        ctypes.CDLL(p)
                        info["c_library_loaded_from"] = p
                        loaded = True
                        break
                    except Exception as le:
                        info[f"load_err_{Path(p).name}"] = str(le)
            if not loaded:
                info["c_library_loaded"] = False
        except Exception as e:
            info["ctypes_error"] = str(e)

        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(info, indent=2).encode('utf-8'))
