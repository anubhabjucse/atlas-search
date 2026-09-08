#include "atlas/bm25_ranker.hpp"
#include "atlas/tfidf_ranker.hpp"
#include "atlas/tokenizer.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

void test_tfidf() {
    atlas::Tokenizer tokenizer;
    atlas::InMemoryIndex index;

    // Based on the actual tiny Atlas corpus:
    //
    // 1: Distributed systems use consensus algorithms.
    // 2: Databases use transactions and consensus.
    // 3: Machine learning uses neural networks.
    // 4: Distributed databases are interesting.

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

    atlas::TFIDFRanker ranker(index);

    const atlas::QueryTerms query = {
        "consensus"
    };

    const double document_one_score =
        ranker.score(1, query);

    const double document_two_score =
        ranker.score(2, query);

    const double document_three_score =
        ranker.score(3, query);

    // consensus appears in 2 of 4 documents.
    //
    // IDF = log(4 / 2) > 0

    assert(document_one_score > 0.0);
    assert(document_two_score > 0.0);

    // Document 3 does not contain consensus.
    assert(document_three_score == 0.0);

    // Both matching documents contain the term once,
    // so their TF-IDF scores should be equal.
    assert(
        std::abs(
            document_one_score -
            document_two_score
        ) < 1e-9
    );
}

void test_tfidf_term_frequency() {
    atlas::Tokenizer tokenizer;
    atlas::InMemoryIndex index;

    index.add(
        1,
        tokenizer.tokenize(
            "consensus consensus systems"
        )
    );

    index.add(
        2,
        tokenizer.tokenize(
            "consensus systems"
        )
    );

    index.add(
        3,
        tokenizer.tokenize(
            "database systems"
        )
    );

    atlas::TFIDFRanker ranker(index);

    const atlas::QueryTerms query = {
        "consensus"
    };

    const double document_one_score =
        ranker.score(1, query);

    const double document_two_score =
        ranker.score(2, query);

    // consensus appears in 2/3 documents,
    // so IDF is positive.
    assert(document_one_score > 0.0);
    assert(document_two_score > 0.0);

    // Basic TF-IDF uses raw term frequency,
    // so TF=2 scores higher than TF=1.
    assert(
        document_one_score >
        document_two_score
    );
}

void test_bm25() {
    atlas::Tokenizer tokenizer;
    atlas::InMemoryIndex index;

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

    atlas::BM25Ranker ranker(index);

    const atlas::QueryTerms query = {
        "consensus"
    };

    const double document_one_score =
        ranker.score(1, query);

    const double document_two_score =
        ranker.score(2, query);

    const double document_three_score =
        ranker.score(3, query);

    assert(document_one_score > 0.0);
    assert(document_two_score > 0.0);

    assert(document_three_score == 0.0);
}

void test_bm25_length_normalization() {
    atlas::Tokenizer tokenizer;
    atlas::InMemoryIndex index;

    index.add(
        1,
        tokenizer.tokenize(
            "consensus"
        )
    );

    index.add(
        2,
        tokenizer.tokenize(
            "consensus very long document "
            "with many additional words"
        )
    );

    index.add(
        3,
        tokenizer.tokenize(
            "database systems"
        )
    );

    atlas::BM25Ranker ranker(index);

    const atlas::QueryTerms query = {
        "consensus"
    };

    const double short_document_score =
        ranker.score(1, query);

    const double long_document_score =
        ranker.score(2, query);

    assert(short_document_score > 0.0);
    assert(long_document_score > 0.0);

    // Both documents contain consensus once.
    //
    // BM25 length normalization should penalize
    // the much longer document.
    assert(
        short_document_score >
        long_document_score
    );
}

void test_missing_term() {
    atlas::Tokenizer tokenizer;
    atlas::InMemoryIndex index;

    index.add(
        1,
        tokenizer.tokenize(
            "Distributed systems use consensus."
        )
    );

    index.add(
        2,
        tokenizer.tokenize(
            "Databases use transactions."
        )
    );

    atlas::TFIDFRanker tfidf(index);
    atlas::BM25Ranker bm25(index);

    const atlas::QueryTerms query = {
        "missing"
    };

    assert(tfidf.score(1, query) == 0.0);
    assert(bm25.score(1, query) == 0.0);
}

} // namespace

int main() {
    test_tfidf();
    test_tfidf_term_frequency();
    test_bm25();
    test_bm25_length_normalization();
    test_missing_term();

    std::cout << "ranking_test passed\n";

    return 0;
}