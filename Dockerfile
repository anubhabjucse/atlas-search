FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libicu-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /atlas

COPY . .

RUN cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release

RUN cmake --build build \
    --target atlas_search_worker atlas_index_builder \
    --parallel