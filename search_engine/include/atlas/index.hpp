#pragma once

#include "atlas/tokenizer.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace atlas {

using DocumentId = std::uint32_t;

inline constexpr std::size_t kPostingBlockSize = 128;

struct Posting {
    DocumentId document_id;
    std::uint32_t term_frequency;

    /*
     * Token positions are zero-based and strictly increasing.
     *
     * Keeping positions in postings gives Atlas the information
     * required for phrase/proximity search without storing the
     * original document text in the search index.
     */
    std::vector<std::uint32_t> positions;
};

struct PostingBlock {
    std::size_t begin = 0;
    std::size_t end = 0;

    DocumentId first_document = 0;
    DocumentId last_document = 0;

    std::uint32_t max_term_frequency = 0;
    std::uint32_t min_document_length = 0;
};

struct TermData {
    std::vector<Posting> postings;
    std::vector<PostingBlock> blocks;
    std::uint32_t max_term_frequency = 0;
};

struct StringHash {
    using is_transparent = void;

    std::size_t operator()(
        std::string_view value
    ) const noexcept {
        return std::hash<std::string_view>{}(value);
    }

    std::size_t operator()(
        const std::string& value
    ) const noexcept {
        return std::hash<std::string_view>{}(value);
    }

    std::size_t operator()(
        const char* value
    ) const noexcept {
        return std::hash<std::string_view>{}(
            value
        );
    }
};

struct StringEqual {
    using is_transparent = void;

    bool operator()(
        std::string_view lhs,
        std::string_view rhs
    ) const noexcept {
        return lhs == rhs;
    }

    bool operator()(
        const std::string& lhs,
        const std::string& rhs
    ) const noexcept {
        return lhs == rhs;
    }

    bool operator()(
        const std::string& lhs,
        std::string_view rhs
    ) const noexcept {
        return lhs == rhs;
    }

    bool operator()(
        std::string_view lhs,
        const std::string& rhs
    ) const noexcept {
        return lhs == rhs;
    }

    bool operator()(
        const char* lhs,
        const std::string& rhs
    ) const noexcept {
        return std::string_view(lhs) == rhs;
    }

    bool operator()(
        const std::string& lhs,
        const char* rhs
    ) const noexcept {
        return lhs == std::string_view(rhs);
    }

    bool operator()(
        const char* lhs,
        std::string_view rhs
    ) const noexcept {
        return std::string_view(lhs) == rhs;
    }

    bool operator()(
        std::string_view lhs,
        const char* rhs
    ) const noexcept {
        return lhs == std::string_view(rhs);
    }
};

class Index {
public:
    virtual ~Index() = default;

    virtual void add(
        DocumentId document_id,
        const std::vector<Token>& tokens
    ) = 0;

    virtual const std::vector<Posting>& postings(
        std::string_view term
    ) const = 0;

    virtual const std::vector<PostingBlock>& posting_blocks(
        std::string_view term
    ) const = 0;

    virtual std::uint32_t term_frequency(
        std::string_view term,
        DocumentId document_id
    ) const = 0;

    virtual const std::vector<std::uint32_t>& term_positions(
        std::string_view term,
        DocumentId document_id
    ) const = 0;

    virtual std::uint32_t max_term_frequency(
        std::string_view term
    ) const = 0;

    virtual std::size_t posting_block_size() const noexcept = 0;

    virtual std::size_t vocabulary_size() const = 0;

    virtual DocumentId max_document_id() const = 0;

    virtual std::size_t document_count() const = 0;

    virtual std::size_t document_length(
        DocumentId document_id
    ) const = 0;

    virtual std::size_t total_document_length() const = 0;

    virtual double average_document_length() const = 0;

    virtual std::size_t document_frequency(
        std::string_view term
    ) const = 0;
};

class InMemoryIndex final : public Index {
public:
    explicit InMemoryIndex(
        std::size_t posting_block_size = kPostingBlockSize
    );

    void add(
        DocumentId document_id,
        const std::vector<Token>& tokens
    ) override;

    const std::vector<Posting>& postings(
        std::string_view term
    ) const override;

    const std::vector<PostingBlock>& posting_blocks(
        std::string_view term
    ) const override;

    std::uint32_t term_frequency(
        std::string_view term,
        DocumentId document_id
    ) const override;

    const std::vector<std::uint32_t>& term_positions(
        std::string_view term,
        DocumentId document_id
    ) const override;

    std::uint32_t max_term_frequency(
        std::string_view term
    ) const override;

    std::size_t posting_block_size() const noexcept override;

    std::size_t vocabulary_size() const override;

    DocumentId max_document_id() const override;

    std::size_t document_count() const override;

    std::size_t document_length(
        DocumentId document_id
    ) const override;

    std::size_t total_document_length() const override;

    double average_document_length() const override;

    std::size_t document_frequency(
        std::string_view term
    ) const override;

    void save(
        const std::filesystem::path& path
    ) const;

    void load(
        const std::filesystem::path& path
    );

private:
    using Dictionary = std::unordered_map<
        std::string,
        TermData,
        StringHash,
        StringEqual
    >;

    Dictionary dictionary_;

    std::unordered_map<DocumentId, std::size_t>
        document_lengths_;

    std::size_t document_count_ = 0;

    std::size_t total_document_length_ = 0;

    DocumentId max_document_id_ = 0;

    bool has_documents_ = false;

    std::size_t posting_block_size_;
};

} // namespace atlas