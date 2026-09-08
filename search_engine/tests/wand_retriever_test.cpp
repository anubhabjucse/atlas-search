#include "atlas/bm25_ranker.hpp"
#include "atlas/index.hpp"
#include "atlas/tokenizer.hpp"
#include "atlas/top_k.hpp"
#include "atlas/wand_retriever.hpp"
#include "atlas/tfidf_ranker.hpp"
#include <cassert>
#include <iostream>
#include <iomanip>
namespace
{

    atlas::InMemoryIndex build_index()
    {
        atlas::InMemoryIndex index;
        atlas::Tokenizer tokenizer;

        index.add(
            1,
            tokenizer.tokenize(
                "distributed systems use consensus algorithms"));

        index.add(
            2,
            tokenizer.tokenize(
                "databases use transactions and consensus"));

        index.add(
            3,
            tokenizer.tokenize(
                "machine learning uses neural networks"));

        index.add(
            4,
            tokenizer.tokenize(
                "distributed databases are interesting"));

        index.add(
            5,
            tokenizer.tokenize(
                "consensus algorithms are important"));

        return index;
    }

    void test_wand_matches_exhaustive_bm25()
    {
        auto index = build_index();

        atlas::BM25Ranker ranker(index);

        atlas::TopKRetriever exhaustive(index);
        atlas::WANDRetriever wand(index);

        const atlas::QueryTerms query{
            "distributed",
            "consensus"};

        const auto expected =
            exhaustive.search(query, ranker, 3);

        const auto actual =
            wand.search(query, ranker, 3);

        assert(actual.size() == expected.size());

        for (std::size_t i = 0; i < expected.size(); ++i)
        {
            assert(
                actual[i].document_id ==
                expected[i].document_id);

            assert(
                actual[i].score ==
                expected[i].score);
        }
        std::cout << "Expected:\n";

        for (const auto &result : expected)
        {
            std::cout
                << result.document_id
                << " -> "
                << result.score
                << '\n';
        }

        std::cout << "Actual:\n";

        for (const auto &result : actual)
        {
            std::cout
                << result.document_id
                << " -> "
                << result.score
                << '\n';
        }
    }

    void test_wand_limits_results()
    {
        auto index = build_index();

        atlas::BM25Ranker ranker(index);
        atlas::WANDRetriever wand(index);

        const atlas::QueryTerms query{
            "consensus"};

        const auto results =
            wand.search(query, ranker, 2);

        assert(results.size() == 2);
    }

    void test_wand_handles_missing_terms()
    {
        auto index = build_index();

        atlas::BM25Ranker ranker(index);
        atlas::WANDRetriever wand(index);

        const atlas::QueryTerms query{
            "missing"};

        const auto results =
            wand.search(query, ranker, 5);

        assert(results.empty());
    }

    void test_wand_handles_empty_query()
    {
        auto index = build_index();

        atlas::BM25Ranker ranker(index);
        atlas::WANDRetriever wand(index);

        const atlas::QueryTerms query{};

        const auto results =
            wand.search(query, ranker, 5);

        assert(results.empty());
    }

    void test_wand_handles_zero_k()
    {
        auto index = build_index();

        atlas::BM25Ranker ranker(index);
        atlas::WANDRetriever wand(index);

        const atlas::QueryTerms query{
            "consensus"};

        const auto results =
            wand.search(query, ranker, 0);

        assert(results.empty());
    }

    void test_wand_with_tfidf()
    {
        auto index = build_index();

        atlas::TFIDFRanker ranker(index);

        atlas::TopKRetriever exhaustive(index);
        atlas::WANDRetriever wand(index);

        const atlas::QueryTerms query{
            "distributed",
            "consensus"};

        const auto expected =
            exhaustive.search(query, ranker, 3);

        const auto actual =
            wand.search(query, ranker, 3);

        assert(actual.size() == expected.size());

        std::cout << std::setprecision(17);

        for (std::size_t i = 0; i < expected.size(); ++i)
        {
            std::cout
                << "COMPARE i=" << i
                << " expected_id=" << expected[i].document_id
                << " actual_id=" << actual[i].document_id
                << " expected_score=" << expected[i].score
                << " actual_score=" << actual[i].score
                << std::endl;

            assert(
                actual[i].document_id ==
                expected[i].document_id);

            assert(
                actual[i].score ==
                expected[i].score);
        }
    }

} // namespace

int main()
{
    test_wand_matches_exhaustive_bm25();
    test_wand_limits_results();
    test_wand_handles_missing_terms();
    test_wand_handles_empty_query();
    test_wand_handles_zero_k();
    test_wand_with_tfidf();

    std::cout << "WAND tests passed.\n";

    return 0;
}