#include "atlas/search_engine.hpp"

#include <cassert>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace
{

    void assert_same_document_ids(
        const std::vector<atlas::ScoredResult> &lhs,
        const std::vector<atlas::ScoredResult> &rhs)
    {
        assert(lhs.size() == rhs.size());

        for (std::size_t i = 0; i < lhs.size(); ++i)
        {
            assert(
                lhs[i].document_id ==
                rhs[i].document_id);
        }
    }

    void test_single_term()
    {
        atlas::SearchEngine engine;

        engine.index_document(1, "cat dog");
        engine.index_document(2, "cat bird");
        engine.index_document(3, "dog bird");

        const auto result =
            engine.search("cat");

        assert(
            (result ==
             std::vector<atlas::DocumentId>{1, 2}));
    }

    void test_and_query()
    {
        atlas::SearchEngine engine;

        engine.index_document(1, "cat dog");
        engine.index_document(2, "cat bird");
        engine.index_document(3, "dog bird");
        engine.index_document(4, "cat dog bird");

        const auto result =
            engine.search("cat AND dog");

        assert(
            (result ==
             std::vector<atlas::DocumentId>{1, 4}));
    }

    void test_or_query()
    {
        atlas::SearchEngine engine;

        engine.index_document(1, "cat");
        engine.index_document(2, "dog");
        engine.index_document(3, "bird");

        const auto result =
            engine.search("cat OR dog");

        assert(
            (result ==
             std::vector<atlas::DocumentId>{1, 2}));
    }

    void test_not_query()
    {
        atlas::SearchEngine engine;

        engine.index_document(1, "cat");
        engine.index_document(2, "dog");
        engine.index_document(3, "cat dog");
        engine.index_document(4, "bird");

        const auto result =
            engine.search("NOT cat");

        assert(
            (result ==
             std::vector<atlas::DocumentId>{2, 4}));
    }

    void test_nested_expression()
    {
        atlas::SearchEngine engine;

        engine.index_document(1, "cat dog");
        engine.index_document(2, "cat bird");
        engine.index_document(3, "dog bird");
        engine.index_document(4, "cat dog bird");

        const auto result =
            engine.search(
                "(cat OR dog) AND NOT bird");

        assert(
            (result ==
             std::vector<atlas::DocumentId>{1}));
    }

    void test_operator_precedence()
    {
        atlas::SearchEngine engine;

        engine.index_document(1, "cat");
        engine.index_document(2, "dog");
        engine.index_document(3, "dog bird");
        engine.index_document(4, "cat bird");

        const auto result =
            engine.search(
                "cat OR dog AND bird");

        assert(
            (result ==
             std::vector<atlas::DocumentId>{1, 3, 4}));
    }

    void test_missing_boolean_term()
    {
        atlas::SearchEngine engine;

        engine.index_document(1, "cat");

        const auto result =
            engine.search("elephant");

        assert(result.empty());
    }

    void test_vocabulary_and_statistics()
    {
        atlas::SearchEngine engine;

        engine.index_document(1, "cat dog");
        engine.index_document(2, "cat bird");
        engine.index_document(3, "dog bird");

        assert(engine.vocabulary_size() == 3);
        assert(engine.document_count() == 3);
        assert(engine.max_document_id() == 3);

        assert(
            std::abs(
                engine.average_document_length() - 2.0) < 1e-9);
    }

    void test_document_ids_must_increase()
    {
        atlas::SearchEngine engine;

        engine.index_document(5, "cat");

        bool threw = false;

        try
        {
            engine.index_document(3, "dog");
        }
        catch (const std::invalid_argument &)
        {
            threw = true;
        }

        assert(threw);
    }

    void test_finalize_state()
    {
        atlas::SearchEngine engine;

        assert(!engine.is_finalized());

        engine.index_document(1, "cat");

        engine.finalize();

        assert(engine.is_finalized());

        // Finalization is intentionally idempotent.
        engine.finalize();

        bool threw = false;

        try
        {
            engine.index_document(2, "dog");
        }
        catch (const std::logic_error &)
        {
            threw = true;
        }

        assert(threw);
    }

    void test_ranked_default_search()
    {
        atlas::SearchEngine engine;

        engine.index_document(1, "cat dog");
        engine.index_document(2, "cat cat cat");
        engine.index_document(3, "dog bird");

        const auto result =
            engine.search("cat", 2);

        assert(result.size() == 2);

        for (const auto &item : result)
        {
            assert(item.score > 0.0);
        }
    }

    void test_ranked_zero_k()
    {
        atlas::SearchEngine engine;

        engine.index_document(1, "cat dog");
        engine.index_document(2, "cat bird");

        const auto result =
            engine.search("cat", 0);

        assert(result.empty());
    }

    void test_ranked_missing_term()
    {
        atlas::SearchEngine engine;

        engine.index_document(1, "cat dog");
        engine.index_document(2, "cat bird");

        const auto result =
            engine.search("elephant", 10);

        assert(result.empty());
    }

    void test_ranked_duplicate_terms()
    {
        atlas::SearchEngine engine;

        engine.index_document(1, "cat dog");
        engine.index_document(2, "cat cat");
        engine.index_document(3, "dog bird");

        const auto single =
            engine.search("cat", 10);

        const auto duplicate =
            engine.search("cat cat", 10);

        assert_same_document_ids(
            single,
            duplicate);
    }

    void test_bm25_top_k()
    {
        atlas::SearchEngine engine;

        engine.index_document(1, "cat dog");
        engine.index_document(2, "cat cat cat");
        engine.index_document(3, "dog bird");
        engine.index_document(4, "cat bird");

        atlas::SearchOptions options;

        options.ranking =
            atlas::RankingType::BM25;

        options.retrieval =
            atlas::RetrievalType::TOP_K;

        const auto result =
            engine.search(
                "cat",
                3,
                options);

        assert(result.size() == 3);

        for (const auto &item : result)
        {
            assert(item.score > 0.0);
        }
    }

    void test_tfidf_top_k()
    {
        atlas::SearchEngine engine;

        engine.index_document(1, "cat dog");
        engine.index_document(2, "cat cat cat");
        engine.index_document(3, "dog bird");
        engine.index_document(4, "cat bird");

        atlas::SearchOptions options;

        options.ranking =
            atlas::RankingType::TFIDF;

        options.retrieval =
            atlas::RetrievalType::TOP_K;

        const auto result =
            engine.search(
                "cat",
                3,
                options);

        assert(result.size() == 3);

        for (const auto &item : result)
        {
            assert(item.score > 0.0);
        }
    }

    void test_wand_matches_top_k_bm25()
    {
        atlas::SearchEngine engine;

        engine.index_document(1, "cat dog");
        engine.index_document(2, "cat cat cat");
        engine.index_document(3, "dog bird");
        engine.index_document(4, "cat bird");
        engine.index_document(5, "cat dog bird");
        engine.index_document(6, "bird");

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

        const auto expected =
            engine.search(
                "cat dog",
                4,
                top_k);

        const auto actual =
            engine.search(
                "cat dog",
                4,
                wand);

        assert_same_document_ids(
            expected,
            actual);
    }

    void test_bmw_matches_top_k_bm25()
    {
        atlas::SearchEngine engine;

        engine.index_document(1, "cat dog");
        engine.index_document(2, "cat cat cat");
        engine.index_document(3, "dog bird");
        engine.index_document(4, "cat bird");
        engine.index_document(5, "cat dog bird");
        engine.index_document(6, "bird");

        atlas::SearchOptions top_k;

        top_k.ranking =
            atlas::RankingType::BM25;

        top_k.retrieval =
            atlas::RetrievalType::TOP_K;

        atlas::SearchOptions bmw;

        bmw.ranking =
            atlas::RankingType::BM25;

        bmw.retrieval =
            atlas::RetrievalType::BLOCK_MAX_WAND;

        const auto expected =
            engine.search(
                "cat dog",
                4,
                top_k);

        const auto actual =
            engine.search(
                "cat dog",
                4,
                bmw);

        assert_same_document_ids(
            expected,
            actual);
    }

    void test_wand_matches_top_k_tfidf()
    {
        atlas::SearchEngine engine;

        engine.index_document(1, "cat dog");
        engine.index_document(2, "cat cat cat");
        engine.index_document(3, "dog bird");
        engine.index_document(4, "cat bird");
        engine.index_document(5, "cat dog bird");

        atlas::SearchOptions top_k;

        top_k.ranking =
            atlas::RankingType::TFIDF;

        top_k.retrieval =
            atlas::RetrievalType::TOP_K;

        atlas::SearchOptions wand;

        wand.ranking =
            atlas::RankingType::TFIDF;

        wand.retrieval =
            atlas::RetrievalType::WAND;

        const auto expected =
            engine.search(
                "cat dog",
                4,
                top_k);

        const auto actual =
            engine.search(
                "cat dog",
                4,
                wand);

        assert_same_document_ids(
            expected,
            actual);
    }

    void test_bmw_matches_top_k_tfidf()
    {
        atlas::SearchEngine engine;

        engine.index_document(1, "cat dog");
        engine.index_document(2, "cat cat cat");
        engine.index_document(3, "dog bird");
        engine.index_document(4, "cat bird");
        engine.index_document(5, "cat dog bird");

        atlas::SearchOptions top_k;

        top_k.ranking =
            atlas::RankingType::TFIDF;

        top_k.retrieval =
            atlas::RetrievalType::TOP_K;

        atlas::SearchOptions bmw;

        bmw.ranking =
            atlas::RankingType::TFIDF;

        bmw.retrieval =
            atlas::RetrievalType::BLOCK_MAX_WAND;

        const auto expected =
            engine.search(
                "cat dog",
                4,
                top_k);

        const auto actual =
            engine.search(
                "cat dog",
                4,
                bmw);

        assert_same_document_ids(
            expected,
            actual);
    }

    void test_retrieval_statistics()
    {
        atlas::SearchEngine engine;

        engine.index_document(1, "cat dog");
        engine.index_document(2, "cat cat");
        engine.index_document(3, "dog bird");

        atlas::RetrievalStats stats{};

        atlas::SearchOptions options;

        options.ranking =
            atlas::RankingType::BM25;

        options.retrieval =
            atlas::RetrievalType::TOP_K;

        const auto result =
            engine.search(
                "cat",
                2,
                options,
                &stats);

        assert(!result.empty());
        assert(stats.documents_scored > 0);
        assert(stats.postings_visited > 0);
    }

    void test_boolean_retrieval_rejected_by_ranked_api()
    {
        atlas::SearchEngine engine;

        engine.index_document(1, "cat dog");
        engine.index_document(2, "cat bird");

        atlas::SearchOptions options;

        options.ranking =
            atlas::RankingType::BM25;

        options.retrieval =
            atlas::RetrievalType::BOOLEAN;

        bool threw = false;

        try
        {
            (void)engine.search(
                "cat",
                10,
                options);
        }
        catch (const std::invalid_argument &)
        {
            threw = true;
        }

        assert(threw);
    }

    void test_k_limits_results()
    {
        atlas::SearchEngine engine;

        engine.index_document(1, "cat");
        engine.index_document(2, "cat");
        engine.index_document(3, "cat");
        engine.index_document(4, "cat");
        engine.index_document(5, "cat");

        const auto result =
            engine.search("cat", 2);

        assert(result.size() == 2);
    }

    void test_k_larger_than_candidates()
    {
        atlas::SearchEngine engine;

        engine.index_document(1, "cat");
        engine.index_document(2, "cat");

        const auto result =
            engine.search("cat", 10);

        assert(result.size() == 2);
    }
    void test_free_text_terms_do_not_need_to_be_adjacent()
    {
        atlas::SearchEngine engine;

        engine.index_document(
            1,
            "Tata recently announced several new passenger cars");

        engine.index_document(
            2,
            "Tata Motors");

        engine.index_document(
            3,
            "electric cars");

        engine.finalize();

        const auto result =
            engine.search(
                "Tata cars",
                10);

        assert(!result.empty());

        bool found_1 = false;
        bool found_2 = false;
        bool found_3 = false;

        for (const auto &item : result)
        {
            if (item.document_id == 1)
            {
                found_1 = true;
            }

            if (item.document_id == 2)
            {
                found_2 = true;
            }

            if (item.document_id == 3)
            {
                found_3 = true;
            }
        }

        assert(found_1);
        assert(found_2);
        assert(found_3);
    }
    void test_ranked_boolean_query_is_not_treated_as_terms()
    {
        atlas::SearchEngine engine;

        engine.index_document(
            1,
            "cat dog");

        engine.index_document(
            2,
            "cat");

        engine.index_document(
            3,
            "dog");

        engine.finalize();

        const auto result =
            engine.search(
                "cat AND dog",
                10);

        assert(result.size() == 1);
        assert(result[0].document_id == 1);
    }
    void test_ranked_phrase_query_uses_positional_semantics()
    {
        atlas::SearchEngine engine;

        engine.index_document(
            1,
            "Tata cars");

        engine.index_document(
            2,
            "Tata makes cars");

        engine.finalize();

        const auto result =
            engine.search(
                "\"Tata cars\"",
                10);

        assert(result.size() == 1);
        assert(result[0].document_id == 1);
    }
    void test_ranked_mixed_term_and_phrase_query()
    {
        atlas::SearchEngine engine;

        engine.index_document(
            1,
            "Tata electric cars");

        engine.index_document(
            2,
            "Tata electric vehicles");

        engine.index_document(
            3,
            "electric cars");

        engine.finalize();

        const auto result =
            engine.search(
                "Tata \"electric cars\"",
                10);

        assert(result.size() == 1);
        assert(result[0].document_id == 1);
    }
    void test_all_ranked_retrievers_have_same_results()
    {
        atlas::SearchEngine engine;

        engine.index_document(
            1,
            "cat dog");

        engine.index_document(
            2,
            "cat cat cat");

        engine.index_document(
            3,
            "dog bird");

        engine.index_document(
            4,
            "cat bird");

        engine.index_document(
            5,
            "cat dog bird");

        engine.index_document(
            6,
            "bird");

        engine.finalize();

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

        const auto top =
            engine.search(
                "cat dog",
                6,
                top_k);

        const auto wand_result =
            engine.search(
                "cat dog",
                6,
                wand);

        const auto bmw_result =
            engine.search(
                "cat dog",
                6,
                bmw);

        assert_same_document_ids(
            top,
            wand_result);

        assert_same_document_ids(
            top,
            bmw_result);
    }
    void test_search_explanation_is_structured()
    {
        atlas::SearchEngine engine;

        engine.index_document(
            1,
            "Tata makes electric cars");

        engine.index_document(
            2,
            "Electric vehicles");

        engine.finalize();

        const atlas::SearchExplanation explanation =
            engine.explain(
                "Tata electric cars",
                1);

        assert(
            explanation.document_id == 1);

        assert(
            explanation.query_terms == 3);

        assert(
            explanation.matched_terms == 3);

        assert(
            explanation.matched_term_names.size() == 3);

        assert(
            explanation.term_frequencies.size() == 3);

        assert(
            explanation.term_idfs.size() == 3);

        assert(
            explanation.lexical_score > 0.0);

        assert(
            explanation.final_score ==
            explanation.lexical_score);

        assert(
            explanation.mode ==
            atlas::ExplanationMode::LEXICAL);

        assert(
            explanation.matched_query);
    }
    void test_phrase_explanation_is_positional()
    {
        atlas::SearchEngine engine;

        engine.index_document(
            1,
            "Tata cars");

        engine.index_document(
            2,
            "Tata makes cars");

        engine.finalize();

        const atlas::SearchExplanation explanation =
            engine.explain(
                "\"Tata cars\"",
                1);

        assert(
            explanation.mode ==
            atlas::ExplanationMode::PHRASE);

        assert(
            explanation.phrase_match);

        assert(
            explanation.matched_query);
    }

} // namespace

int main()
{
    test_single_term();
    test_and_query();
    test_or_query();
    test_not_query();
    test_nested_expression();
    test_operator_precedence();
    test_missing_boolean_term();

    test_vocabulary_and_statistics();
    test_document_ids_must_increase();
    test_finalize_state();

    test_ranked_default_search();
    test_ranked_zero_k();
    test_ranked_missing_term();
    test_ranked_duplicate_terms();

    test_bm25_top_k();
    test_tfidf_top_k();

    test_wand_matches_top_k_bm25();
    test_bmw_matches_top_k_bm25();

    test_wand_matches_top_k_tfidf();
    test_bmw_matches_top_k_tfidf();

    test_retrieval_statistics();
    test_boolean_retrieval_rejected_by_ranked_api();

    test_k_limits_results();
    test_k_larger_than_candidates();
    test_free_text_terms_do_not_need_to_be_adjacent();
    test_ranked_boolean_query_is_not_treated_as_terms();
    test_ranked_phrase_query_uses_positional_semantics();
    test_ranked_mixed_term_and_phrase_query();
    test_all_ranked_retrievers_have_same_results();
    test_search_explanation_is_structured();
    test_phrase_explanation_is_positional();
    return 0;
}