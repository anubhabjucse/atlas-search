#pragma once

#include "atlas/ranking_model.hpp"

namespace atlas {

class TFIDFRanker final : public RankingModel {
public:
    explicit TFIDFRanker(
        const Index& index
    );

    double score(
        DocumentId document_id,
        const QueryTerms& query_terms
    ) const override;

    double score(
        DocumentId document_id,
        const PreparedQuery& query
    ) const override;

    double max_score(
        std::string_view term
    ) const override;

    double max_score(
        const PreparedQueryTerm& term
    ) const override;

    double block_max_score(
        std::string_view term,
        const PostingBlock& block
    ) const override;

    double block_max_score(
        const PreparedQueryTerm& term,
        const PostingBlock& block
    ) const override;

private:
    const Index& index_;
};

} // namespace atlas