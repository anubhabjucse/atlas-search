#include "atlas/index.hpp"
#include "atlas/tokenizer.hpp"

#include <cassert>
#include <cmath>
#include <string>
#include <string_view>

void test_term_frequency() {
    atlas::Tokenizer tokenizer;
    atlas::InMemoryIndex index;

    const auto tokens =
        tokenizer.tokenize("cat cat dog");

    index.add(1, tokens);

    const auto& cats = index.postings("cat");
    const auto& dogs = index.postings("dog");

    assert(cats.size() == 1);
    assert(cats[0].document_id == 1);
    assert(cats[0].term_frequency == 2);

    assert(dogs.size() == 1);
    assert(dogs[0].document_id == 1);
    assert(dogs[0].term_frequency == 1);
}

void test_document_statistics() {
    atlas::Tokenizer tokenizer;
    atlas::InMemoryIndex index;

    index.add(
        1,
        tokenizer.tokenize("cat cat dog")
    );

    index.add(
        2,
        tokenizer.tokenize("cat bird")
    );

    index.add(
        3,
        tokenizer.tokenize("dog bird bird bird")
    );

    // Number of documents
    assert(index.document_count() == 3);

    // Individual document lengths
    assert(index.document_length(1) == 3);
    assert(index.document_length(2) == 2);
    assert(index.document_length(3) == 4);

    // Unknown document
    assert(index.document_length(999) == 0);

    // Total number of tokens in corpus
    assert(index.total_document_length() == 9);

    // Average document length
    assert(
        std::abs(
            index.average_document_length() - 3.0
        ) < 1e-9
    );

    // Document frequency
    assert(index.document_frequency("cat") == 2);
    assert(index.document_frequency("dog") == 2);
    assert(index.document_frequency("bird") == 2);

    // Unknown term
    assert(index.document_frequency("missing") == 0);
}

void test_multiple_documents() {
    atlas::Tokenizer tokenizer;
    atlas::InMemoryIndex index;

    index.add(1, tokenizer.tokenize("cat dog"));
    index.add(2, tokenizer.tokenize("cat bird"));
    index.add(3, tokenizer.tokenize("dog bird"));

    const auto& cats = index.postings("cat");
    const auto& dogs = index.postings("dog");
    const auto& birds = index.postings("bird");

    assert(cats.size() == 2);
    assert(cats[0].document_id == 1);
    assert(cats[1].document_id == 2);

    assert(dogs.size() == 2);
    assert(dogs[0].document_id == 1);
    assert(dogs[1].document_id == 3);

    assert(birds.size() == 2);
    assert(birds[0].document_id == 2);
    assert(birds[1].document_id == 3);
}

void test_missing_term() {
    atlas::InMemoryIndex index;

    const auto& postings =
        index.postings("does_not_exist");

    assert(postings.empty());
}

void test_vocabulary_size() {
    atlas::Tokenizer tokenizer;
    atlas::InMemoryIndex index;

    index.add(1, tokenizer.tokenize("cat dog"));
    index.add(2, tokenizer.tokenize("cat bird"));

    assert(index.vocabulary_size() == 3);
}

void test_string_view_lookup() {
    atlas::Tokenizer tokenizer;
    atlas::InMemoryIndex index;

    index.add(
        1,
        tokenizer.tokenize("cat cat dog")
    );

    const std::string term_storage = "cat";
    const std::string_view term_view =
        term_storage;

    const auto& postings =
        index.postings(term_view);

    assert(postings.size() == 1);
    assert(postings[0].document_id == 1);
    assert(postings[0].term_frequency == 2);

    assert(
        index.document_frequency(term_view) == 1
    );

    assert(
        index.term_frequency(term_view, 1) == 2
    );

    assert(
        index.max_term_frequency(term_view) == 2
    );
}

void test_posting_blocks() {
    atlas::Tokenizer tokenizer;

    atlas::InMemoryIndex index(2);

    index.add(
        1,
        tokenizer.tokenize("cat cat")
    );

    index.add(
        2,
        tokenizer.tokenize("cat dog")
    );

    index.add(
        3,
        tokenizer.tokenize("cat cat cat")
    );

    const auto& blocks =
        index.posting_blocks("cat");

    assert(index.posting_block_size() == 2);

    assert(blocks.size() == 2);

    // First block contains documents 1 and 2.
    assert(blocks[0].begin == 0);
    assert(blocks[0].end == 2);
    assert(blocks[0].first_document == 1);
    assert(blocks[0].last_document == 2);
    assert(blocks[0].max_term_frequency == 2);
    assert(blocks[0].min_document_length == 2);

    // Second block contains document 3.
    assert(blocks[1].begin == 2);
    assert(blocks[1].end == 3);
    assert(blocks[1].first_document == 3);
    assert(blocks[1].last_document == 3);
    assert(blocks[1].max_term_frequency == 3);
    assert(blocks[1].min_document_length == 3);
}

void test_empty_index() {
    atlas::InMemoryIndex index;

    assert(index.vocabulary_size() == 0);
    assert(index.document_count() == 0);
    assert(index.max_document_id() == 0);
    assert(index.total_document_length() == 0);
    assert(index.average_document_length() == 0.0);

    assert(index.postings("cat").empty());
    assert(index.posting_blocks("cat").empty());
    assert(index.document_frequency("cat") == 0);
    assert(index.max_term_frequency("cat") == 0);
    assert(index.term_frequency("cat", 1) == 0);
}

int main() {
    test_term_frequency();
    test_multiple_documents();
    test_missing_term();
    test_vocabulary_size();
    test_document_statistics();
    test_string_view_lookup();
    test_posting_blocks();
    test_empty_index();

    return 0;
}