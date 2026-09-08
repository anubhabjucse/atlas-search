#pragma once

#include "atlas/ranking_model.hpp"
#include "atlas/retrieval_stats.hpp"

#include <cstddef>
#include <vector>
#include "atlas/ranked_retriever.hpp"
namespace atlas
{

    class TopKRetriever : public RankedRetriever
    {
    public:
        explicit TopKRetriever(
            const Index &index);

        /*
         * Original API.
         *
         * Retained for compatibility with existing tests and
         * benchmarks.
         */
        std::vector<ScoredResult> search(
            const QueryTerms &query_terms,
            const RankingModel &ranking_model,
            std::size_t k,
            RetrievalStats *stats = nullptr) const;

        /*
         * V3.3 prepared-query API.
         */
        std::vector<ScoredResult> search_prepared(
            const PreparedQuery &query,
            const RankingModel &ranking_model,
            std::size_t k,
            RetrievalStats *stats = nullptr) const override;

    private:
        const Index &index_;
    };

} // namespace atlas