#include "atlas/boolean_retriever.hpp"

namespace atlas {

BooleanRetriever::BooleanRetriever(const Index& index)
    : index_(index) {
}

ResultSet
BooleanRetriever::term(
    std::string_view term
) const {
    const auto& postings =
        index_.postings(term);

    ResultSet result;
    result.reserve(postings.size());

    for (const Posting& posting : postings) {
        result.push_back(posting.document_id);
    }

    return result;
}

ResultSet
BooleanRetriever::intersect(
    const ResultSet& left,
    const ResultSet& right
) const {
    ResultSet result;

    result.reserve(
        left.size() < right.size()
            ? left.size()
            : right.size()
    );

    std::size_t left_pos = 0;
    std::size_t right_pos = 0;

    while (left_pos < left.size() &&
           right_pos < right.size()) {

        const DocumentId left_id =
            left[left_pos];

        const DocumentId right_id =
            right[right_pos];

        if (left_id == right_id) {
            result.push_back(left_id);

            ++left_pos;
            ++right_pos;
        }
        else if (left_id < right_id) {
            ++left_pos;
        }
        else {
            ++right_pos;
        }
    }

    return result;
}

ResultSet
BooleanRetriever::unite(
    const ResultSet& left,
    const ResultSet& right
) const {
    ResultSet result;

    result.reserve(
        left.size() + right.size()
    );

    std::size_t left_pos = 0;
    std::size_t right_pos = 0;

    while (left_pos < left.size() ||
           right_pos < right.size()) {

        if (left_pos == left.size()) {
            result.push_back(
                right[right_pos++]
            );
        }
        else if (right_pos == right.size()) {
            result.push_back(
                left[left_pos++]
            );
        }
        else {
            const DocumentId left_id =
                left[left_pos];

            const DocumentId right_id =
                right[right_pos];

            if (left_id < right_id) {
                result.push_back(left_id);
                ++left_pos;
            }
            else if (right_id < left_id) {
                result.push_back(right_id);
                ++right_pos;
            }
            else {
                result.push_back(left_id);

                ++left_pos;
                ++right_pos;
            }
        }
    }

    return result;
}

ResultSet
BooleanRetriever::complement(
    const ResultSet& input
) const {
    ResultSet result;

    const DocumentId max_document_id =
        index_.max_document_id();

    result.reserve(
        max_document_id >= input.size()
            ? max_document_id - input.size()
            : 0
    );

    std::size_t input_pos = 0;

    for (DocumentId document_id = 1;
         document_id <= max_document_id;
         ++document_id) {

        if (input_pos < input.size() &&
            input[input_pos] == document_id) {

            ++input_pos;
        }
        else {
            result.push_back(document_id);
        }
    }

    return result;
}

ResultSet
BooleanRetriever::and_query(
    std::string_view left_term,
    std::string_view right_term
) const {
    return intersect(
        term(left_term),
        term(right_term)
    );
}

ResultSet
BooleanRetriever::or_query(
    std::string_view left_term,
    std::string_view right_term
) const {
    return unite(
        term(left_term),
        term(right_term)
    );
}

ResultSet
BooleanRetriever::not_query(
    std::string_view term_name
) const {
    return complement(
        term(term_name)
    );
}

} // namespace atlas