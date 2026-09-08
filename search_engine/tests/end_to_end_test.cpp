#include "atlas/search_engine.hpp"

#include <cassert>
#include <cmath>
#include <vector>

namespace {

void assert_same_document_ids(
    const std::vector<atlas::ScoredResult>& lhs,
    const std::vector<atlas::ScoredResult>& rhs
) {
    assert(lhs.size() == rhs.size());

    for (std::size_t i = 0; i < lhs.size(); ++i) {
        assert(
            lhs[i].document_id ==
            rhs[i].document_id
        );
    }
}

void test_boolean_end_to_end() {
    atlas::SearchEngine engine;

    engine.index_document(
        1,
        "distributed systems use consensus"
    );

    engine.index_document(
        2,
        "database systems use transactions"
    );

    engine.index_document(
        3,
        "distributed databases use consensus"
    );

    engine.index_document(
        4,
        "consensus algorithms include raft"
    );

    {
        const auto result =
            engine.search("distributed");

        assert(
            (result ==
             std::vector<atlas::DocumentId>{1, 3})
        );
    }

    {
        const auto result =
            engine.search(
                "distributed AND consensus"
            );

        assert(
            (result ==
             std::vector<atlas::DocumentId>{1, 3})
        );
    }

    {
        const auto result =
            engine.search(
                "distributed AND NOT consensus"
            );

        assert(result.empty());
    }

    {
        const auto result =
            engine.search(
                "database OR raft"
            );

        assert(
            (result ==
             std::vector<atlas::DocumentId>{2, 4})
        );
    }

    {
        const auto result =
            engine.search(
                "(distributed OR database) AND consensus"
            );

        assert(
            (result ==
             std::vector<atlas::DocumentId>{1, 3})
        );
    }
}

void test_ranked_end_to_end() {
    atlas::SearchEngine engine;

    engine.index_document(
        1,
        "distributed systems use consensus"
    );

    engine.index_document(
        2,
        "database systems use transactions"
    );

    engine.index_document(
        3,
        "distributed databases use consensus"
    );

    engine.index_document(
        4,
        "consensus algorithms include raft"
    );

    engine.finalize();

    assert(engine.is_finalized());
    assert(engine.document_count() == 4);
    assert(engine.vocabulary_size() > 0);
    assert(engine.max_document_id() == 4);

    assert(
        std::abs(
            engine.average_document_length() - 4.0
        ) < 1e-9
    );

    atlas::SearchOptions options;

    options.ranking =
        atlas::RankingType::BM25;

    options.retrieval =
        atlas::RetrievalType::TOP_K;

    atlas::RetrievalStats stats{};

    const auto result =
        engine.search(
            "distributed consensus",
            3,
            options,
            &stats
        );

    assert(!result.empty());
    assert(result.size() <= 3);

    for (const auto& item : result) {
        assert(item.score > 0.0);
    }

    assert(stats.documents_scored > 0);
    assert(stats.postings_visited > 0);
}

void test_all_ranked_retrieval_modes() {
    atlas::SearchEngine engine;

    engine.index_document(
        1,
        "distributed systems consensus"
    );

    engine.index_document(
        2,
        "distributed databases consensus"
    );

    engine.index_document(
        3,
        "distributed systems"
    );

    engine.index_document(
        4,
        "consensus algorithms raft"
    );

    engine.index_document(
        5,
        "database transactions"
    );

    atlas::SearchOptions top_k;

    top_k.ranking =
        atlas::RankingType::BM25;

    top_k.retrieval =
        atlas::RetrievalType::TOP_K;

    atlas::SearchOptions wand;

    wand.ranking =
        atlas::RankingType::BM25;

    wand.retrieval =
        atlas::RetrievalType::WAND;

    atlas::SearchOptions bmw;

    bmw.ranking =
        atlas::RankingType::BM25;

    bmw.retrieval =
        atlas::RetrievalType::BLOCK_MAX_WAND;

    const auto exhaustive =
        engine.search(
            "distributed consensus",
            4,
            top_k
        );

    const auto wand_result =
        engine.search(
            "distributed consensus",
            4,
            wand
        );

    const auto bmw_result =
        engine.search(
            "distributed consensus",
            4,
            bmw
        );

    assert_same_document_ids(
        exhaustive,
        wand_result
    );

    assert_same_document_ids(
        exhaustive,
        bmw_result
    );
}

void test_tfidf_end_to_end() {
    atlas::SearchEngine engine;

    engine.index_document(
        1,
        "search engine retrieval"
    );

    engine.index_document(
        2,
        "search engine indexing"
    );

    engine.index_document(
        3,
        "retrieval ranking"
    );

    atlas::SearchOptions options;

    options.ranking =
        atlas::RankingType::TFIDF;

    options.retrieval =
        atlas::RetrievalType::TOP_K;

    const auto result =
        engine.search(
            "search engine",
            3,
            options
        );

    assert(!result.empty());
    assert(result.size() <= 3);

    for (const auto& item : result) {
        assert(item.score > 0.0);
    }
}

void test_missing_ranked_query() {
    atlas::SearchEngine engine;

    engine.index_document(
        1,
        "search engine"
    );

    engine.index_document(
        2,
        "retrieval system"
    );

    const auto result =
        engine.search(
            "elephant",
            10
        );

    assert(result.empty());
}

void test_zero_k_end_to_end() {
    atlas::SearchEngine engine;

    engine.index_document(
        1,
        "search engine"
    );

    const auto result =
        engine.search(
            "search",
            0
        );

    assert(result.empty());
}

} // namespace

int main() {
    test_boolean_end_to_end();
    test_ranked_end_to_end();
    test_all_ranked_retrieval_modes();
    test_tfidf_end_to_end();
    test_missing_ranked_query();
    test_zero_k_end_to_end();

    return 0;
}