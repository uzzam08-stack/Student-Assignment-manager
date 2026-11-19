#!/bin/bash
set -e

echo "=== Vercel Build: Compiling C Priority Queue Engine ==="
make -C c_engine shared_lib
echo "=== Build Complete: C engine shared library created ==="
ls -la c_engine/bin/
