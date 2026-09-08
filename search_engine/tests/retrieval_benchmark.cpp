#include "atlas/bm25_ranker.hpp"
#include "atlas/block_max_wand_retriever.hpp"
#include "atlas/index.hpp"
#include "atlas/retrieval_stats.hpp"
#include "atlas/tfidf_ranker.hpp"
#include "atlas/tokenizer.hpp"
#include "atlas/top_k.hpp"
#include "atlas/wand_retriever.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace {

using atlas::BM25Ranker;
using atlas::BlockMaxWANDRetriever;
using atlas::DocumentId;
using atlas::InMemoryIndex;
using atlas::Posting;
using atlas::QueryTerms;
using atlas::RetrievalStats;
using atlas::ScoredResult;
using atlas::TFIDFRanker;
using atlas::Token;
using atlas::Tokenizer;
using atlas::TopKRetriever;
using atlas::WANDRetriever;

constexpr std::size_t kDefaultIterations = 20;
constexpr std::size_t kDefaultWarmup = 5;

constexpr std::size_t kDefaultDocuments[] = {
    5000,
    10000,
    50000,
    100000,
    210000
};

constexpr std::size_t kDefaultBlockSizes[] = {
    64,
    128,
    256
};

constexpr std::size_t kDefaultKs[] = {
    1,
    10,
    50
};

constexpr std::size_t kQueriesPerBucket = 8;

const char* kDefaultDatasetPath =
    "database_adapter/data/news/News_Category_Dataset_v3.json";

const char* kDefaultOutputPath = "benchmark.txt";

struct QuerySpec {
    std::string name;
    std::string category;
    std::vector<std::string> terms;
};

struct BenchmarkConfig {
    std::string dataset_path = kDefaultDatasetPath;
    std::string output_path = kDefaultOutputPath;

    std::size_t iterations = kDefaultIterations;
    std::size_t warmup = kDefaultWarmup;

    std::vector<std::size_t> document_counts = {
        std::begin(kDefaultDocuments),
        std::end(kDefaultDocuments)
    };

    std::vector<std::size_t> block_sizes = {
        std::begin(kDefaultBlockSizes),
        std::end(kDefaultBlockSizes)
    };

    std::vector<std::size_t> ks = {
        std::begin(kDefaultKs),
        std::end(kDefaultKs)
    };
};

struct Corpus {
    InMemoryIndex index;
    std::vector<QuerySpec> queries;
    std::size_t documents_loaded = 0;
};

struct TimingSummary {
    double mean_us = 0.0;
    double p50_us = 0.0;
    double p95_us = 0.0;
    double p99_us = 0.0;
    double throughput_qps = 0.0;
    double cpu_seconds = 0.0;

    std::size_t documents_scored = 0;
    std::size_t postings_visited = 0;
    std::size_t blocks_skipped = 0;
};

enum class Algorithm {
    Exhaustive,
    WAND,
    BlockMaxWAND
};

const char* algorithm_name(Algorithm algorithm) {
    switch (algorithm) {
    case Algorithm::Exhaustive:
        return "Exhaustive";
    case Algorithm::WAND:
        return "WAND";
    case Algorithm::BlockMaxWAND:
        return "Block-Max WAND";
    }

    return "Unknown";
}

void print_usage() {
    std::cout
        << "Atlas retrieval benchmark\n\n"
        << "Options:\n"
        << "  --dataset-path <path>   News JSON/JSONL dataset path\n"
        << "  --documents <n,...>     Document counts to benchmark\n"
        << "  --blocks <n,...>        Block sizes\n"
        << "  --k <n,...>             Top-K values\n"
        << "  --iterations <n>        Timed iterations\n"
        << "  --warmup <n>            Warmup iterations\n"
        << "  --output <path>         Output file\n"
        << "  --help                  Show this help\n\n"
        << "Defaults:\n"
        << "  dataset: " << kDefaultDatasetPath << "\n"
        << "  documents: 5000,10000,50000,100000,210000\n"
        << "  blocks: 64,128,256\n"
        << "  K: 1,10,50\n"
        << "  iterations: 20\n"
        << "  warmup: 5\n"
        << "  output: benchmark.txt\n";
}

std::vector<std::size_t> parse_size_list(
    const std::string& value
) {
    std::vector<std::size_t> result;

    std::stringstream stream(value);
    std::string item;

    while (std::getline(stream, item, ',')) {
        if (item.empty()) {
            continue;
        }

        const unsigned long long parsed =
            std::stoull(item);

        if (parsed == 0) {
            throw std::invalid_argument(
                "numeric values must be greater than zero"
            );
        }

        result.push_back(
            static_cast<std::size_t>(parsed)
        );
    }

    if (result.empty()) {
        throw std::invalid_argument(
            "empty numeric list"
        );
    }

    return result;
}

/*
 * The dataset is JSON Lines:
 *
 * {"link":"...","headline":"...","category":"...",...}
 *
 * We only need the headline field. This parser deliberately
 * avoids introducing a third-party JSON dependency into the
 * benchmark executable.
 */
bool extract_json_string(
    const std::string& line,
    std::string_view key,
    std::string& value
) {
    const std::string key_pattern =
        "\"" + std::string(key) + "\"";

    const std::size_t key_position =
        line.find(key_pattern);

    if (key_position == std::string::npos) {
        return false;
    }

    std::size_t colon =
        line.find(':', key_position + key_pattern.size());

    if (colon == std::string::npos) {
        return false;
    }

    std::size_t start = colon + 1;

    while (
        start < line.size() &&
        std::isspace(
            static_cast<unsigned char>(line[start])
        )
    ) {
        ++start;
    }

    if (start >= line.size() || line[start] != '"') {
        return false;
    }

    ++start;

    std::string result;
    result.reserve(128);

    bool escaped = false;

    for (std::size_t i = start; i < line.size(); ++i) {
        const char c = line[i];

        if (!escaped) {
            if (c == '"') {
                value = std::move(result);
                return true;
            }

            if (c == '\\') {
                escaped = true;
                continue;
            }

            result.push_back(c);
            continue;
        }

        escaped = false;

        switch (c) {
        case '"':
            result.push_back('"');
            break;

        case '\\':
            result.push_back('\\');
            break;

        case '/':
            result.push_back('/');
            break;

        case 'b':
            result.push_back('\b');
            break;

        case 'f':
            result.push_back('\f');
            break;

        case 'n':
            result.push_back('\n');
            break;

        case 'r':
            result.push_back('\r');
            break;

        case 't':
            result.push_back('\t');
            break;

        case 'u': {
            /*
             * Decode basic JSON \uXXXX escapes into UTF-8.
             * This is sufficient for the benchmark dataset
             * and avoids a JSON library dependency.
             */
            if (i + 4 >= line.size()) {
                return false;
            }

            unsigned int codepoint = 0;

            for (std::size_t j = 1; j <= 4; ++j) {
                const char hex = line[i + j];

                codepoint <<= 4;

                if (hex >= '0' && hex <= '9') {
                    codepoint +=
                        static_cast<unsigned int>(
                            hex - '0'
                        );
                } else if (
                    hex >= 'a' &&
                    hex <= 'f'
                ) {
                    codepoint +=
                        static_cast<unsigned int>(
                            hex - 'a' + 10
                        );
                } else if (
                    hex >= 'A' &&
                    hex <= 'F'
                ) {
                    codepoint +=
                        static_cast<unsigned int>(
                            hex - 'A' + 10
                        );
                } else {
                    return false;
                }
            }

            i += 4;

            if (codepoint <= 0x7F) {
                result.push_back(
                    static_cast<char>(codepoint)
                );
            } else if (codepoint <= 0x7FF) {
                result.push_back(
                    static_cast<char>(
                        0xC0 | (codepoint >> 6)
                    )
                );

                result.push_back(
                    static_cast<char>(
                        0x80 | (codepoint & 0x3F)
                    )
                );
            } else {
                result.push_back(
                    static_cast<char>(
                        0xE0 | (codepoint >> 12)
                    )
                );

                result.push_back(
                    static_cast<char>(
                        0x80 |
                        ((codepoint >> 6) & 0x3F)
                    )
                );

                result.push_back(
                    static_cast<char>(
                        0x80 | (codepoint & 0x3F)
                    )
                );
            }

            break;
        }

        default:
            /*
             * Unknown JSON escape. Preserve the escaped
             * character rather than silently dropping it.
             */
            result.push_back(c);
            break;
        }
    }

    return false;
}

std::vector<Token> tokenize_document(
    const Tokenizer& tokenizer,
    const std::string& headline
) {
    return tokenizer.tokenize(headline);
}

struct TermFrequencyInfo {
    std::unordered_map<std::string, std::size_t> document_frequency;
};

void collect_document_frequency(
    const std::vector<Token>& tokens,
    TermFrequencyInfo& statistics
) {
    std::unordered_set<std::string> unique_terms;

    unique_terms.reserve(tokens.size());

    for (const Token& token : tokens) {
        unique_terms.insert(token.term);
    }

    for (const std::string& term : unique_terms) {
        ++statistics.document_frequency[term];
    }
}

std::vector<std::string> select_terms_from_range(
    const std::unordered_map<std::string, std::size_t>& df,
    std::size_t document_count,
    double minimum_ratio,
    double maximum_ratio
) {
    struct Candidate {
        std::string term;
        std::size_t df;
    };

    std::vector<Candidate> candidates;

    for (const auto& [term, frequency] : df) {
        const double ratio =
            static_cast<double>(frequency) /
            static_cast<double>(document_count);

        if (
            ratio >= minimum_ratio &&
            ratio < maximum_ratio
        ) {
            candidates.push_back({
                term,
                frequency
            });
        }
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const Candidate& lhs, const Candidate& rhs) {
            if (lhs.df != rhs.df) {
                return lhs.df > rhs.df;
            }

            return lhs.term < rhs.term;
        }
    );

    std::vector<std::string> terms;

    const std::size_t count =
        std::min(
            kQueriesPerBucket,
            candidates.size()
        );

    terms.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        terms.push_back(candidates[i].term);
    }

    return terms;
}

std::vector<std::string> select_rare_terms(
    const std::unordered_map<std::string, std::size_t>& df,
    std::size_t document_count
) {
    struct Candidate {
        std::string term;
        std::size_t df;
    };

    std::vector<Candidate> candidates;

    for (const auto& [term, frequency] : df) {
        const double ratio =
            static_cast<double>(frequency) /
            static_cast<double>(document_count);

        if (ratio > 0.0 && ratio < 0.01) {
            candidates.push_back({
                term,
                frequency
            });
        }
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const Candidate& lhs, const Candidate& rhs) {
            if (lhs.df != rhs.df) {
                return lhs.df < rhs.df;
            }

            return lhs.term < rhs.term;
        }
    );

    std::vector<std::string> terms;

    const std::size_t count =
        std::min(
            kQueriesPerBucket,
            candidates.size()
        );

    terms.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        terms.push_back(candidates[i].term);
    }

    return terms;
}

std::vector<QuerySpec> build_query_workload(
    const TermFrequencyInfo& statistics,
    std::size_t document_count
) {
    std::vector<QuerySpec> workload;

    /*
     * These are workload classes, not arbitrary synthetic terms.
     * Every selected term occurs in the exact corpus being
     * benchmarked.
     */
    const std::vector<std::pair<std::string, std::vector<std::string>>>
        buckets = {
            {
                "high_df",
                select_terms_from_range(
                    statistics.document_frequency,
                    document_count,
                    0.20,
                    1.01
                )
            },
            {
                "medium_df",
                select_terms_from_range(
                    statistics.document_frequency,
                    document_count,
                    0.05,
                    0.20
                )
            },
            {
                "low_df",
                select_terms_from_range(
                    statistics.document_frequency,
                    document_count,
                    0.01,
                    0.05
                )
            },
            {
                "rare_df",
                select_rare_terms(
                    statistics.document_frequency,
                    document_count
                )
            }
        };

    std::size_t query_number = 0;

    for (const auto& [category, terms] : buckets) {
        for (std::size_t i = 0; i < terms.size(); ++i) {
            QuerySpec query;
            query.name =
                category +
                "_" +
                std::to_string(query_number++);

            query.category = category;
            query.terms.push_back(terms[i]);

            workload.push_back(std::move(query));
        }
    }

    /*
     * Build deterministic two-term and four-term queries from
     * the same selected term pools.
     */
    for (const auto& [category, terms] : buckets) {
        if (terms.size() >= 2) {
            for (
                std::size_t i = 0;
                i + 1 < terms.size() &&
                i < kQueriesPerBucket;
                i += 2
            ) {
                QuerySpec query;
                query.name =
                    category +
                    "_2term_" +
                    std::to_string(i / 2);

                query.category = category;
                query.terms = {
                    terms[i],
                    terms[i + 1]
                };

                workload.push_back(std::move(query));
            }
        }

        if (terms.size() >= 4) {
            QuerySpec query;
            query.name =
                category +
                "_4term";

            query.category = category;
            query.terms = {
                terms[0],
                terms[1],
                terms[2],
                terms[3]
            };

            workload.push_back(std::move(query));
        }
    }

    /*
     * The dataset may not contain enough terms in every DF
     * bucket, especially for small corpora. In that case we
     * simply benchmark the workload that actually exists.
     */
    if (workload.empty()) {
        throw std::runtime_error(
            "could not construct any benchmark queries "
            "from the selected news corpus"
        );
    }

    return workload;
}

Corpus load_news_corpus(
    const BenchmarkConfig& config,
    std::size_t requested_documents,
    std::size_t block_size
) {
    std::ifstream input(
        config.dataset_path,
        std::ios::in | std::ios::binary
    );

    if (!input) {
        throw std::runtime_error(
            "unable to open news dataset: " +
            config.dataset_path
        );
    }

    Corpus corpus;
    corpus.index = InMemoryIndex(block_size);

    Tokenizer tokenizer;
    TermFrequencyInfo statistics;

    std::string line;
    DocumentId document_id = 0;

    while (
        document_id < requested_documents &&
        std::getline(input, line)
    ) {
        if (line.empty()) {
            continue;
        }

        std::string headline;

        if (
            !extract_json_string(
                line,
                "headline",
                headline
            )
        ) {
            continue;
        }

        if (headline.empty()) {
            continue;
        }

        std::vector<Token> tokens =
            tokenize_document(
                tokenizer,
                headline
            );

        if (tokens.empty()) {
            continue;
        }

        corpus.index.add(
            document_id,
            tokens
        );

        collect_document_frequency(
            tokens,
            statistics
        );

        ++document_id;
    }

    corpus.documents_loaded =
        static_cast<std::size_t>(document_id);

    if (
        corpus.documents_loaded <
        requested_documents
    ) {
        std::ostringstream message;

        message
            << "requested "
            << requested_documents
            << " documents, but only "
            << corpus.documents_loaded
            << " usable headline documents "
            << "could be loaded";

        throw std::runtime_error(
            message.str()
        );
    }

    corpus.queries =
        build_query_workload(
            statistics,
            corpus.documents_loaded
        );

    return corpus;
}

QueryTerms make_query_terms(
    const QuerySpec& query
) {
    QueryTerms result;

    result.reserve(query.terms.size());

    for (const std::string& term : query.terms) {
        result.push_back(term);
    }

    return result;
}

bool same_results(
    const std::vector<ScoredResult>& lhs,
    const std::vector<ScoredResult>& rhs
) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    constexpr double kScoreEpsilon = 1e-9;

    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (
            lhs[i].document_id !=
            rhs[i].document_id
        ) {
            return false;
        }

        if (
            std::abs(
                lhs[i].score -
                rhs[i].score
            ) > kScoreEpsilon
        ) {
            return false;
        }
    }

    return true;
}

std::vector<ScoredResult> run_algorithm(
    Algorithm algorithm,
    const TopKRetriever& exhaustive,
    const WANDRetriever& wand,
    const BlockMaxWANDRetriever& block_max_wand,
    const QueryTerms& query_terms,
    const atlas::RankingModel& ranking_model,
    std::size_t k,
    RetrievalStats* stats
) {
    switch (algorithm) {
    case Algorithm::Exhaustive:
        return exhaustive.search(
            query_terms,
            ranking_model,
            k,
            stats
        );

    case Algorithm::WAND:
        return wand.search(
            query_terms,
            ranking_model,
            k,
            stats
        );

    case Algorithm::BlockMaxWAND:
        return block_max_wand.search(
            query_terms,
            ranking_model,
            k,
            stats
        );
    }

    return {};
}

void validate_correctness(
    const TopKRetriever& exhaustive,
    const WANDRetriever& wand,
    const BlockMaxWANDRetriever& block_max_wand,
    const std::vector<QuerySpec>& queries,
    const atlas::RankingModel& ranking_model,
    std::size_t k
) {
    for (const QuerySpec& query : queries) {
        const QueryTerms query_terms =
            make_query_terms(query);

        RetrievalStats exhaustive_stats;
        RetrievalStats wand_stats;
        RetrievalStats block_max_stats;

        const auto expected =
            exhaustive.search(
                query_terms,
                ranking_model,
                k,
                &exhaustive_stats
            );

        const auto wand_results =
            wand.search(
                query_terms,
                ranking_model,
                k,
                &wand_stats
            );

        const auto block_max_results =
            block_max_wand.search(
                query_terms,
                ranking_model,
                k,
                &block_max_stats
            );

        if (
            !same_results(
                expected,
                wand_results
            )
        ) {
            throw std::runtime_error(
                "correctness failure: WAND differs "
                "from exhaustive for query '" +
                query.name +
                "'"
            );
        }

        if (
            !same_results(
                expected,
                block_max_results
            )
        ) {
            throw std::runtime_error(
                "correctness failure: Block-Max WAND "
                "differs from exhaustive for query '" +
                query.name +
                "'"
            );
        }
    }
}

double percentile(
    std::vector<double> values,
    double percentile_value
) {
    if (values.empty()) {
        return 0.0;
    }

    std::sort(
        values.begin(),
        values.end()
    );

    const double position =
        percentile_value *
        static_cast<double>(values.size() - 1);

    const std::size_t lower =
        static_cast<std::size_t>(
            position
        );

    const std::size_t upper =
        std::min(
            lower + 1,
            values.size() - 1
        );

    const double fraction =
        position -
        static_cast<double>(lower);

    return
        values[lower] +
        fraction *
        (
            values[upper] -
            values[lower]
        );
}

TimingSummary benchmark_algorithm(
    Algorithm algorithm,
    const TopKRetriever& exhaustive,
    const WANDRetriever& wand,
    const BlockMaxWANDRetriever& block_max_wand,
    const std::vector<QuerySpec>& queries,
    const atlas::RankingModel& ranking_model,
    std::size_t k,
    std::size_t warmup,
    std::size_t iterations
) {
    if (queries.empty()) {
        throw std::runtime_error(
            "benchmark workload is empty"
        );
    }

    /*
     * Warmup.
     */
    for (std::size_t i = 0; i < warmup; ++i) {
        for (const QuerySpec& query : queries) {
            const QueryTerms query_terms =
                make_query_terms(query);

            RetrievalStats stats;

            const auto results =
                run_algorithm(
                    algorithm,
                    exhaustive,
                    wand,
                    block_max_wand,
                    query_terms,
                    ranking_model,
                    k,
                    &stats
                );

            volatile std::size_t guard =
                results.size();

            (void)guard;
        }
    }

    std::vector<double> timings_us;
    timings_us.reserve(
        iterations * queries.size()
    );

    std::size_t total_documents_scored = 0;
    std::size_t total_postings_visited = 0;
    std::size_t total_blocks_skipped = 0;

    const std::clock_t cpu_start =
        std::clock();

    const auto wall_start =
        std::chrono::steady_clock::now();

    for (std::size_t iteration = 0;
         iteration < iterations;
         ++iteration) {

        for (const QuerySpec& query : queries) {
            const QueryTerms query_terms =
                make_query_terms(query);

            RetrievalStats stats;

            const auto start =
                std::chrono::steady_clock::now();

            const auto results =
                run_algorithm(
                    algorithm,
                    exhaustive,
                    wand,
                    block_max_wand,
                    query_terms,
                    ranking_model,
                    k,
                    &stats
                );

            const auto end =
                std::chrono::steady_clock::now();

            const double elapsed_us =
                static_cast<double>(
                    std::chrono::duration_cast<
                        std::chrono::duration<double,
                        std::micro>
                    >(end - start).count()
                );

            timings_us.push_back(
                elapsed_us
            );

            total_documents_scored +=
                stats.documents_scored;

            total_postings_visited +=
                stats.postings_visited;

            total_blocks_skipped +=
                stats.blocks_skipped;

            volatile std::size_t guard =
                results.size();

            (void)guard;
        }
    }

    const auto wall_end =
        std::chrono::steady_clock::now();

    const std::clock_t cpu_end =
        std::clock();

    const double wall_seconds =
        std::chrono::duration<double>(
            wall_end - wall_start
        ).count();

    const double cpu_seconds =
        static_cast<double>(
            cpu_end - cpu_start
        ) /
        static_cast<double>(
            CLOCKS_PER_SEC
        );

    TimingSummary summary;

    const double total_us =
        std::accumulate(
            timings_us.begin(),
            timings_us.end(),
            0.0
        );

    summary.mean_us =
        total_us /
        static_cast<double>(
            timings_us.size()
        );

    summary.p50_us =
        percentile(
            timings_us,
            0.50
        );

    summary.p95_us =
        percentile(
            timings_us,
            0.95
        );

    summary.p99_us =
        percentile(
            timings_us,
            0.99
        );

    if (wall_seconds > 0.0) {
        summary.throughput_qps =
            static_cast<double>(
                timings_us.size()
            ) /
            wall_seconds;
    }

    summary.cpu_seconds =
        cpu_seconds;

    summary.documents_scored =
        total_documents_scored /
        iterations;

    summary.postings_visited =
        total_postings_visited /
        iterations;

    summary.blocks_skipped =
        total_blocks_skipped /
        iterations;

    return summary;
}

std::size_t current_memory_bytes() {
#if defined(_WIN32)

    PROCESS_MEMORY_COUNTERS counters{};

    if (
        GetProcessMemoryInfo(
            GetCurrentProcess(),
            &counters,
            sizeof(counters)
        )
    ) {
        return static_cast<std::size_t>(
            counters.WorkingSetSize
        );
    }

    return 0;

#elif defined(__linux__)

    std::ifstream status(
        "/proc/self/status"
    );

    std::string line;

    while (std::getline(status, line)) {
        if (
            line.rfind(
                "VmRSS:",
                0
            ) == 0
        ) {
            std::stringstream stream(line);

            std::string label;
            std::size_t kb = 0;
            std::string unit;

            stream
                >> label
                >> kb
                >> unit;

            return kb * 1024;
        }
    }

    return 0;

#else

    return 0;

#endif
}

void write_separator(
    std::ofstream& output
) {
    output
        << "====================================================================\n";
}

void write_summary_header(
    std::ofstream& output,
    const BenchmarkConfig& config
) {
    output
        << "ATLAS V2.4 RETRIEVAL BENCHMARK\n"
        << "===============================\n\n"
        << "Dataset: "
        << config.dataset_path
        << "\n"
        << "Output: "
        << config.output_path
        << "\n"
        << "Iterations: "
        << config.iterations
        << "\n"
        << "Warmup: "
        << config.warmup
        << "\n\n";

    output
        << "This benchmark treats each JSON 'headline' field as one\n"
        << "logical Atlas document. No individual .txt files are created.\n\n";

    output
        << "For every selected corpus size, the benchmark constructs\n"
        << "queries from the same corpus using actual document frequency.\n"
        << "The exact same query workload is then run against Exhaustive,\n"
        << "WAND, and Block-Max WAND.\n\n";

    output
        << "Memory is process working-set memory and is therefore not an\n"
        << "algorithm-specific allocation measurement.\n\n";
}

void write_query_workload(
    std::ofstream& output,
    const std::vector<QuerySpec>& queries
) {
    output
        << "QUERY WORKLOAD\n"
        << "--------------\n";

    for (const QuerySpec& query : queries) {
        output
            << query.name
            << " ["
            << query.category
            << "] : ";

        for (std::size_t i = 0;
             i < query.terms.size();
             ++i) {

            if (i != 0) {
                output << " ";
            }

            output << query.terms[i];
        }

        output << "\n";
    }

    output << "\n";
}

void run_corpus_benchmark(
    std::ofstream& output,
    const BenchmarkConfig& config,
    std::size_t document_count,
    std::size_t block_size
) {
    output
        << "\n";
    write_separator(output);

    output
        << "CORPUS SIZE: "
        << document_count
        << " DOCUMENTS\n"
        << "BLOCK SIZE: "
        << block_size
        << "\n";

    write_separator(output);

    std::cout
        << "\nLoading "
        << document_count
        << " news headlines with block size "
        << block_size
        << "...\n";

    Corpus corpus =
        load_news_corpus(
            config,
            document_count,
            block_size
        );

    output
        << "Documents loaded: "
        << corpus.documents_loaded
        << "\n"
        << "Vocabulary size: "
        << corpus.index.vocabulary_size()
        << "\n"
        << "Average document length: "
        << std::fixed
        << std::setprecision(3)
        << corpus.index.average_document_length()
        << "\n"
        << "Benchmark queries: "
        << corpus.queries.size()
        << "\n\n";

    write_query_workload(
        output,
        corpus.queries
    );

    TopKRetriever exhaustive(
        corpus.index
    );

    WANDRetriever wand(
        corpus.index
    );

    BlockMaxWANDRetriever block_max_wand(
        corpus.index
    );

    BM25Ranker bm25(
        corpus.index
    );

    TFIDFRanker tfidf(
        corpus.index
    );

    /*
     * Correctness is checked once for each ranking model and K
     * before any performance results are accepted.
     */
    output
        << "CORRECTNESS CHECK\n"
        << "-----------------\n";

    for (const std::size_t k : config.ks) {
        validate_correctness(
            exhaustive,
            wand,
            block_max_wand,
            corpus.queries,
            bm25,
            k
        );

        validate_correctness(
            exhaustive,
            wand,
            block_max_wand,
            corpus.queries,
            tfidf,
            k
        );

        output
            << "K="
            << k
            << " : BM25 PASS, TF-IDF PASS\n";
    }

    output << "\n";

    const std::size_t memory_bytes =
        current_memory_bytes();

    const Algorithm algorithms[] = {
        Algorithm::Exhaustive,
        Algorithm::WAND,
        Algorithm::BlockMaxWAND
    };

    const struct RankingConfiguration {
        const char* name;
        const atlas::RankingModel* model;
    } ranking_configurations[] = {
        {"BM25", &bm25},
        {"TF-IDF", &tfidf}
    };

    output
        << "PERFORMANCE RESULTS\n"
        << "-------------------\n";

    for (
        const RankingConfiguration& ranking :
        ranking_configurations
    ) {
        output
            << "\nRanking model: "
            << ranking.name
            << "\n";

        for (const std::size_t k : config.ks) {
            output
                << "\nK="
                << k
                << "\n";

            for (
                const Algorithm algorithm :
                algorithms
            ) {
                const TimingSummary summary =
                    benchmark_algorithm(
                        algorithm,
                        exhaustive,
                        wand,
                        block_max_wand,
                        corpus.queries,
                        *ranking.model,
                        k,
                        config.warmup,
                        config.iterations
                    );

                output
                    << algorithm_name(algorithm)
                    << " | mean_us="
                    << std::fixed
                    << std::setprecision(2)
                    << summary.mean_us
                    << " | p50_us="
                    << summary.p50_us
                    << " | p95_us="
                    << summary.p95_us
                    << " | p99_us="
                    << summary.p99_us
                    << " | throughput_qps="
                    << summary.throughput_qps
                    << " | cpu_seconds="
                    << summary.cpu_seconds
                    << " | documents_scored="
                    << summary.documents_scored
                    << " | postings_visited="
                    << summary.postings_visited
                    << " | blocks_skipped="
                    << summary.blocks_skipped
                    << " | memory_bytes="
                    << memory_bytes
                    << "\n";
            }
        }
    }

    output << "\n";
}

BenchmarkConfig parse_arguments(
    int argc,
    char** argv
) {
    BenchmarkConfig config;

    for (int i = 1; i < argc; ++i) {
        const std::string argument =
            argv[i];

        auto require_value =
            [&](const char* option) -> std::string {
                if (i + 1 >= argc) {
                    throw std::invalid_argument(
                        std::string(
                            "missing value for "
                        ) + option
                    );
                }

                ++i;
                return argv[i];
            };

        if (argument == "--help") {
            print_usage();
            std::exit(0);
        }

        if (argument == "--dataset-path") {
            config.dataset_path =
                require_value(
                    "--dataset-path"
                );
            continue;
        }

        if (argument == "--documents") {
            config.document_counts =
                parse_size_list(
                    require_value(
                        "--documents"
                    )
                );
            continue;
        }

        if (argument == "--blocks") {
            config.block_sizes =
                parse_size_list(
                    require_value(
                        "--blocks"
                    )
                );
            continue;
        }

        if (argument == "--k") {
            config.ks =
                parse_size_list(
                    require_value(
                        "--k"
                    )
                );
            continue;
        }

        if (argument == "--iterations") {
            config.iterations =
                std::stoull(
                    require_value(
                        "--iterations"
                    )
                );

            if (config.iterations == 0) {
                throw std::invalid_argument(
                    "--iterations must be > 0"
                );
            }

            continue;
        }

        if (argument == "--warmup") {
            config.warmup =
                std::stoull(
                    require_value(
                        "--warmup"
                    )
                );

            continue;
        }

        if (argument == "--output") {
            config.output_path =
                require_value(
                    "--output"
                );
            continue;
        }

        throw std::invalid_argument(
            "unknown argument: " +
            argument
        );
    }

    return config;
}

} // namespace

int main(
    int argc,
    char** argv
) {
    try {
        const BenchmarkConfig config =
            parse_arguments(
                argc,
                argv
            );

        std::ofstream output(
            config.output_path,
            std::ios::out |
            std::ios::trunc
        );

        if (!output) {
            throw std::runtime_error(
                "unable to open benchmark output: " +
                config.output_path
            );
        }

        write_summary_header(
            output,
            config
        );

        /*
         * Each block size is benchmarked independently because
         * block size changes the physical posting layout.
         */
        for (
            const std::size_t document_count :
            config.document_counts
        ) {
            for (
                const std::size_t block_size :
                config.block_sizes
            ) {
                run_corpus_benchmark(
                    output,
                    config,
                    document_count,
                    block_size
                );
            }
        }

        output
            << "\n";
        write_separator(output);

        output
            << "BENCHMARK COMPLETE\n";

        write_separator(output);

        std::cout
            << "\nBenchmark complete.\n"
            << "Results written to: "
            << config.output_path
            << "\n";

        return 0;
    }
    catch (const std::exception& exception) {
        std::cerr
            << "Benchmark failed: "
            << exception.what()
            << "\n";

        return 1;
    }
}