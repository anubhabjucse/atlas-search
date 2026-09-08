#pragma once

#include "atlas/block_max_wand_retriever.hpp"
#include "atlas/bm25_ranker.hpp"
#include "atlas/boolean_retriever.hpp"
#include "atlas/index_builder.hpp"
#include "atlas/phrase_retriever.hpp"
#include "atlas/query.hpp"
#include "atlas/ranked_retriever.hpp"
#include "atlas/retrieval_stats.hpp"
#include "atlas/tfidf_ranker.hpp"
#include "atlas/top_k.hpp"
#include "atlas/wand_retriever.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace atlas
{

    enum class RankingType
    {
        TFIDF,
        BM25
    };

    enum class RetrievalType
    {
        BOOLEAN,
        TOP_K,
        WAND,
        BLOCK_MAX_WAND
    };

    enum class SearchMode
    {
        LEXICAL,
        PHRASE,
        BOOLEAN
    };

    struct SearchOptions
    {
        RankingType ranking = RankingType::BM25;
        RetrievalType retrieval = RetrievalType::TOP_K;
        SearchMode mode = SearchMode::LEXICAL;
    };
    enum class ExplanationMode
    {
        LEXICAL,
        PHRASE,
        BOOLEAN
    };
    struct SearchExplanation
    {
        DocumentId document_id = 0;
        ExplanationMode mode =
            ExplanationMode::LEXICAL;

        double final_score = 0.0;
        double lexical_score = 0.0;

        std::size_t query_terms = 0;
        std::size_t matched_terms = 0;
        bool matched_query = false;

        // True when a positional phrase matched this document.
        bool phrase_match = false;
        std::vector<std::string> matched_term_names;
        std::vector<std::uint32_t> term_frequencies;
        std::vector<double> term_idfs;

        RetrievalStats retrieval;
    };

    class SearchEngine
    {
    public:
        SearchEngine();

        void index_document(
            DocumentId document_id,
            std::string_view text);

        void finalize();

        bool is_finalized() const noexcept;

        void save(
            const std::filesystem::path &path) const;

        void load(
            const std::filesystem::path &path);

        /*
         * Original Boolean search API.
         */
        std::vector<DocumentId>
        search(
            std::string_view query) const;

        /*
         * Ranked free-text search.
         */
        std::vector<ScoredResult>
        search(
            std::string_view query,
            std::size_t k,
            const SearchOptions &options,
            RetrievalStats *stats = nullptr) const;

        std::vector<ScoredResult>
        search(
            std::string_view query,
            std::size_t k) const;

        /*
         * Explicit phrase search.
         *
         * The input is normal user text, not a quoted query.
         */
        std::vector<ScoredResult>
        search_phrase(
            std::string_view phrase,
            std::size_t k,
            RetrievalStats *stats = nullptr) const;

        /*
         * Explain exactly how Atlas scored one document for a
         * free-text query.
         */
        SearchExplanation explain(
            std::string_view query,
            DocumentId document_id,
            RankingType ranking = RankingType::BM25) const;

        std::size_t
        vocabulary_size() const noexcept;

        std::size_t
        document_count() const noexcept;

        DocumentId
        max_document_id() const noexcept;

        double
        average_document_length() const noexcept;

    private:
        const RankingModel &
        ranking_model(
            RankingType type) const;
        const RankedRetriever &
        ranked_retriever(
            RetrievalType type) const;

        ResultSet
        evaluate_boolean(
            const QueryNode &node) const;

        std::vector<ScoredResult>
        ranked_search(
            const PreparedQuery &prepared,
            std::size_t k,
            const SearchOptions &options,
            RetrievalStats *stats) const;

        std::vector<std::string>
        normalize_phrase(
            std::string_view phrase) const;
        bool matches_phrase(
            DocumentId document_id,
            const std::vector<std::string> &terms) const;
        bool
        has_explicit_query_syntax(
            std::string_view query) const;

        std::vector<ScoredResult>
        rank_candidates(
            const std::vector<DocumentId> &candidates,
            const PreparedQuery &prepared,
            std::size_t k,
            const RankingModel &ranker,
            RetrievalStats *stats) const;

        IndexBuilder index_builder_;

        QueryParser query_parser_;

        BooleanRetriever boolean_retriever_;

        QueryPreparer query_preparer_;

        PhraseRetriever phrase_retriever_;

        TFIDFRanker tfidf_ranker_;

        BM25Ranker bm25_ranker_;

        TopKRetriever top_k_retriever_;

        WANDRetriever wand_retriever_;

        BlockMaxWANDRetriever block_max_wand_retriever_;
    };

} // namespace atlas