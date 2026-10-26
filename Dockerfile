FROM python:3.12-slim

# Install C compiler and build tools
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    gcc \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy application files
COPY . /app

# Build C core engine shared library
RUN make -C c_engine shared_lib

# Install Python dependencies
RUN pip install --no-cache-dir -r backend/requirements.txt

# Create directory for persistent assignment storage
RUN mkdir -p /data

# Default environment configuration
ENV PORT=5000 \
    HOST=0.0.0.0 \
    SAPM_DATA_FILEPATH=/data/assignments_data.csv \
    SAPM_C_LIB_PATH=/app/c_engine/bin/libc_engine.so \
    PYTHONUNBUFFERED=1

EXPOSE 5000

# Start WSGI server bound to $PORT
CMD ["sh", "-c", "gunicorn --bind 0.0.0.0:${PORT:-5000} --workers 1 --threads 4 'backend.app:create_app()'"]
