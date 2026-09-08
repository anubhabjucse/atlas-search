#pragma once

#include "atlas/index.hpp"
#include "atlas/retrieval_stats.hpp"

#include <string>
#include <vector>

namespace atlas {

class PhraseRetriever {
public:
    explicit PhraseRetriever(
        const Index& index
    );

    /*
     * Returns documents containing the normalized phrase.
     *
     * Every term must occur at consecutive token positions.
     */
    std::vector<DocumentId> search(
        const std::vector<std::string>& terms,
        RetrievalStats* stats = nullptr
    ) const;

private:
    bool matches_phrase(
        DocumentId document_id,
        const std::vector<std::string>& terms
    ) const;

    const Index& index_;
};

} // namespace atlas