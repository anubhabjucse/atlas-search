#include "atlas/phrase_retriever.hpp"

#include <algorithm>

namespace atlas {

PhraseRetriever::PhraseRetriever(
    const Index& index
)
    : index_(index) {
}

std::vector<DocumentId>
PhraseRetriever::search(
    const std::vector<std::string>& terms,
    RetrievalStats* stats
) const {
    if (terms.empty()) {
        return {};
    }

    /*
     * Use the rarest term as the candidate generator.
     *
     * This avoids scanning every document when the phrase contains
     * a selective term.
     */
    const std::vector<Posting>* candidate_postings =
        nullptr;

    for (const std::string& term : terms) {

        const auto& postings =
            index_.postings(term);

        if (postings.empty()) {
            return {};
        }

        if (candidate_postings == nullptr ||
            postings.size() <
                candidate_postings->size()) {

            candidate_postings =
                &postings;
        }
    }

    std::vector<DocumentId> results;

    if (candidate_postings == nullptr) {
        return results;
    }

    results.reserve(
        candidate_postings->size()
    );

    for (const Posting& posting :
         *candidate_postings) {

        if (stats) {
            ++stats->postings_visited;
        }

        if (matches_phrase(
                posting.document_id,
                terms
            )) {

            results.push_back(
                posting.document_id
            );
        }
    }

    return results;
}

bool PhraseRetriever::matches_phrase(
    DocumentId document_id,
    const std::vector<std::string>& terms
) const {
    if (terms.empty()) {
        return false;
    }

    if (terms.size() == 1) {
        return index_.term_frequency(
            terms.front(),
            document_id
        ) > 0;
    }

    const auto& first_positions =
        index_.term_positions(
            terms.front(),
            document_id
        );

    if (first_positions.empty()) {
        return false;
    }

    /*
     * For every possible starting position of the first term,
     * verify that every subsequent term occurs at +1.
     */
    for (const std::uint32_t start :
         first_positions) {

        bool matches = true;

        for (std::size_t i = 1;
             i < terms.size();
             ++i) {

            const auto& positions =
                index_.term_positions(
                    terms[i],
                    document_id
                );

            const std::uint32_t expected =
                start +
                static_cast<std::uint32_t>(i);

            if (!std::binary_search(
                    positions.begin(),
                    positions.end(),
                    expected
                )) {

                matches = false;
                break;
            }
        }

        if (matches) {
            return true;
        }
    }

    return false;
}

} // namespace atlas