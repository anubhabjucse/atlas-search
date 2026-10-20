
# ============================================================
# Stage 1: Build the Atlas C++ search worker
# ============================================================
FROM ubuntu:24.04 AS cpp-builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libicu-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /atlas

COPY CMakeLists.txt ./
COPY search_engine ./search_engine
COPY api/database_adapter/include ./api/database_adapter/include
COPY api/database_adapter/src ./api/database_adapter/src
COPY api/database_adapter/tests ./api/database_adapter/tests

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --target atlas_search_worker --parallel


# ============================================================
# Stage 2: Combined Atlas API + EmbeddingGemma runtime
# ============================================================
FROM ghcr.io/ggml-org/llama.cpp:server AS runtime

ENV DEBIAN_FRONTEND=noninteractive
ENV NODE_ENV=production

RUN apt-get update && apt-get install -y \
    curl \
    ca-certificates \
    libicu74 \
    bash \
    && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL https://deb.nodesource.com/setup_22.x | bash - \
    && apt-get install -y nodejs \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /atlas

# Atlas C++ worker
COPY --from=cpp-builder \
    /atlas/build/atlas_search_worker \
    /atlas/build/atlas_search_worker

# Node backend and database adapter
COPY api/backend/package.json \
    api/backend/package-lock.json \
    ./api/backend/

COPY api/database_adapter/package.json \
    api/database_adapter/package-lock.json \
    ./api/database_adapter/

RUN cd api/database_adapter && npm ci --omit=dev \
    && cd ../backend && npm ci --omit=dev

COPY api/database_adapter/index.js ./api/database_adapter/index.js

# ============================================================
# Lexical index + compact INT8 vector index
# ============================================================
RUN mkdir -p /atlas/data/index /models

RUN curl -fL --retry 3 \
    -o /atlas/data/index/news.atlas \
    https://github.com/anubhabjucse/atlas-search/releases/download/v4.0-index/news.atlas

RUN curl -fL --retry 3 \
    -o /atlas/data/index/news.vector.int8 \
    https://github.com/anubhabjucse/atlas-search/releases/download/v5.2.0-int8-index/news.vector.int8

# EmbeddingGemma model
RUN curl -fL --retry 3 \
    -o /models/embeddinggemma-300M-Q8_0.gguf \
    https://huggingface.co/ggml-org/embeddinggemma-300M-GGUF/resolve/main/embeddinggemma-300M-Q8_0.gguf

COPY api/backend/src ./api/backend/src
COPY api/database_adapter/src ./api/database_adapter/src
COPY api/frontend/package.json ./api/frontend/package.json
COPY api/frontend/api ./api/frontend/api

# ============================================================
# Runtime configuration
# ============================================================
ENV ATLAS_WORKER_PATH=/atlas/build/atlas_search_worker
ENV ATLAS_INDEX_PATH=/atlas/data/index/news.atlas
ENV ATLAS_VECTOR_INDEX_PATH=/atlas/data/index/news.vector.int8
ENV ATLAS_EMBEDDING_URL=http://127.0.0.1:8080
ENV ATLAS_RUNTIME_PATH=
ENV PORT=3000

COPY docker/start-atlas.sh /atlas/docker/start-atlas.sh
RUN chmod +x /atlas/docker/start-atlas.sh

WORKDIR /atlas/api/backend

EXPOSE 3000

# Override the llama.cpp base image entrypoint so we can launch
# both llama-server and the Atlas Node API.
ENTRYPOINT ["/bin/bash", "/atlas/docker/start-atlas.sh"]
