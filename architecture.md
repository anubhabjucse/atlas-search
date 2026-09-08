# Atlas Architecture

> Canonical project context. Update this file when a major
> architectural, algorithmic, dependency, benchmark, or design decision
> changes.

## Project

**Atlas** is a high-performance keyword information-retrieval engine.

-   Primary physical benchmark: **10 million real documents**
-   Larger-scale target: **100M → 1B → 10B**, studied through
    simulation, storage calculations, architecture, and workload
    experiments rather than claimed as physically indexed data.
-   Engineering loop: **learn → implement → benchmark → understand →
    improve**

For major techniques, Atlas will understand the baseline, study relevant
material, implement where practical, test correctness, benchmark against
the baseline, analyze results, and pursue improvements only when
evidence justifies them.

## Roadmap

  -----------------------------------------------------------------------
  Phase                   Scope                   Exit condition
  ----------------------- ----------------------- -----------------------
  0                       Tiny IR engine          Boolean retrieval works
                                                  locally

  1                       Ranked retrieval        TF-IDF and BM25 work
                                                  and are benchmarked

  2                       Index efficiency        Compression/skips
                                                  implemented and
                                                  measured

  3                       Top-K retrieval         WAND / Block-Max WAND
                                                  implemented and
                                                  measured

  4                       Single-node performance Concurrent serving with
                                                  QPS/p50/p95/p99

  5                       Distributed retrieval   Sharding +
                                                  coordinator + global
                                                  top-K

  6                       Incremental indexing    Add/update/delete with
                                                  segments and merges

  7                       Evaluation              Reproducible quality +
                                                  systems benchmark suite

  8                       Scale                   1M → 10M real-document
                                                  benchmark

  9                       Research                Evidence-driven
                                                  Atlas-specific
                                                  experiments

  10                      Web-scale study         100M → 1B → 10B scaling
                                                  analysis
  -----------------------------------------------------------------------

### Infrastructure boundary

AWS begins **after the 100K-document local benchmark**.

Before 100K: local development, correctness testing, and performance
benchmarking.

After 100K: 1. Deploy the same single-node Atlas to AWS. 2. Introduce
distributed infrastructure afterward.

This keeps cloud/distributed debugging separate from early search-engine
correctness work.

## Current State: V0

The V0 pipeline is:

``` text
Document
  ↓
Tokenizer
  ↓
Inverted Index
  ↓
Query Parser → Boolean Retriever
  ↓
Results
```

V0 consists of four independent core components coordinated by
`SearchEngine`:

-   `Tokenizer`
-   `Index` / `InMemoryIndex`
-   `QueryParser`
-   `BooleanRetriever`

`SearchEngine` is the public application-level facade. It coordinates
indexing and querying but does not contain the underlying algorithms,
preserving independent testability.

### Module boundary

Atlas is split into two modules:

``` text
database_adapter
  | loads documents from a database or other source
  v
SearchEngine::index_document
  |
  v
search_engine
  |- Tokenizer
  |- Index
  |- QueryParser
  |- BooleanRetriever
  `- Ranking components
```

`search_engine` has no dependency on a database adapter. It is built as the
`search_engine` CMake library and owns its headers, implementation, and unit
tests under `search_engine/`. `database_adapter` is a separate CMake library
that links to `search_engine`; its adapters own source-specific loading and
call the stable `SearchEngine` API. The initial `TextFileDatabaseAdapter`
loads the sample corpus from `database_adapter/data/`.

------------------------------------------------------------------------

## Tokenization

Atlas uses a **production-oriented, Unicode-aware tokenizer from day 1**
rather than a toy whitespace tokenizer.

### Implementation

Use **ICU4C** for:

-   Unicode word-boundary analysis via `BreakIterator`
-   Unicode-aware segmentation
-   dictionary-backed word breaking where ICU supports it
-   `NFKC_Casefold` normalization

ICU is infrastructure for tokenization, not the retrieval engine.

### Token API

The tokenizer initially exposes:

``` text
Token {
    normalized_term
    position
}
```

Positions are retained from day 1 for future phrase queries and
positional retrieval.

Original-text character offsets are not part of the public API yet
because Unicode normalization can change representation and requires an
explicit offset-mapping design.

### Deliberately excluded from V0

-   stemming
-   stopword removal
-   synonym expansion
-   language-specific ranking
-   query expansion

These remain separate retrieval-policy decisions to evaluate later.

------------------------------------------------------------------------

## Indexing

The first index is `InMemoryIndex`, behind the stable `Index` interface.

``` text
term → vector<Posting>
```

Each posting contains:

``` text
(document_id, term_frequency)
```

Repeated occurrences within a document are aggregated:

``` text
"cat cat dog"

cat → (document_id, 2)
dog → (document_id, 1)
```

Term frequency is retained because later TF-IDF and BM25 ranking require
it.

Postings are maintained in increasing `DocumentId` order. V0 requires
documents to be indexed with **strictly increasing IDs**, preserving
sorted postings naturally and establishing an invariant useful for
future intersection, skipping, compression, and top-K processing.

The index also owns the V0 document-ID universe and exposes:

``` text
max_document_id()
```

This lets `NOT` operate without requiring the caller to supply the
document universe.

V0 keeps the complete index in memory. Persistence, compression, memory
mapping, immutable segments, and distributed shards are future
implementations and should not require changes to tokenization or query
logic.

### Module boundary

``` text
Tokenizer
    │
    │ vector<Token>
    ▼
Index interface
    │
    ▼
InMemoryIndex
    │
    ▼
Dictionary
    │
    └── term → vector<Posting>
                  ├── document_id
                  └── term_frequency
```

------------------------------------------------------------------------

## Query Representation

Query parsing is separate from execution.

`QueryParser` converts query text into a canonical `QueryNode` AST. It
does not access the index or execute retrieval.

V0 supports:

-   terms
-   `AND`
-   `OR`
-   `NOT`
-   parentheses

Precedence:

``` text
NOT > AND > OR
```

The AST uses `unique_ptr` ownership for child nodes.

Example:

``` text
cat OR dog AND bird
```

is parsed as:

``` text
       OR
      /  \
    cat   AND
         /   \
       dog   bird
```

The canonical AST establishes a stable boundary between query
understanding and retrieval execution. Future rule-based parsers or
optional model-assisted query understanding can produce the same
representation without changing the retrieval engine.

------------------------------------------------------------------------

## Boolean Retrieval

`BooleanRetriever` depends on the abstract `Index` interface rather than
directly on `InMemoryIndex`.

The index owns postings; the retriever owns query-time operations.

V0 operations:

-   `AND` → postings/result-set intersection
-   `OR` → postings/result-set union
-   `NOT` → complement against the index's document-ID universe

Postings are sorted, so AND and OR use linear two-pointer merge
algorithms.

Boolean primitives operate on sorted `ResultSet` objects rather than
being tied to individual terms. This allows recursive evaluation of
arbitrary Boolean ASTs without duplicating intersection/union/complement
logic.

``` text
Query
  ↓
BooleanRetriever
  ↓
Index interface
  ↓
InMemoryIndex
  ↓
Postings
```

The V0 `NOT` implementation assumes the document universe is contiguous:

``` text
1..max_document_id
```

This is a temporary simplification and is not the intended large-scale
design.

------------------------------------------------------------------------

## End-to-End V0

``` text
Document
  │
  ▼
Tokenizer
  │
  ▼
Tokens
  │
  ▼
InMemoryIndex
  │
  ▼
Postings

Query
  │
  ▼
QueryParser
  │
  ▼
QueryNode AST
  │
  ▼
BooleanRetriever
  │
  ▼
ResultSet
```

`SearchEngine` coordinates these components while keeping their
responsibilities separate.

------------------------------------------------------------------------

## Repository

``` text
atlas/
├── architecture.md
├── CMakeLists.txt
├── README.md
├── search_engine/
│   ├── include/atlas/     Public search-engine headers
│   ├── src/               Search-engine implementations
│   └── tests/              Search-engine unit and end-to-end tests
└── database_adapter/
    ├── data/tiny/         Sample source documents
    ├── include/atlas/     Database-adapter interfaces
    ├── src/               Database-adapter implementations
    └── tests/              Adapter integration tests
```

## Dependencies

-   C++20
-   CMake
-   ICU4C

The `search_engine` library contains the core indexing, retrieval, ranking,
and top-K components. The `database_adapter` library links to it and owns
source-specific document loading. The initial adapter is
`TextFileDatabaseAdapter`, which reads files from `database_adapter/data/`.

Python may later support dataset preparation, benchmark orchestration,
evaluation, plotting, and experiment analysis.

## Current Work

1.  Complete the ranked-search facade by combining candidate generation,
  ranking, and Top-K selection.
2.  Add database-specific integrations under `database_adapter` without
  changing the `search_engine` library.
3.  Benchmark the V0 and ranked-retrieval baselines.

## Decision Record

### 2026-09-01 --- Canonical architecture file

`architecture.md` is the canonical project-context file and should be
updated when architecture or major technical decisions change.

### 2026-09-01 --- Production-oriented tokenizer from day 1

Atlas starts with a Unicode-aware ICU4C tokenizer rather than a naive
whitespace tokenizer. Additional tokenization behavior is added only
when benchmarks or retrieval requirements justify it.

### 2026-09-01 --- AWS after 100K

AWS infrastructure begins after the 100K local benchmark, with
single-node deployment first and distributed sharding later.

### 2026-09-01 --- Term frequency retained in V0

Postings store `(document_id, term_frequency)` instead of only document
IDs, preserving information required by TF-IDF/BM25 while keeping
Boolean retrieval simple.

### 2026-09-01 --- Stable index abstraction

Retrieval depends on the `Index` interface rather than `InMemoryIndex`,
allowing future persistent, compressed, segmented, or distributed
implementations without coupling storage changes to tokenization or
query processing.

### 2026-09-01 --- Boolean retrieval separated from index storage

`BooleanRetriever` owns query execution while the index owns postings
and the document universe. This keeps storage and retrieval independent.

### 2026-09-01 --- Sorted postings

Postings remain in increasing `DocumentId` order, enabling linear
two-pointer Boolean operations and establishing an invariant for future
skipping, compression, and top-K retrieval.

### 2026-09-01 --- Query parsing separated from retrieval

`QueryParser` produces a canonical AST without accessing index storage
or executing queries, creating a stable boundary for future
query-understanding systems.

### 2026-09-01 --- SearchEngine facade

`SearchEngine` is the public facade coordinating tokenization, indexing,
parsing, and Boolean retrieval without absorbing their algorithms.

### 2026-09-01 --- Index owns the document universe

`max_document_id()` is exposed by the index so callers do not supply the
document universe to `NOT`.

### 2026-09-01 --- Monotonically increasing document IDs

V0 requires strictly increasing document IDs to maintain sorted postings
naturally and support future intersection, skipping, compression, and
top-K processing.

### 2026-09-03 --- Search engine and database adapter modules

The search engine is isolated under `search_engine/` and builds as the
reusable `search_engine` library. Database-specific loading is isolated
under `database_adapter/`, which builds as a separate library linked to the
search engine. `TextFileDatabaseAdapter` reads source documents and indexes
them through `SearchEngine::index_document`. The adapter integration test
verifies this boundary independently from the search-engine tests.
## 29. V1.1 — Document and Corpus Statistics

V1.1 extends the existing V0 index with the statistics required for
TF-IDF and BM25.

### Changes

Added document/corpus statistics to the `Index` abstraction:

- document count
- document length per document
- total document length
- average document length
- document frequency per term

The existing V0 inverted-index and posting structures are preserved.

### Statistics

For each indexed document:

```text
document length = number of analyzed tokens

The index maintains:

document_count
document_length(document_id)
total_document_length
average_document_length

Document frequency is obtained from the existing posting list:

DF(term) = postings(term).size()

Because each term/document pair has exactly one posting, posting-list
size is equal to the number of documents containing the term.

Architecture Decision

Statistics are added to the existing InMemoryIndex without
changing the V0 posting representation.

The existing:

dictionary
    ↓
term
    ↓
postings
    ↓
(document_id, term_frequency)

structure remains unchanged.

Document lengths are maintained separately:

document_lengths
    ↓
document_id → document length

Corpus-level statistics are maintained incrementally during indexing.

Reasoning

The statistics are required by the upcoming ranking stages:

V1.1 Statistics
       ↓
V1.2 TF-IDF
       ↓
V1.3 BM25

Maintaining them during indexing avoids repeatedly scanning the entire
corpus when calculating ranking statistics.

Document frequency is not duplicated in a separate data structure yet,
because it can currently be obtained directly from the existing
postings.

This decision can be revisited later using profiling and benchmarking.

V0 Invariants Preserved
Document IDs remain strictly increasing.
Postings remain sorted by document ID.
Each term/document pair has one posting.
Term frequency remains available.
Existing Boolean retrieval semantics remain unchanged.
Tokenization remains independent of indexing.
The Index abstraction remains the boundary used by retrieval.
Tests Added

V1.1 tests cover:

document count
individual document length
unknown document lookup
total corpus length
average document length
document frequency
unknown-term document frequency

All existing V0 tests must continue to pass.

Current Repository Structure
atlas/
├── architecture.md
├── CMakeLists.txt
├── README.md
├── search_engine/
│   ├── include/atlas/     Public engine headers
│   ├── src/               Engine implementations
│   └── tests/             Engine unit and end-to-end tests
└── database_adapter/
    ├── data/tiny/         Sample source documents
    ├── include/atlas/     Adapter interfaces
    ├── src/               Adapter implementations
    └── tests/              Adapter integration tests
Status

V1.1 implementation complete. The repository is currently implementing
the V1.2 ranking and V1.3 top-K components.

Next:

V1.2 — TF-IDF baseline

TF-IDF will be implemented as a separate ranking component and will
not be placed inside the index.

## 30. V1.2 — TF-IDF and BM25 Ranking

V1.2 introduces the first ranking models.

Both TF-IDF and BM25 operate on the existing `Index` abstraction and
remain independent of storage, tokenization, query parsing, and Boolean
retrieval.

### Ranking Architecture

```text
Index
  │
  ├── postings
  ├── term frequency
  ├── document frequency
  ├── document length
  └── corpus statistics
          │
          ▼
    RankingModel
       ├── TFIDFRanker
       └── BM25Ranker

The ranking layer does not modify the index.

RankingModel

A common RankingModel interface was introduced.

class RankingModel {
public:
    virtual ~RankingModel() = default;

    virtual double score(
        DocumentId document_id,
        const QueryTerms& query_terms
    ) const = 0;
};

This establishes a stable boundary for future ranking models.

TF-IDF

The first TF-IDF implementation uses:

TF(t,d) = f(t,d)

IDF(t) = log(N / DF(t))

score(D,Q)
    = Σ TF(t,D) × IDF(t)

This is intentionally a simple baseline.

The purpose is to establish a correct and measurable lexical ranking
baseline before introducing additional weighting or normalization.

BM25

BM25 uses:

score(D,Q)
=
Σ IDF(t)
×
[ f(t,D)(k1+1) ]
/
[ f(t,D) + k1(1-b+b|D|/avgdl) ]

Default parameters:

k1 = 1.2
b  = 0.75

These are baseline values rather than permanent choices.

Future experiments may benchmark alternative values.

Architecture Decision

TF-IDF and BM25 are implemented as separate classes behind the common
RankingModel interface.

Reason:

ranking must remain independent of storage
multiple ranking algorithms should be interchangeable
future ranking models should not require changes to the index
retrieval and ranking should remain independently testable
later Top-K optimization should operate above the ranking abstraction
Current Query Boundary

V1.2 does not yet modify SearchEngine.

Rankers currently accept a set of query terms directly:

QueryTerms
    ↓
RankingModel
    ↓
document score

This avoids coupling the first ranking implementation to the existing
Boolean AST.

The eventual search pipeline is expected to become:

Query
  ↓
Query understanding / parsing
  ↓
Canonical query representation
  ↓
Candidate retrieval
  ↓
RankingModel
  ↓
Top-K
Important Implementation Note

The current V1.2 implementation scores a specified document against
the query terms.

It is intentionally not yet responsible for:

candidate generation
Top-K selection
query parsing
query expansion
LLM query understanding
WAND
result ordering across the complete candidate set

Those concerns remain separate components.

Tests

V1.2 tests cover:

TF-IDF scoring
BM25 scoring
term-frequency effects
BM25 document-length normalization
missing query terms
positive scoring for matching documents
Current Repository Structure
atlas/
├── architecture.md
├── CMakeLists.txt
├── README.md
├── search_engine/
│   ├── include/atlas/     Public engine headers
│   ├── src/               Engine implementations
│   └── tests/             Engine unit and end-to-end tests
└── database_adapter/
    ├── data/tiny/         Sample source documents
    ├── include/atlas/     Adapter interfaces
    ├── src/               Adapter implementations
    └── tests/              Adapter integration tests
Status

V1.2 implements the initial TF-IDF and BM25 ranking models. V1.3 adds
the Top-K component and its test; the adapter integration test verifies
that source documents can be indexed through the public search API.

Next:

V2 — Index efficiency

Future ranking work will build on this abstraction rather than
embedding ranking logic inside the index or search engine.


---

## One important learning point

There's a subtle thing I want you to understand before we move on.

Our current implementation does:

```cpp
ranker.score(document_id, query_terms);

So it is not yet a complete ranked search API.

That's intentional.

Right now we're establishing the mathematical/ranking layer:

candidate document
        +
query terms
        ↓
     score

Later we'll build:

Boolean / lexical retrieval
          ↓
     candidate set
          ↓
     RankingModel
          ↓
     scored candidates
          ↓
        Top-K

This is exactly why we're keeping the interfaces separate. When we introduce Top-K, WAND, and eventually concurrent serving, we won't have to rip TF-IDF/BM25 apart.

## V1.3 — Candidate Ranking and Top-K Retrieval

### Version

V1.3

### Goal

Complete the first ranked-retrieval pipeline by combining:

- query terms
- candidate generation
- ranking models
- Top-K selection

This iteration does not modify the tokenizer, index structure, Boolean retriever, query parser, TF-IDF implementation, or BM25 implementation.

### Architecture

The V1 ranking path is now:

Query Terms
    ↓
Candidate Generation
    ↓
RankingModel
    ├── TFIDFRanker
    └── BM25Ranker
    ↓
Top-K Selection
    ↓
Ranked Results

### Top-K abstraction

`TopKRetriever` is responsible for selecting the best K scored documents.

The ranking model remains responsible only for calculating a document's score for a query.

This separation is intentional:

- RankingModel answers: "How relevant is this document?"
- TopKRetriever answers: "Which K documents should be returned?"

This keeps ranking algorithms independent from result-selection strategy.

### Candidate generation

Candidates are currently generated by collecting the union of documents appearing in the posting lists for the query terms.

For example:

query:
    distributed consensus

candidate documents:
    union(postings("distributed"),
          postings("consensus"))

This is a deliberately simple V1 implementation.

Later versions can replace this candidate-generation strategy with more efficient posting-list traversal without changing the RankingModel interface.

### Top-K algorithm

Top-K selection uses a min-heap containing at most K results.

For each scored candidate:

1. If fewer than K results are stored, insert the result.
2. Otherwise compare it with the current worst result.
3. Replace the worst result when the new result is better.
4. Sort the final K results in descending score order.

This avoids fully sorting every scored candidate.

The current complexity is approximately:

    O(C log K)

for Top-K maintenance, where C is the number of candidates.

The implementation still scores every candidate. Future retrieval optimizations such as WAND and Block-Max WAND can reduce the number of candidates that need to be fully scored.

### Tie-breaking

When two documents have identical scores, the smaller DocumentId is preferred.

This provides deterministic result ordering.

### Zero-score candidates

Candidates whose final score is zero are not returned.

This keeps the result set focused on documents with an actual scoring contribution.

### Tests

V1.3 tests verify:

- Top-K limits the number of returned documents.
- K larger than the candidate set returns all matching documents.
- Results are ordered by descending score.
- Term frequency can affect ranking.
- K = 0 returns no results.
- Queries with no matching terms return no results.

### Performance status

V1.3 is a correctness-oriented baseline.

Candidate generation currently uses an `unordered_set` and ranking currently scans posting lists to find document term frequencies.

No low-level optimization is being introduced yet.

This is intentional.

The performance engineering sequence remains:

correctness
    ↓
clean abstraction
    ↓
benchmark
    ↓
profile
    ↓
optimize

### Architectural invariant

Ranking algorithms must not own candidate-generation or Top-K policy.

Candidate retrieval, scoring, and result selection remain separate responsibilities.

This allows future versions to introduce:

- efficient posting traversal
- skip structures
- WAND
- Block-Max WAND
- specialized Top-K algorithms
- caching
- concurrency

without rewriting TF-IDF or BM25.

### V1 completion criteria

V1 is complete when Atlas can:

1. Maintain corpus statistics.
2. Calculate TF-IDF scores.
3. Calculate BM25 scores.
4. Generate candidate documents.
5. Rank candidates.
6. Return deterministic Top-K results.

After V1.3, the ranked-retrieval baseline is considered complete.

The next major phase is V2: Efficient Retrieval.
## V2.1 — Posting Cursor and Efficient Advancement

### Version

V2.1

### Goal

Introduce an independent posting-list traversal abstraction that can efficiently
move through sorted posting lists without modifying the underlying `Posting`
representation.

### Architecture

The V2.1 traversal path is:

Posting List
    ↓
PostingCursor
    ├── current()
    ├── next()
    └── advance(target_document_id)

The cursor operates over an existing posting list supplied by the `Index`
interface.

### PostingCursor responsibility

`PostingCursor` owns traversal state, not posting data.

The underlying posting list remains owned by the index and is treated as
read-only during traversal.

This separation is intentional.

A posting list may therefore be traversed independently by multiple search
operations without storing mutable cursor state inside each `Posting`.

### Sequential traversal

`next()` advances the cursor by one posting.

Its complexity is:

    O(1)

### Targeted advancement

`advance(target_document_id)` moves the cursor to the first posting whose
DocumentId is greater than or equal to the target.

Because posting lists are maintained in strictly increasing DocumentId order,
the cursor uses binary search over the remaining posting range.

Its current complexity is approximately:

    O(log N)

where N is the number of remaining postings.

### Why this is useful

Without targeted advancement, reaching a later document requires visiting every
posting between the current position and the target.

With `advance()`, the cursor can jump directly toward the target using the
ordering invariant of the posting list.

This provides the primitive required for future multi-posting-list traversal
algorithms.

### Relationship to WAND

V2.1 does not implement WAND.

The cursor only answers:

    "How can I move efficiently to a target DocumentId?"

WAND will answer a different question:

    "Can this candidate be skipped because it cannot beat the current Top-K
     threshold?"

The distinction is important.

V2.1 establishes the traversal primitive first so that V2.2 can build dynamic
pruning on top of it.

### Relationship to block metadata

V2.1 does not introduce skip pointers, block metadata, or score upper bounds.

Those mechanisms will be introduced in later V2 phases when they are required
by WAND and Block-Max WAND.

### Architectural invariant

Posting traversal state must not be stored inside `Posting`.

`Posting` remains immutable data:

    Posting
        ├── document_id
        └── term_frequency

Traversal state belongs to:

    PostingCursor

This keeps the posting representation independent from query execution and
supports future concurrent search requests.

### Tests

V2.1 tests verify:

- Cursor starts at the first posting.
- Sequential advancement works.
- Targeted advancement reaches an existing document.
- Targeted advancement reaches the first document at or after a target.
- Advancing past the end exhausts the cursor.
- Missing terms produce an empty cursor.
- The cursor never moves backward.
- Cursor traversal does not modify the underlying posting list.

### Performance status

V2.1 introduces the first efficient traversal primitive but does not yet
change the complete Top-K retrieval algorithm.

The current V1 Top-K implementation still scores all generated candidates.

V2.2 will use posting cursors as the foundation for WAND candidate pruning.

### Next step

V2.2 — WAND.
## V2.2 — WAND Dynamic Pruning

### Version

V2.2

### Goal

Introduce WAND (Weak AND) retrieval to avoid fully scoring candidate
documents that cannot possibly enter the current Top-K result set.

### Core idea

The V1 retrieval baseline scores every candidate document.

WAND changes the process to:

candidate
    ↓
estimate maximum possible score
    ↓
compare against Top-K threshold
    ├── cannot win → skip
    └── may win → score

### Architecture

WAND depends on three existing abstractions:

    PostingCursor
    RankingModel
    Top-K selection

The conceptual flow is:

Query Terms
    ↓
Posting Cursors
    ↓
Upper-bound / pivot evaluation
    ↓
Candidate pruning
    ↓
RankingModel::score()
    ↓
Top-K

### RankingModel upper bounds

`RankingModel` now exposes:

    max_score(term)

This returns a safe upper bound on the contribution that a term can make to
a document's score.

WAND uses this value for pruning but does not depend on the internal scoring
formula.

This preserves the ranking abstraction.

### TF-IDF upper bound

For the current TF-IDF implementation:

    max_score(term) =
        maximum observed TF for term × IDF(term)

Because the current index contains the complete posting list, the maximum
observed term frequency provides an exact global upper bound for the current
corpus.

### BM25 upper bound

For the current BM25 formulation, the term-frequency component approaches
`k1 + 1` as TF increases.

Therefore a safe global bound is:

    IDF(term) × (k1 + 1)

This bound intentionally does not attempt to model the exact document-length
maximum.

A looser bound may reduce pruning effectiveness but preserves correctness.

Block-specific bounds will be introduced in V2.3.

### WAND cursor coordination

WAND maintains one `PostingCursor` for each query term.

Cursors are ordered by their current DocumentId.

The algorithm accumulates term upper bounds until the cumulative bound reaches
the current Top-K threshold.

The cursor where this happens becomes the pivot.

If the smallest current DocumentId is behind the pivot, cursors can be advanced
toward the pivot using:

    PostingCursor::advance()

If the smallest DocumentId equals the pivot DocumentId, the candidate is
potentially competitive and is fully scored.

### Top-K threshold

The Top-K heap provides the current minimum score among the retained results.

Before K results exist, the threshold is zero.

After K results exist, the threshold becomes the score of the worst retained
result.

As stronger results are found, the threshold rises, allowing WAND to prune
more aggressively.

### Correctness principle

Upper bounds must never underestimate a document's possible score.

A bound that is too high reduces pruning efficiency.

A bound that is too low can incorrectly eliminate a document that belongs in
Top-K and therefore violates retrieval correctness.

V2.2 therefore prioritizes safe bounds over maximally tight bounds.

### Relationship to V2.1

V2.1 introduced:

    PostingCursor::advance()

V2.2 uses that primitive for dynamic pruning.

V2.1 answers:

    "How can a posting cursor move toward a target efficiently?"

V2.2 answers:

    "Which candidates can be skipped because they cannot beat Top-K?"

### Tests

WAND tests verify:

- WAND produces the same Top-K results as exhaustive retrieval.
- BM25 works through WAND.
- TF-IDF works through WAND.
- Top-K limits are respected.
- Missing terms produce empty results.
- Empty queries produce empty results.
- K = 0 produces empty results.

### Current limitation

V2.2 still uses global term-level upper bounds.

It does not yet maintain block-specific maximum score contributions.

Therefore WAND may still inspect candidates that a tighter bound could eliminate.

V2.3 will introduce Block-Max WAND.

### Next step

V2.3 — Block-Max WAND.
## V2.2 Top-K Correctness Invariant

WAND uses `TopKRetriever` as the correctness baseline, so the ordering semantics of Top-K must be deterministic and explicitly defined.

Atlas ranking order is:

1. Higher score is better.
2. When scores are equal, smaller `DocumentId` is better.

The Top-K heap stores the current best `K` results, while `top()` exposes the worst retained result.

Therefore the heap ordering must satisfy:

```text
worse result:
    lower score

if scores are equal:
    larger DocumentId