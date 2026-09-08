#pragma once

#include "atlas/index.hpp"
#include "atlas/tokenizer.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string_view>

namespace atlas
{

    class IndexBuilder
    {
    public:
        explicit IndexBuilder(
            std::size_t posting_block_size = kPostingBlockSize);

        IndexBuilder(const IndexBuilder &) = delete;
        IndexBuilder &operator=(const IndexBuilder &) = delete;

        IndexBuilder(IndexBuilder &&) noexcept = default;
        IndexBuilder &operator=(IndexBuilder &&) noexcept = default;

        ~IndexBuilder() = default;

        void add_document(
            DocumentId document_id,
            std::string_view text);

        void finalize();

        bool is_finalized() const noexcept;

        /*
         * Persist the index currently owned by this builder.
         *
         * Saving is allowed before or after finalize().
         */
        void save(
            const std::filesystem::path& path
        ) const;

        /*
         * Load a previously persisted index.
         *
         * Loading establishes the finalized lifecycle state because
         * the loaded index is immediately searchable and must not be
         * modified through add_document().
         */
        void load(
            const std::filesystem::path& path
        );

        const Index &index() const noexcept;

        InMemoryIndex &index() noexcept;

        std::size_t document_count() const noexcept;

        std::size_t vocabulary_size() const noexcept;

    private:
        Tokenizer tokenizer_;
        InMemoryIndex index_;
        bool finalized_ = false;
    };

} // namespace atlas