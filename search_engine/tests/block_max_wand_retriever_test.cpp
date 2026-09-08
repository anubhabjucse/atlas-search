#include "atlas/block_max_wand_retriever.hpp"
#include "atlas/bm25_ranker.hpp"
#include "atlas/index.hpp"
#include "atlas/tfidf_ranker.hpp"
#include "atlas/tokenizer.hpp"
#include "atlas/top_k.hpp"

#include <cassert>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

namespace {

atlas::InMemoryIndex build_index()
{
    atlas::InMemoryIndex index;
    atlas::Tokenizer tokenizer;

    for (std::size_t document_id = 1;
         document_id <= atlas::kPostingBlockSize * 3 + 17;
         ++document_id) {
        std::string text = "background document";

        if (document_id == 3) {
            text += " consensus consensus distributed";
        } else if (document_id == 130) {
            text += " consensus consensus consensus distributed";
        } else if (document_id == 260) {
            text += " consensus distributed distributed";
        } else if (document_id % 17 == 0) {
            text += " consensus";
        }

        index.add(
            static_cast<atlas::DocumentId>(document_id),
            tokenizer.tokenize(text)
        );
    }

    return index;
}

void assert_same_results(
    const std::vector<atlas::ScoredResult>& expected,
    const std::vector<atlas::ScoredResult>& actual
)
{
    assert(actual.size() == expected.size());

    for (std::size_t i = 0; i < expected.size(); ++i) {
        assert(actual[i].document_id == expected[i].document_id);
        assert(actual[i].score == expected[i].score);
    }
}

void test_matches_exhaustive()
{
    auto index = build_index();
    const atlas::QueryTerms query{"consensus", "distributed"};

    atlas::TopKRetriever exhaustive(index);
    atlas::BlockMaxWANDRetriever bmw(index);
    atlas::BM25Ranker bm25(index);
    atlas::TFIDFRanker tfidf(index);

    for (const std::size_t k : {1U, 3U, 10U, 50U}) {
        assert_same_results(
            exhaustive.search(query, bm25, k),
            bmw.search(query, bm25, k)
        );

        assert_same_results(
            exhaustive.search(query, tfidf, k),
            bmw.search(query, tfidf, k)
        );
    }
}

void test_tie_breaks_by_smallest_document_id()
{
    atlas::InMemoryIndex index;
    atlas::Tokenizer tokenizer;

    index.add(1, tokenizer.tokenize("other"));
    index.add(2, tokenizer.tokenize("shared"));
    index.add(5, tokenizer.tokenize("shared"));

    atlas::BM25Ranker ranker(index);
    atlas::BlockMaxWANDRetriever bmw(index);

    const auto results = bmw.search({"shared"}, ranker, 1);

    assert(results.size() == 1);
    assert(results.front().document_id == 2);
}

void test_empty_and_zero_k()
{
    auto index = build_index();
    atlas::BM25Ranker ranker(index);
    atlas::BlockMaxWANDRetriever bmw(index);

    assert(bmw.search({}, ranker, 10).empty());
    assert(bmw.search({"consensus"}, ranker, 0).empty());
    assert(bmw.search({"missing"}, ranker, 10).empty());
}

} // namespace

int main()
{
    test_matches_exhaustive();
    test_tie_breaks_by_smallest_document_id();
    test_empty_and_zero_k();

    std::cout << "Block-Max WAND tests passed.\n";
    return 0;
}