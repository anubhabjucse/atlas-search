#include "atlas/bm25_ranker.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace atlas {

BM25Ranker::BM25Ranker(
    const Index& index,
    double k1,
    double b
)
    : index_(index),
      k1_(k1),
      b_(b) {
}

double BM25Ranker::score(
    DocumentId document_id,
    const QueryTerms& query_terms
) const {
    const std::size_t document_count =
        index_.document_count();

    if (document_count == 0) {
        return 0.0;
    }

    const double average_document_length =
        index_.average_document_length();

    if (average_document_length == 0.0) {
        return 0.0;
    }

    const double document_length =
        static_cast<double>(
            index_.document_length(document_id)
        );

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
                1.0 +
                (
                    static_cast<double>(
                        document_count
                    ) -
                    static_cast<double>(
                        document_frequency
                    ) +
                    0.5
                ) /
                (
                    static_cast<double>(
                        document_frequency
                    ) +
                    0.5
                )
            );

        const double tf =
            static_cast<double>(
                term_frequency
            );

        const double normalization =
            1.0 -
            b_ +
            b_ *
            (
                document_length /
                average_document_length
            );

        const double denominator =
            tf +
            k1_ * normalization;

        if (denominator <= 0.0) {
            continue;
        }

        score +=
            idf *
            (
                (tf * (k1_ + 1.0)) /
                denominator
            );
    }

    return score;
}

double BM25Ranker::score(
    DocumentId document_id,
    const PreparedQuery& query
) const {
    if (query.document_count() == 0) {
        return 0.0;
    }

    const double average_document_length =
        query.average_document_length();

    if (average_document_length == 0.0) {
        return 0.0;
    }

    const double document_length =
        static_cast<double>(
            index_.document_length(document_id)
        );

    double score = 0.0;

    for (const PreparedQueryTerm& term :
         query.terms()) {

        if (term.postings == nullptr ||
            term.postings->empty()) {
            continue;
        }

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

        const double tf =
            static_cast<double>(
                it->term_frequency
            );

        const double normalization =
            1.0 -
            b_ +
            b_ *
            (
                document_length /
                average_document_length
            );

        const double denominator =
            tf +
            k1_ * normalization;

        if (denominator <= 0.0) {
            continue;
        }

        score +=
            term.idf *
            (
                (tf * (k1_ + 1.0)) /
                denominator
            );
    }

    return score;
}

double BM25Ranker::max_score(
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
            1.0 +
            (
                static_cast<double>(
                    document_count
                ) -
                static_cast<double>(
                    document_frequency
                ) +
                0.5
            ) /
            (
                static_cast<double>(
                    document_frequency
                ) +
                0.5
            )
        );

    return
        idf *
        (k1_ + 1.0);
}

double BM25Ranker::max_score(
    const PreparedQueryTerm& term
) const {
    return
        term.idf *
        (k1_ + 1.0);
}

double BM25Ranker::block_max_score(
    std::string_view term,
    const PostingBlock& block
) const {
    const std::size_t document_count =
        index_.document_count();

    const std::size_t document_frequency =
        index_.document_frequency(term);

    const double average_document_length =
        index_.average_document_length();

    if (document_count == 0 ||
        document_frequency == 0 ||
        average_document_length == 0.0 ||
        block.begin >= block.end ||
        block.max_term_frequency == 0) {
        return 0.0;
    }

    const double idf =
        std::log(
            1.0 +
            (
                static_cast<double>(
                    document_count
                ) -
                static_cast<double>(
                    document_frequency
                ) +
                0.5
            ) /
            (
                static_cast<double>(
                    document_frequency
                ) +
                0.5
            )
        );

    const double tf =
        static_cast<double>(
            block.max_term_frequency
        );

    const double min_document_length =
        static_cast<double>(
            block.min_document_length
        );

    const double normalization =
        1.0 -
        b_ +
        b_ *
        (
            min_document_length /
            average_document_length
        );

    const double denominator =
        tf +
        k1_ * normalization;

    if (denominator <= 0.0) {
        return 0.0;
    }

    return
        idf *
        (
            (tf * (k1_ + 1.0)) /
            denominator
        );
}

double BM25Ranker::block_max_score(
    const PreparedQueryTerm& term,
    const PostingBlock& block
) const {
    const double average_document_length =
        index_.average_document_length();

    if (average_document_length == 0.0 ||
        block.begin >= block.end ||
        block.max_term_frequency == 0) {
        return 0.0;
    }

    const double tf =
        static_cast<double>(
            block.max_term_frequency
        );

    const double min_document_length =
        static_cast<double>(
            block.min_document_length
        );

    const double normalization =
        1.0 -
        b_ +
        b_ *
        (
            min_document_length /
            average_document_length
        );

    const double denominator =
        tf +
        k1_ * normalization;

    if (denominator <= 0.0) {
        return 0.0;
    }

    return
        term.idf *
        (
            (tf * (k1_ + 1.0)) /
            denominator
        );
}

} // namespace atlas