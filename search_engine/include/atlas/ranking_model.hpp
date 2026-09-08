#pragma once

#include "atlas/index.hpp"
#include "atlas/prepared_query.hpp"

#include <string_view>

namespace atlas {

struct ScoredResult {
    DocumentId document_id;
    double score;
};

class RankingModel {
public:
    virtual ~RankingModel() = default;

    /*
     * Original V2/V3 API.
     *
     * Kept intact so existing callers, tests, and benchmarks
     * continue to compile.
     */
    virtual double score(
        DocumentId document_id,
        const QueryTerms& query_terms
    ) const = 0;

    /*
     * Prepared-query API introduced in V3.3.
     *
     * Default implementation preserves compatibility by
     * forwarding through the original QueryTerms API.
     *
     * Concrete rankers override this to use cached metadata.
     */
    virtual double score(
        DocumentId document_id,
        const PreparedQuery& query
    ) const {
        const QueryTerms terms =
            query.term_views();

        return score(
            document_id,
            terms
        );
    }

    /*
     * Safe global upper bound for one term.
     *
     * Existing WAND uses this API.
     */
    virtual double max_score(
        std::string_view term
    ) const = 0;

    /*
     * Prepared-query equivalent.
     *
     * Concrete rankers override this where cached metadata
     * can avoid repeated index lookups.
     */
    virtual double max_score(
        const PreparedQueryTerm& term
    ) const {
        return max_score(term.term);
    }

    /*
     * Safe upper bound for a term restricted to one
     * physical posting block.
     */
    virtual double block_max_score(
        std::string_view term,
        const PostingBlock& block
    ) const = 0;

    /*
     * Prepared-query equivalent.
     */
    virtual double block_max_score(
        const PreparedQueryTerm& term,
        const PostingBlock& block
    ) const {
        return block_max_score(
            term.term,
            block
        );
    }
};

} // namespace atlas