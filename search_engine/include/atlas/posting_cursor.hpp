#pragma once

#include "atlas/index.hpp"
#include "atlas/retrieval_stats.hpp"

#include <cstddef>
#include <vector>

namespace atlas {

class PostingCursor {
public:
    explicit PostingCursor(
        const std::vector<Posting>& postings,
        std::size_t block_size = kPostingBlockSize,
        RetrievalStats* stats = nullptr
    );

    bool valid() const noexcept;

    const Posting& current() const;

    const std::vector<Posting>& postings() const noexcept;

    void next() noexcept;

    void advance(DocumentId target);

    std::size_t current_block() const noexcept;

    std::size_t block_end() const noexcept;

    void skip_block() noexcept;

private:
    const std::vector<Posting>* postings_;

    std::size_t position_;

    std::size_t block_size_;

    RetrievalStats* stats_;

    /*
     * A posting is counted once when the cursor first lands on
     * that posting.
     */
    mutable std::size_t last_reported_position_;
};

} // namespace atlas