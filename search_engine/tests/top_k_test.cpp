#include "atlas/bm25_ranker.hpp"
#include "atlas/index.hpp"
#include "atlas/tfidf_ranker.hpp"
#include "atlas/tokenizer.hpp"
#include "atlas/top_k.hpp"

#include <cassert>
#include <iostream>

namespace {

atlas::InMemoryIndex build_index()
{
    atlas::InMemoryIndex index;
    atlas::Tokenizer tokenizer;

    index.add(
        1,
        tokenizer.tokenize(
            "Distributed systems use consensus algorithms."
        )
    );

    index.add(
        2,
        tokenizer.tokenize(
            "Databases use transactions and consensus."
        )
    );

    index.add(
        3,
        tokenizer.tokenize(
            "Machine learning uses neural networks."
        )
    );

    index.add(
        4,
        tokenizer.tokenize(
            "Distributed databases are interesting."
        )
    );

    return index;
}

void test_top_k_returns_best_results()
{
    auto index = build_index();

    atlas::TFIDFRanker ranker(index);
    atlas::TopKRetriever retriever(index);

    const atlas::QueryTerms query{
        "consensus"
    };

    const auto results =
        retriever.search(query, ranker, 1);

    assert(results.size() == 1);

    assert(
        results[0].document_id == 1 ||
        results[0].document_id == 2
    );

    assert(results[0].score > 0.0);
}

void test_top_k_limits_result_count()
{
    auto index = build_index();

    atlas::TFIDFRanker ranker(index);
    atlas::TopKRetriever retriever(index);

    const atlas::QueryTerms query{
        "distributed"
    };

    const auto results =
        retriever.search(query, ranker, 1);

    assert(results.size() == 1);
}

void test_k_larger_than_candidates()
{
    auto index = build_index();

    atlas::TFIDFRanker ranker(index);
    atlas::TopKRetriever retriever(index);

    const atlas::QueryTerms query{
        "consensus"
    };

    const auto results =
        retriever.search(query, ranker, 10);

    assert(results.size() == 2);
}

void test_results_are_sorted_descending()
{
    atlas::InMemoryIndex index;
    atlas::Tokenizer tokenizer;

    index.add(
        1,
        tokenizer.tokenize(
            "consensus consensus database"
        )
    );

    index.add(
        2,
        tokenizer.tokenize(
            "consensus database"
        )
    );

    index.add(
        3,
        tokenizer.tokenize(
            "machine learning"
        )
    );

    atlas::TFIDFRanker ranker(index);
    atlas::TopKRetriever retriever(index);

    const atlas::QueryTerms query{
        "consensus"
    };

    const auto results =
        retriever.search(query, ranker, 2);

    assert(results.size() == 2);

    assert(results[0].score >= results[1].score);

    // Higher term frequency should rank document 1 first.
    assert(results[0].document_id == 1);
    assert(results[1].document_id == 2);
}

void test_top_k_tie_breaks_by_document_id()
{
    atlas::InMemoryIndex index;
    atlas::Tokenizer tokenizer;

    /*
     * Documents 2 and 5 contain "consensus" exactly once.
     *
     * Document 7 does not contain "consensus", ensuring:
     *
     *     N  = 3
     *     DF = 2
     *
     * Therefore IDF = log(3 / 2) > 0 and the two
     * matching documents receive positive, identical scores.
     *
     * Atlas tie-breaking rule:
     * smaller DocumentId wins when scores are equal.
     */
    index.add(
        2,
        tokenizer.tokenize(
            "consensus"
        )
    );

    index.add(
        5,
        tokenizer.tokenize(
            "consensus"
        )
    );

    index.add(
        7,
        tokenizer.tokenize(
            "database"
        )
    );

    atlas::TFIDFRanker ranker(index);
    atlas::TopKRetriever retriever(index);

    const atlas::QueryTerms query{
        "consensus"
    };

    const auto results =
        retriever.search(query, ranker, 1);

    assert(results.size() == 1);

    assert(results[0].document_id == 2);
    assert(results[0].score > 0.0);
}

void test_zero_k()
{
    auto index = build_index();

    atlas::BM25Ranker ranker(index);
    atlas::TopKRetriever retriever(index);

    const atlas::QueryTerms query{
        "consensus"
    };

    const auto results =
        retriever.search(query, ranker, 0);

    assert(results.empty());
}

void test_missing_term()
{
    auto index = build_index();

    atlas::BM25Ranker ranker(index);
    atlas::TopKRetriever retriever(index);

    const atlas::QueryTerms query{
        "nonexistent"
    };

    const auto results =
        retriever.search(query, ranker, 5);

    assert(results.empty());
}

} // namespace

int main()
{
    test_top_k_returns_best_results();
    test_top_k_limits_result_count();
    test_k_larger_than_candidates();
    test_results_are_sorted_descending();
    test_top_k_tie_breaks_by_document_id();
    test_zero_k();
    test_missing_term();

    std::cout << "Top-K tests passed.\n";

    return 0;
}