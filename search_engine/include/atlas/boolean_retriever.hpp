#pragma once

#include "atlas/index.hpp"

#include <string_view>
#include <vector>

namespace atlas {

using ResultSet = std::vector<DocumentId>;

class BooleanRetriever {
public:
    explicit BooleanRetriever(const Index& index);

    // Core Boolean operations.
    //
    // All result sets must be sorted by DocumentId.
    ResultSet intersect(
        const ResultSet& left,
        const ResultSet& right
    ) const;

    ResultSet unite(
        const ResultSet& left,
        const ResultSet& right
    ) const;

    ResultSet complement(
        const ResultSet& input
    ) const;

    // Convenience operations for term queries.
    ResultSet term(
        std::string_view term
    ) const;

    ResultSet and_query(
        std::string_view left_term,
        std::string_view right_term
    ) const;

    ResultSet or_query(
        std::string_view left_term,
        std::string_view right_term
    ) const;

    ResultSet not_query(
        std::string_view term
    ) const;

private:
    const Index& index_;
};

} // namespace atlas