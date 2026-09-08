#pragma once

#include "atlas/posting_cursor.hpp"
#include "atlas/ranked_retriever.hpp"

#include <cstddef>
#include <vector>

namespace atlas {

class BlockMaxWANDRetriever:public RankedRetriever {
public:
    explicit BlockMaxWANDRetriever(
        const Index& index
    );

    std::vector<ScoredResult> search(
        const QueryTerms& query_terms,
        const RankingModel& ranking_model,
        std::size_t k,
        RetrievalStats* stats = nullptr
    ) const;

    std::vector<ScoredResult> search_prepared(
        const PreparedQuery& query,
        const RankingModel& ranking_model,
        std::size_t k,
        RetrievalStats* stats = nullptr
    ) const override;

private:
    struct CursorState {
        const PreparedQueryTerm* term;

        PostingCursor cursor;

        const std::vector<PostingBlock>* blocks;

        double max_score;

        std::vector<double> block_max_scores;

        double current_block_max_score = 0.0;
    };

    static void refresh_block_bound(
        CursorState& state
    );

    static double range_upper_bound(
        const CursorState& state,
        DocumentId minimum_document,
        DocumentId maximum_document
    );

    const Index& index_;
};

} // namespace atlas