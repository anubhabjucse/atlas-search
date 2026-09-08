#pragma once

#include "atlas/posting_cursor.hpp"
#include "atlas/ranked_retriever.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace atlas
{

    class WANDRetriever : public RankedRetriever
    {
    public:
        explicit WANDRetriever(
            const Index &index);

        std::vector<ScoredResult> search(
            const QueryTerms &query_terms,
            const RankingModel &ranking_model,
            std::size_t k,
            RetrievalStats *stats = nullptr) const;

        std::vector<ScoredResult> search_prepared(
            const PreparedQuery &query,
            const RankingModel &ranking_model,
            std::size_t k,
            RetrievalStats *stats = nullptr) const override;

    private:
        struct CursorState
        {
            const PreparedQueryTerm *term;

            PostingCursor cursor;

            double max_score;
        };

        const Index &index_;
    };

} // namespace atlas