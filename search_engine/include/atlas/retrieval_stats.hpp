#pragma once

#include <cstddef>

namespace atlas {

struct RetrievalStats {
    std::size_t documents_scored = 0;
    std::size_t postings_visited = 0;
    std::size_t blocks_skipped = 0;

    /*
     * V4 query diagnostics.
     */
    std::size_t query_terms = 0;
    std::size_t matched_terms = 0;
    std::size_t candidates_considered = 0;

    void reset() noexcept {
        documents_scored = 0;
        postings_visited = 0;
        blocks_skipped = 0;
        query_terms = 0;
        matched_terms = 0;
        candidates_considered = 0;
    }
};

} // namespace atlas