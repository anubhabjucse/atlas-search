
#include "atlas/index_builder.hpp"

#include <cassert>
#include <stdexcept>
#include <string_view>

void test_document_ingestion() {
    atlas::IndexBuilder builder;

    builder.add_document(
        1,
        "cat cat dog"
    );

    builder.add_document(
        2,
        "cat bird"
    );

    const atlas::Index& index =
        builder.index();

    assert(index.document_count() == 2);
    assert(index.vocabulary_size() == 3);

    assert(index.document_length(1) == 3);
    assert(index.document_length(2) == 2);

    assert(index.document_frequency("cat") == 2);
    assert(index.document_frequency("dog") == 1);
    assert(index.document_frequency("bird") == 1);

    assert(index.term_frequency("cat", 1) == 2);
    assert(index.term_frequency("cat", 2) == 1);
}

void test_tokenization_belongs_to_builder() {
    atlas::IndexBuilder builder;

    /*
     * Tokenization happens internally.
     *
     * The caller supplies raw text rather than Token objects.
     */
    builder.add_document(
        1,
        "Cats, CATS, cats!"
    );

    const atlas::Index& index =
        builder.index();

    /*
     * The existing tokenizer performs NFKC case-folding,
     * so all variants become the same indexed term.
     */
    assert(index.vocabulary_size() == 1);
    assert(index.document_frequency("cats") == 1);
    assert(index.term_frequency("cats", 1) == 3);
}

void test_empty_document() {
    atlas::IndexBuilder builder;

    builder.add_document(
        1,
        ""
    );

    assert(builder.document_count() == 1);
    assert(builder.vocabulary_size() == 0);

    const atlas::Index& index =
        builder.index();

    assert(index.document_length(1) == 0);
    assert(index.total_document_length() == 0);
    assert(index.average_document_length() == 0.0);
}

void test_builder_statistics() {
    atlas::IndexBuilder builder;

    builder.add_document(
        10,
        "one two three"
    );

    builder.add_document(
        11,
        "one two"
    );

    builder.add_document(
        12,
        "one"
    );

    assert(builder.document_count() == 3);
    assert(builder.vocabulary_size() == 3);

    const atlas::Index& index =
        builder.index();

    assert(index.total_document_length() == 6);
    assert(index.average_document_length() == 2.0);
    assert(index.max_document_id() == 12);
}

void test_custom_posting_block_size() {
    atlas::IndexBuilder builder(2);

    builder.add_document(1, "cat");
    builder.add_document(2, "cat");
    builder.add_document(3, "cat");

    const atlas::Index& index =
        builder.index();

    assert(index.posting_block_size() == 2);

    const auto& blocks =
        index.posting_blocks("cat");

    assert(blocks.size() == 2);

    assert(blocks[0].first_document == 1);
    assert(blocks[0].last_document == 2);

    assert(blocks[1].first_document == 3);
    assert(blocks[1].last_document == 3);
}

void test_document_ids_must_increase() {
    atlas::IndexBuilder builder;

    builder.add_document(
        5,
        "cat"
    );

    bool threw = false;

    try {
        builder.add_document(
            3,
            "dog"
        );
    }
    catch (const std::invalid_argument&) {
        threw = true;
    }

    assert(threw);
}

void test_finalize() {
    atlas::IndexBuilder builder;

    assert(!builder.is_finalized());

    builder.add_document(
        1,
        "cat dog"
    );

    builder.finalize();

    assert(builder.is_finalized());

    /*
     * Finalization does not invalidate the resulting index.
     */
    const atlas::Index& index =
        builder.index();

    assert(index.document_count() == 1);
    assert(index.vocabulary_size() == 2);
}

void test_finalize_is_idempotent() {
    atlas::IndexBuilder builder;

    builder.add_document(
        1,
        "cat"
    );

    builder.finalize();
    builder.finalize();

    assert(builder.is_finalized());
    assert(builder.document_count() == 1);
}

void test_cannot_add_after_finalize() {
    atlas::IndexBuilder builder;

    builder.add_document(
        1,
        "cat"
    );

    builder.finalize();

    bool threw = false;

    try {
        builder.add_document(
            2,
            "dog"
        );
    }
    catch (const std::logic_error&) {
        threw = true;
    }

    assert(threw);

    assert(
        builder.document_count() == 1
    );
}

int main() {
    test_document_ingestion();
    test_tokenization_belongs_to_builder();
    test_empty_document();
    test_builder_statistics();
    test_custom_posting_block_size();
    test_document_ids_must_increase();
    test_finalize();
    test_finalize_is_idempotent();
    test_cannot_add_after_finalize();

    return 0;
}
