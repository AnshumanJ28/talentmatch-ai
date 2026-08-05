# ============================================================================
# TalentMatch AI v2 — Multi-stage Dockerfile
#
# Stage 1: C++ Builder
#   - Installs cmake, g++, git
#   - Downloads nlohmann/json via FetchContent
#   - Builds libtalentmatch.so
#
# Stage 2: Python Runtime
#   - Copies libtalentmatch.so from builder
#   - Installs Python dependencies
#   - Pre-downloads the embedding model
#   - Runs the FastAPI server
# ============================================================================

# --- Stage 1: C++ builder ------------------------------------------------
FROM ubuntu:22.04 AS cpp_builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    build-essential \
    git \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

COPY cpp_core/ /build/cpp_core/

RUN mkdir -p /build/cpp_core/build \
 && cmake -S /build/cpp_core -B /build/cpp_core/build \
           -DCMAKE_BUILD_TYPE=Release \
           -DENABLE_XGBOOST=OFF \
 && cmake --build /build/cpp_core/build --config Release -j$(nproc)

# --- Stage 2: Python runtime ---------------------------------------------
FROM python:3.11-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    libgl1 libglib2.0-0 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

ENV HF_HOME=/app/.cache

# Copy built C++ library
COPY --from=cpp_builder /build/cpp_core/build/talentmatch.so /app/cpp_core/build/

# Install Python dependencies
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Pre-download embedding model at build time (avoid cold-start delay)
RUN python -c "from sentence_transformers import SentenceTransformer; \
               SentenceTransformer('sentence-transformers/all-MiniLM-L6-v2')"

# Copy application code
COPY src/ ./src/
COPY api.py .
COPY models/ ./models/

ENV DATA_DIR=/app/data
ENV TALENTMATCH_LIB_PATH=/app/cpp_core/build/talentmatch.so

RUN mkdir -p /app/data

EXPOSE 8000
CMD ["uvicorn", "api:app", "--host", "0.0.0.0", "--port", "8000"]
