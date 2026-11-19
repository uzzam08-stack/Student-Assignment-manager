import os
import sys
from pathlib import Path

# Add project root to sys.path so backend and c_engine are accessible
ROOT_DIR = Path(__file__).resolve().parent.parent
if str(ROOT_DIR) not in sys.path:
    sys.path.insert(0, str(ROOT_DIR))

# Mark environment as Vercel serverless
os.environ.setdefault("VERCEL", "1")

# If C engine shared library hasn't been built yet, attempt to build it
c_lib_so = ROOT_DIR / "c_engine" / "bin" / "libc_engine.so"
c_lib_dylib = ROOT_DIR / "c_engine" / "bin" / "libc_engine.dylib"
if not c_lib_so.exists() and not c_lib_dylib.exists():
    import subprocess
    try:
        subprocess.run(["make", "-C", str(ROOT_DIR / "c_engine"), "shared_lib"], check=False)
    except Exception:
        pass

from backend.app import create_app

app = create_app()
handler = app

if __name__ == "__main__":
    port = int(os.environ.get("PORT", 5000))
    app.run(host="0.0.0.0", port=port)
