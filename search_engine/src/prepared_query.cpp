#include "atlas/prepared_query.hpp"

#include "atlas/query.hpp"

#include <cmath>
#include <functional>
#include <string>
#include <unordered_set>
#include <utility>

namespace atlas {

QueryPreparer::QueryPreparer(
    const Index& index
)
    : index_(index) {
}

std::string QueryPreparer::normalize_term(
    std::string_view term
) const {
    const std::vector<Token> tokens =
        tokenizer_.tokenize(term);

    /*
     * A single query term must normalize to exactly
     * one token.
     *
     * For example:
     *
     *     "CAT"       -> "cat"
     *     "hello"     -> "hello"
     *     "hello world" -> ignored
     */
    if (tokens.size() != 1) {
        return {};
    }

    return tokens.front().term;
}

PreparedQuery QueryPreparer::prepare(
    const QueryTerms& query_terms
) const {
    PreparedQuery prepared;

    prepared.document_count_ =
        index_.document_count();

    prepared.average_document_length_ =
        index_.average_document_length();

    std::unordered_set<std::string> seen;

    seen.reserve(
        query_terms.size()
    );

    prepared.terms_.reserve(
        query_terms.size()
    );

    for (const std::string_view raw_term :
         query_terms) {

        std::string normalized =
            normalize_term(raw_term);

        if (normalized.empty()) {
            continue;
        }

        /*
         * Deduplicate after normalization.
         *
         * "CAT", "cat", and "Cat" therefore become
         * a single query term.
         */
        if (!seen.insert(normalized).second) {
            continue;
        }

        const std::vector<Posting>& postings =
            index_.postings(normalized);

        const std::vector<PostingBlock>& blocks =
            index_.posting_blocks(normalized);

        const std::size_t document_frequency =
            index_.document_frequency(normalized);

        const std::uint32_t max_term_frequency =
            index_.max_term_frequency(normalized);

        double idf = 0.0;

        if (prepared.document_count_ > 0 &&
            document_frequency > 0) {

            idf =
                std::log(1.0+(
                    static_cast<double>(
                        prepared.document_count_
                    ) -
                    static_cast<double>(
                        document_frequency
                    )+0.5
                )/( static_cast<double>(
                        document_frequency
                    )+0.5)
                );
        }

        const double max_score =
            static_cast<double>(
                max_term_frequency
            ) *
            idf;

        prepared.terms_.push_back(
            PreparedQueryTerm{
                std::move(normalized),
                &postings,
                &blocks,
                document_frequency,
                idf,
                max_term_frequency,
                max_score
            }
        );
    }

    return prepared;
}

PreparedQuery QueryPreparer::prepare(
    const QueryNode& query
) const {
    QueryTerms terms;

    std::function<void(const QueryNode&)> collect =
        [&](const QueryNode& node) {

            switch (node.type) {

            case QueryNodeType::TERM:
                terms.push_back(node.term);
                return;

            case QueryNodeType::PHRASE: {
                const std::vector<Token> tokens =
                    tokenizer_.tokenize(node.term);

                for (const Token& token : tokens) {
                    terms.push_back(token.term);
                }

                return;
            }

            case QueryNodeType::AND:
            case QueryNodeType::OR:
            case QueryNodeType::NOT:
                break;
            }

            for (const auto& child :
                 node.children) {

                if (child) {
                    collect(*child);
                }
            }
        };

    collect(query);

    return prepare(terms);
}

QueryTerms PreparedQuery::term_views() const {
    QueryTerms result;

    result.reserve(
        terms_.size()
    );

    for (const PreparedQueryTerm& term :
         terms_) {

        result.push_back(
            std::string_view(term.term)
        );
    }

    return result;
}

} // namespace atlas