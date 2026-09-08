#include "atlas/tfidf_ranker.hpp"

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace atlas {

TFIDFRanker::TFIDFRanker(
    const Index& index
)
    : index_(index) {
}

double TFIDFRanker::score(
    DocumentId document_id,
    const QueryTerms& query_terms
) const {
    const std::size_t document_count =
        index_.document_count();

    if (document_count == 0) {
        return 0.0;
    }

    double score = 0.0;

    for (const std::string_view term :
         query_terms) {

        const std::uint32_t term_frequency =
            index_.term_frequency(
                term,
                document_id
            );

        if (term_frequency == 0) {
            continue;
        }

        const std::size_t document_frequency =
            index_.document_frequency(term);

        if (document_frequency == 0) {
            continue;
        }

        const double idf =
            std::log(
                static_cast<double>(
                    document_count
                ) /
                static_cast<double>(
                    document_frequency
                )
            );

        score +=
            static_cast<double>(
                term_frequency
            ) *
            idf;
    }

    return score;
}

double TFIDFRanker::score(
    DocumentId document_id,
    const PreparedQuery& query
) const {
    if (query.document_count() == 0) {
        return 0.0;
    }

    double score = 0.0;

    for (const PreparedQueryTerm& term :
         query.terms()) {

        if (term.postings == nullptr ||
            term.postings->empty()) {
            continue;
        }

        /*
         * Posting lists are ordered by DocumentId.
         *
         * Use lower_bound rather than asking the index to perform
         * another term lookup and another document lookup.
         */
        const auto it =
            std::lower_bound(
                term.postings->begin(),
                term.postings->end(),
                document_id,
                [](const Posting& posting,
                   DocumentId document) {
                    return posting.document_id < document;
                }
            );

        if (it == term.postings->end() ||
            it->document_id != document_id) {
            continue;
        }

        score +=
            static_cast<double>(
                it->term_frequency
            ) *
            term.idf;
    }

    return score;
}

double TFIDFRanker::max_score(
    std::string_view term
) const {
    const std::size_t document_count =
        index_.document_count();

    const std::size_t document_frequency =
        index_.document_frequency(term);

    if (document_count == 0 ||
        document_frequency == 0) {
        return 0.0;
    }

    const double idf =
        std::log(
            static_cast<double>(
                document_count
            ) /
            static_cast<double>(
                document_frequency
            )
        );

    const std::uint32_t maximum_term_frequency =
        index_.max_term_frequency(term);

    return
        static_cast<double>(
            maximum_term_frequency
        ) *
        idf;
}

double TFIDFRanker::max_score(
    const PreparedQueryTerm& term
) const {
    return term.max_score;
}

double TFIDFRanker::block_max_score(
    std::string_view term,
    const PostingBlock& block
) const {
    const std::size_t document_count =
        index_.document_count();

    const std::size_t document_frequency =
        index_.document_frequency(term);

    if (document_count == 0 ||
        document_frequency == 0 ||
        block.begin >= block.end) {
        return 0.0;
    }

    const double idf =
        std::log(
            static_cast<double>(
                document_count
            ) /
            static_cast<double>(
                document_frequency
            )
        );

    return
        static_cast<double>(
            block.max_term_frequency
        ) *
        idf;
}

double TFIDFRanker::block_max_score(
    const PreparedQueryTerm& term,
    const PostingBlock& block
) const {
    if (block.begin >= block.end) {
        return 0.0;
    }

    return
        static_cast<double>(
            block.max_term_frequency
        ) *
        term.idf;
}

} // namespace atlas