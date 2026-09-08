# ============================================================
# Stage 1: Build Atlas C++ search engine
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

RUN cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release

RUN cmake --build build \
    --target atlas_search_worker \
    --parallel


# ============================================================
# Stage 2: Runtime
# ============================================================

# ============================================================
# Stage 2: Runtime
# ============================================================

FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive
ENV NODE_ENV=production

RUN apt-get update && apt-get install -y \
    curl \
    ca-certificates \
    libicu74 \
    && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL https://deb.nodesource.com/setup_22.x | bash - \
    && apt-get install -y nodejs \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /atlas

# Atlas Linux search worker
COPY --from=cpp-builder \
    /atlas/build/atlas_search_worker \
    /atlas/build/atlas_search_worker

# Node backend + database adapter
COPY api/backend/package.json \
    api/backend/package-lock.json \
    ./api/backend/

COPY api/database_adapter/package.json \
    api/database_adapter/package-lock.json \
    ./api/database_adapter/

RUN cd api/database_adapter && npm ci --omit=dev

RUN cd api/backend && npm ci --omit=dev

COPY api/backend/src ./api/backend/src
COPY api/database_adapter/index.js ./api/database_adapter/index.js
COPY api/database_adapter/src ./api/database_adapter/src
# Production search index
RUN mkdir -p ./data/index \
    && curl -L \
    -o ./data/index/news.atlas \
    https://github.com/anubhabjucse/atlas-search/releases/download/v4.0-index/news.atlas

ENV ATLAS_WORKER_PATH=./build/atlas_search_worker
ENV ATLAS_INDEX_PATH=./data/index/news.atlas
ENV ATLAS_RUNTIME_PATH=

WORKDIR /atlas/api/backend

EXPOSE 3000

CMD ["npm", "start"]