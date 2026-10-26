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

test: build
	@echo "Running C engine test suite..."
	$(MAKE) -C c_engine test
	@echo "Running backend pytest suite..."
	python3 -m pytest backend/tests/test_api.py -v

run: build
	@echo "Starting Student Assignment Priority Manager on http://$(HOST):$(PORT)..."
	gunicorn --bind $(HOST):$(PORT) --workers $(WORKERS) --threads $(THREADS) "backend.app:create_app()"

clean:
	$(MAKE) -C c_engine clean
	rm -rf .pytest_cache
