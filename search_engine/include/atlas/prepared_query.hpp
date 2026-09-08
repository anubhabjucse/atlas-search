#pragma once

#include "atlas/index.hpp"
#include "atlas/tokenizer.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace atlas {

struct QueryNode;

using QueryTerms = std::vector<std::string_view>;

struct PreparedQueryTerm {
    std::string term;

    const std::vector<Posting>* postings = nullptr;
    const std::vector<PostingBlock>* blocks = nullptr;

    std::size_t document_frequency = 0;

    double idf = 0.0;

    std::uint32_t max_term_frequency = 0;

    double max_score = 0.0;
};

class PreparedQuery {
public:
    PreparedQuery() = default;

    PreparedQuery(const PreparedQuery&) = delete;
    PreparedQuery& operator=(const PreparedQuery&) = delete;

    PreparedQuery(PreparedQuery&&) noexcept = default;
    PreparedQuery& operator=(PreparedQuery&&) noexcept = default;

    ~PreparedQuery() = default;

    const std::vector<PreparedQueryTerm>& terms() const noexcept {
        return terms_;
    }

    std::vector<PreparedQueryTerm>& terms() noexcept {
        return terms_;
    }

    bool empty() const noexcept {
        return terms_.empty();
    }

    std::size_t size() const noexcept {
        return terms_.size();
    }

    std::size_t document_count() const noexcept {
        return document_count_;
    }

    double average_document_length() const noexcept {
        return average_document_length_;
    }

    QueryTerms term_views() const;

private:
    friend class QueryPreparer;

    std::vector<PreparedQueryTerm> terms_;

    std::size_t document_count_ = 0;

    double average_document_length_ = 0.0;
};

class QueryPreparer {
public:
    explicit QueryPreparer(
        const Index& index
    );

    PreparedQuery prepare(
        const QueryNode& query
    ) const;

    PreparedQuery prepare(
        const QueryTerms& query_terms
    ) const;

private:
    std::string normalize_term(
        std::string_view term
    ) const;

    const Index& index_;

    Tokenizer tokenizer_;
};

} // namespace atlas