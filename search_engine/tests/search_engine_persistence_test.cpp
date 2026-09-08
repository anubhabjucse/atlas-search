#include "atlas/search_engine.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>
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


void assert_same_boolean_results(
    const std::vector<atlas::DocumentId>& lhs,
    const std::vector<atlas::DocumentId>& rhs
) {
    assert(lhs == rhs);
}


void test_save_load_boolean_search() {
    const std::filesystem::path path =
        "atlas_v4_search_engine.idx";

    atlas::SearchEngine original;

    original.index_document(
        1,
        "cat dog"
    );

    original.index_document(
        2,
        "cat bird"
    );

    original.index_document(
        3,
        "dog bird"
    );

    original.index_document(
        4,
        "cat dog bird"
    );

    original.finalize();

    const auto before =
        original.search(
            "cat AND dog"
        );

    assert_same_boolean_results(
        before,
        std::vector<atlas::DocumentId>{1, 4}
    );

    original.save(path);

    /*
     * original is deliberately allowed to go out of scope.
     *
     * The next SearchEngine has no relationship with the first
     * engine or its in-memory index.
     */
    {
        atlas::SearchEngine reopened;

        assert(!reopened.is_finalized());

        reopened.load(path);

        assert(reopened.is_finalized());

        const auto after =
            reopened.search(
                "cat AND dog"
            );

        assert_same_boolean_results(
            before,
            after
        );

        assert(
            reopened.vocabulary_size() ==
            original.vocabulary_size()
        );

        assert(
            reopened.document_count() ==
            original.document_count()
        );

        assert(
            reopened.max_document_id() ==
            original.max_document_id()
        );

        assert(
            std::abs(
                reopened.average_document_length() -
                original.average_document_length()
            ) < 1e-9
        );

        bool threw = false;

        try {
            reopened.index_document(
                5,
                "elephant"
            );
        }
        catch (const std::logic_error&) {
            threw = true;
        }

        assert(threw);
    }

    std::filesystem::remove(path);
}


void test_save_load_ranked_search() {
    const std::filesystem::path path =
        "atlas_v4_ranked_search_engine.idx";

    atlas::SearchEngine original;

    original.index_document(
        1,
        "cat dog"
    );

    original.index_document(
        2,
        "cat cat cat"
    );

    original.index_document(
        3,
        "dog bird"
    );

    original.index_document(
        4,
        "cat bird"
    );

    original.index_document(
        5,
        "cat dog bird"
    );

    original.finalize();

    atlas::SearchOptions options;

    options.ranking =
        atlas::RankingType::BM25;

    options.retrieval =
        atlas::RetrievalType::TOP_K;

    const auto before =
        original.search(
            "cat dog",
            4,
            options
        );

    assert(!before.empty());

    for (const auto& result : before) {
        assert(result.score > 0.0);
    }

    original.save(path);

    atlas::SearchEngine reopened;

    reopened.load(path);

    const auto after =
        reopened.search(
            "cat dog",
            4,
            options
        );

    assert_same_document_ids(
        before,
        after
    );

    assert(
        before.size() ==
        after.size()
    );

    /*
     * Scores should also survive exactly because all ranking
     * statistics are persisted.
     */
    for (std::size_t i = 0;
         i < before.size();
         ++i) {

        assert(
            std::abs(
                before[i].score -
                after[i].score
            ) < 1e-9
        );
    }

    std::filesystem::remove(path);
}


void test_load_nonexistent_file() {
    atlas::SearchEngine engine;

    bool threw = false;

    try {
        engine.load(
            "atlas_v4_missing_search_engine.idx"
        );
    }
    catch (const std::runtime_error&) {
        threw = true;
    }

    assert(threw);

    /*
     * Failed loading must not finalize the engine.
     */
    assert(!engine.is_finalized());

    /*
     * It should still be possible to build an index normally.
     */
    engine.index_document(
        1,
        "cat"
    );

    assert(
        engine.document_count() == 1
    );
}


void test_load_after_finalize_rejected() {
    const std::filesystem::path path =
        "atlas_v4_load_after_finalize.idx";

    atlas::SearchEngine source;

    source.index_document(
        1,
        "cat"
    );

    source.save(path);

    atlas::SearchEngine engine;

    engine.index_document(
        10,
        "dog"
    );

    engine.finalize();

    bool threw = false;

    try {
        engine.load(path);
    }
    catch (const std::logic_error&) {
        threw = true;
    }

    assert(threw);

    /*
     * Existing finalized index must remain usable.
     */
    const auto result =
        engine.search("dog");

    assert(
        (result ==
         std::vector<atlas::DocumentId>{10})
    );

    std::filesystem::remove(path);
}


void test_load_then_rank_with_all_retrievers() {
    const std::filesystem::path path =
        "atlas_v4_all_retrievers.idx";

    atlas::SearchEngine source;

    source.index_document(
        1,
        "cat dog"
    );

    source.index_document(
        2,
        "cat cat"
    );

    source.index_document(
        3,
        "dog bird"
    );

    source.index_document(
        4,
        "cat dog bird"
    );

    source.index_document(
        5,
        "bird"
    );

    source.finalize();

    source.save(path);

    atlas::SearchEngine reopened;

    reopened.load(path);

    const std::vector<
        atlas::RetrievalType
    > retrieval_types = {
        atlas::RetrievalType::TOP_K,
        atlas::RetrievalType::WAND,
        atlas::RetrievalType::BLOCK_MAX_WAND
    };

    for (const auto retrieval :
         retrieval_types) {

        atlas::SearchOptions options;

        options.ranking =
            atlas::RankingType::BM25;

        options.retrieval =
            retrieval;

        const auto result =
            reopened.search(
                "cat dog",
                4,
                options
            );

        assert(!result.empty());

        for (const auto& item : result) {
            assert(item.score > 0.0);
        }
    }

    std::filesystem::remove(path);
}

} // namespace


int main() {
    test_save_load_boolean_search();

    test_save_load_ranked_search();

    test_load_nonexistent_file();

    test_load_after_finalize_rejected();

    test_load_then_rank_with_all_retrievers();

    return 0;
}