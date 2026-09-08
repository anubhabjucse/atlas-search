# Atlas

A from-scratch information-retrieval engine targeting a 10M-document benchmark
and a measured study of how the architecture scales toward web-scale workloads.

## Current status

**Phase 0 — Tiny IR engine**

Current components:
- `search_engine`: reusable Unicode-aware indexing and retrieval library
- `database_adapter`: data-source integration layer with a text-file adapter

The search engine is independent of storage and database technology.
Adapters load documents from their source and call `SearchEngine::index_document`.
New database integrations can therefore be added without changing retrieval
algorithms or search-engine tests.

## Central project context

See [`architecture.md`](architecture.md).

## Build

Atlas currently requires:
- C++20
- CMake 3.20+
- ICU4C

## Modules

```text
search_engine/
	include/  Public search API and retrieval components
	src/      Tokenization, indexing, parsing, retrieval, ranking
	tests/    Search-engine unit and end-to-end tests

database_adapter/
	data/     Sample documents for the file adapter
	include/  Database adapter interfaces
	src/      Data-source implementations
	tests/    Adapter integration tests
```

`search_engine` builds as a standalone library. `database_adapter` links to
it and owns the responsibility of translating database records into indexed
documents.

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```
