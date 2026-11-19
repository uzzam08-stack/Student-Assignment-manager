#!/bin/sh
set -e
echo "Building C priority queue engine..."
make -C c_engine shared_lib
echo "Build complete."
