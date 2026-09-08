#include "atlas/posting_cursor.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace atlas {

namespace {

constexpr std::size_t kNoPosition =
    static_cast<std::size_t>(-1);

} // namespace

PostingCursor::PostingCursor(
    const std::vector<Posting>& postings,
    std::size_t block_size,
    RetrievalStats* stats
)
    : postings_(&postings),
      position_(0),
      block_size_(block_size),
      stats_(stats),
      last_reported_position_(kNoPosition) {

    if (block_size_ == 0) {
        throw std::invalid_argument(
            "posting cursor block size must be greater than zero"
        );
    }
}

bool PostingCursor::valid() const noexcept
{
    return position_ < postings_->size();
}

const Posting& PostingCursor::current() const
{
    assert(valid());

    if (stats_ &&
        last_reported_position_ != position_) {

        ++stats_->postings_visited;

        last_reported_position_ =
            position_;
    }

    return (*postings_)[position_];
}

const std::vector<Posting>&
PostingCursor::postings() const noexcept
{
    return *postings_;
}

void PostingCursor::next() noexcept
{
    if (valid()) {
        ++position_;
    }
}

void PostingCursor::advance(
    DocumentId target
) {
    if (!valid()) {
        return;
    }

    /*
     * Posting lists are sorted by DocumentId.
     *
     * lower_bound gives logarithmic advancement instead of
     * walking one posting at a time.
     */
    const auto begin =
        postings_->begin() +
        static_cast<std::ptrdiff_t>(
            position_
        );

    const auto it =
        std::lower_bound(
            begin,
            postings_->end(),
            target,
            [](const Posting& posting,
               DocumentId document_id) {

                return posting.document_id <
                       document_id;
            }
        );

    position_ =
        static_cast<std::size_t>(
            std::distance(
                postings_->begin(),
                it
            )
        );

    /*
     * The new position has not yet been reported through current().
     */
    last_reported_position_ =
        kNoPosition;
}

std::size_t
PostingCursor::current_block() const noexcept
{
    assert(valid());

    return position_ / block_size_;
}

std::size_t
PostingCursor::block_end() const noexcept
{
    assert(valid());

    const std::size_t block =
        current_block();

    const std::size_t end =
        (block + 1) *
        block_size_;

    return std::min(
        end,
        postings_->size()
    );
}

void PostingCursor::skip_block() noexcept
{
    if (!valid()) {
        return;
    }

    position_ =
        block_end();

    last_reported_position_ =
        kNoPosition;
}

} // namespace atlas