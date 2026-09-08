#include "atlas/index.hpp"
#include "atlas/posting_cursor.hpp"
#include "atlas/tokenizer.hpp"

#include <cassert>
#include <iostream>
#include <vector>

namespace {

std::vector<atlas::Posting> build_postings(std::size_t count)
{
    std::vector<atlas::Posting> postings;
    postings.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        postings.push_back(
            atlas::Posting{
                static_cast<atlas::DocumentId>(i + 1),
                1
            }
        );
    }

    return postings;
}

void test_cursor_starts_at_first_posting()
{
    const auto postings = build_postings(10);

    atlas::PostingCursor cursor(postings);

    assert(cursor.valid());
    assert(cursor.current().document_id == 1);
}

void test_next_moves_sequentially()
{
    const auto postings = build_postings(10);

    atlas::PostingCursor cursor(postings);

    cursor.next();

    assert(cursor.valid());
    assert(cursor.current().document_id == 2);

    cursor.next();

    assert(cursor.current().document_id == 3);
}

void test_next_exhausts_cursor()
{
    const auto postings = build_postings(3);

    atlas::PostingCursor cursor(postings);

    cursor.next();
    cursor.next();
    cursor.next();

    assert(!cursor.valid());
}

void test_advance_moves_to_target()
{
    const auto postings = build_postings(10);

    atlas::PostingCursor cursor(postings);

    cursor.advance(7);

    assert(cursor.valid());
    assert(cursor.current().document_id == 7);
}

void test_advance_stops_at_first_document_at_or_after_target()
{
    const std::vector<atlas::Posting> postings{
        {2, 1},
        {5, 1},
        {10, 1},
        {20, 1}
    };

    atlas::PostingCursor cursor(postings);

    cursor.advance(6);

    assert(cursor.valid());
    assert(cursor.current().document_id == 10);
}

void test_advance_beyond_end_exhausts_cursor()
{
    const auto postings = build_postings(10);

    atlas::PostingCursor cursor(postings);

    cursor.advance(100);

    assert(!cursor.valid());
}

void test_advance_does_not_move_backward()
{
    const auto postings = build_postings(10);

    atlas::PostingCursor cursor(postings);

    cursor.advance(7);

    assert(cursor.current().document_id == 7);

    cursor.advance(3);

    assert(cursor.current().document_id == 7);
}

void test_single_block()
{
    const auto postings = build_postings(10);

    atlas::PostingCursor cursor(postings);

    assert(cursor.current_block() == 0);
    assert(cursor.block_end() == 10);
}

void test_block_boundary()
{
    const auto postings =
        build_postings(atlas::kPostingBlockSize * 2 + 10);

    atlas::PostingCursor cursor(postings);

    assert(cursor.current_block() == 0);
    assert(cursor.block_end() == atlas::kPostingBlockSize);

    cursor.advance(
        static_cast<atlas::DocumentId>(
            atlas::kPostingBlockSize + 1
        )
    );

    assert(cursor.current_block() == 1);
    assert(
        cursor.block_end() ==
        atlas::kPostingBlockSize * 2
    );
}

void test_final_partial_block()
{
    const std::size_t count =
        atlas::kPostingBlockSize * 2 + 10;

    const auto postings =
        build_postings(count);

    atlas::PostingCursor cursor(postings);

    cursor.advance(
        static_cast<atlas::DocumentId>(
            atlas::kPostingBlockSize * 2 + 1
        )
    );

    assert(cursor.current_block() == 2);
    assert(cursor.block_end() == count);
}

void test_skip_block()
{
    const auto postings =
        build_postings(atlas::kPostingBlockSize * 3);

    atlas::PostingCursor cursor(postings);

    assert(cursor.current_block() == 0);
    assert(cursor.current().document_id == 1);

    cursor.skip_block();

    assert(cursor.valid());
    assert(cursor.current_block() == 1);
    assert(
        cursor.current().document_id ==
        atlas::kPostingBlockSize + 1
    );

    cursor.skip_block();

    assert(cursor.valid());
    assert(cursor.current_block() == 2);
    assert(
        cursor.current().document_id ==
        atlas::kPostingBlockSize * 2 + 1
    );
}

void test_skip_final_block_exhausts_cursor()
{
    const auto postings =
        build_postings(atlas::kPostingBlockSize * 2);

    atlas::PostingCursor cursor(postings);

    cursor.skip_block();

    assert(cursor.valid());
    assert(cursor.current_block() == 1);

    cursor.skip_block();

    assert(!cursor.valid());
}

void test_skip_block_from_partial_final_block()
{
    const std::size_t count =
        atlas::kPostingBlockSize + 10;

    const auto postings =
        build_postings(count);

    atlas::PostingCursor cursor(postings);

    cursor.skip_block();

    assert(cursor.valid());
    assert(cursor.current_block() == 1);
    assert(cursor.block_end() == count);

    cursor.skip_block();

    assert(!cursor.valid());
}

} // namespace

int main()
{
    test_cursor_starts_at_first_posting();
    test_next_moves_sequentially();
    test_next_exhausts_cursor();
    test_advance_moves_to_target();
    test_advance_stops_at_first_document_at_or_after_target();
    test_advance_beyond_end_exhausts_cursor();
    test_advance_does_not_move_backward();

    test_single_block();
    test_block_boundary();
    test_final_partial_block();

    test_skip_block();
    test_skip_final_block_exhausts_cursor();
    test_skip_block_from_partial_final_block();

    std::cout << "Posting cursor tests passed.\n";

    return 0;
}