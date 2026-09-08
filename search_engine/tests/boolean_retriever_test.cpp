#include "atlas/boolean_retriever.hpp"
#include "atlas/tokenizer.hpp"
#include <cassert>
#include <vector>

void test_and() {
    atlas::Tokenizer tokenizer;
    atlas::InMemoryIndex index;

    index.add(1, tokenizer.tokenize("cat dog"));
    index.add(2, tokenizer.tokenize("cat bird"));
    index.add(3, tokenizer.tokenize("dog bird"));
    index.add(4, tokenizer.tokenize("cat dog bird"));

    atlas::BooleanRetriever retriever(index);

    const auto result =
        retriever.and_query("cat", "dog");

    assert(
        (result == std::vector<atlas::DocumentId>{1, 4})
    );
}

void test_or() {
    atlas::Tokenizer tokenizer;
    atlas::InMemoryIndex index;

    index.add(1, tokenizer.tokenize("cat"));
    index.add(2, tokenizer.tokenize("dog"));
    index.add(3, tokenizer.tokenize("cat dog"));

    atlas::BooleanRetriever retriever(index);

    const auto result =
        retriever.or_query("cat", "dog");

    assert(
        (result == std::vector<atlas::DocumentId>{1, 2, 3})
    );
}

void test_not() {
    atlas::Tokenizer tokenizer;
    atlas::InMemoryIndex index;

    index.add(1, tokenizer.tokenize("cat"));
    index.add(2, tokenizer.tokenize("dog"));
    index.add(3, tokenizer.tokenize("cat dog"));
    index.add(4, tokenizer.tokenize("bird"));

    atlas::BooleanRetriever retriever(index);

    const auto result =
        retriever.not_query("cat");

    assert(
        (result == std::vector<atlas::DocumentId>{2, 4})
    );
}

void test_missing_term() {
    atlas::Tokenizer tokenizer;
    atlas::InMemoryIndex index;

    index.add(1, tokenizer.tokenize("cat"));

    atlas::BooleanRetriever retriever(index);

    const auto result =
        retriever.and_query("cat", "elephant");

    assert(result.empty());
}

void test_and_with_no_overlap() {
    atlas::Tokenizer tokenizer;
    atlas::InMemoryIndex index;

    index.add(1, tokenizer.tokenize("cat"));
    index.add(2, tokenizer.tokenize("dog"));

    atlas::BooleanRetriever retriever(index);

    const auto result =
        retriever.and_query("cat", "dog");

    assert(result.empty());
}

void test_or_with_same_term() {
    atlas::Tokenizer tokenizer;
    atlas::InMemoryIndex index;

    index.add(1, tokenizer.tokenize("cat"));
    index.add(2, tokenizer.tokenize("cat"));

    atlas::BooleanRetriever retriever(index);

    const auto result =
        retriever.or_query("cat", "cat");

    assert(
        (result == std::vector<atlas::DocumentId>{1, 2})
    );
}

int main() {
    test_and();
    test_or();
    test_not();
    test_missing_term();
    test_and_with_no_overlap();
    test_or_with_same_term();

    return 0;
}