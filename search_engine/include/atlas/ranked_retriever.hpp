#pragma once

#include "atlas/prepared_query.hpp"
#include "atlas/ranking_model.hpp"
#include "atlas/retrieval_stats.hpp"

#include <cstddef>
#include <vector>

namespace atlas {

/*
 * Common abstraction for ranked retrieval strategies.
 *
 * The retrieval strategy decides HOW candidates are found.
 * The RankingModel decides HOW candidates are scored.
 *
 * SearchEngine depends on this interface rather than knowing
 * the implementation details of Top-K, WAND, or Block-Max WAND.
 */
class RankedRetriever {
public:
    virtual ~RankedRetriever() = default;

    virtual std::vector<ScoredResult> search_prepared(
        const PreparedQuery& query,
        const RankingModel& ranking_model,
        std::size_t k,
        RetrievalStats* stats = nullptr
    ) const = 0;
};

} // namespace atlas