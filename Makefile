CC ?= gcc
PORT ?= 5000
HOST ?= 0.0.0.0
WORKERS ?= 1
THREADS ?= 4

.PHONY: all build test clean run

all: build

build:
	@echo "Building C core priority queue engine..."
	$(MAKE) -C c_engine shared_lib
	@echo "C shared library ready."
	@mkdir -p public
	@cp -f index.html public/index.html
	@echo "Static frontend ready in public/."
	@mkdir -p backend
	@cp -f c_engine/bin/libc_engine.so backend/libc_engine.so 2>/dev/null || true
	@cp -f c_engine/bin/libc_engine.dylib backend/libc_engine.dylib 2>/dev/null || true
	@echo "Copied shared library to backend/."

test: build
	@echo "Running C engine test suite..."
	$(MAKE) -C c_engine test
	@echo "Running backend pytest suite..."
	@if [ -x .venv/bin/pytest ]; then .venv/bin/pytest backend/tests/test_api.py -v; else python3 -m pytest backend/tests/test_api.py -v; fi

run: build
	@echo "Starting Student Assignment Priority Manager on http://$(HOST):$(PORT)..."
	gunicorn --bind $(HOST):$(PORT) --workers $(WORKERS) --threads $(THREADS) "backend.app:create_app()"

clean:
	$(MAKE) -C c_engine clean
	rm -rf .pytest_cache
