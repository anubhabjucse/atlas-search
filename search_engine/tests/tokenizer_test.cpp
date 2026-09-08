\
#include "atlas/tokenizer.hpp"

#include <cassert>
#include <iostream>
#include <string>

void test_casefolding() {
    atlas::Tokenizer tokenizer;
    const auto tokens = tokenizer.tokenize("Atlas ATLAS");

    assert(tokens.size() == 2);
    assert(tokens[0].term == "atlas");
    assert(tokens[1].term == "atlas");
    assert(tokens[0].position == 0);
    assert(tokens[1].position == 1);
}

void test_unicode_normalization() {
    atlas::Tokenizer tokenizer;

    // "é" can be represented as a composed or decomposed sequence.
    const auto tokens = tokenizer.tokenize("café cafe\u0301");

    assert(tokens.size() == 2);
    assert(tokens[0].term == tokens[1].term);
}

void test_punctuation_and_numbers() {
    atlas::Tokenizer tokenizer;
    const auto tokens = tokenizer.tokenize("Atlas, version 2.0!");

    assert(tokens.size() >= 3);
    assert(tokens[0].term == "atlas");
}

void test_non_latin_text() {
    atlas::Tokenizer tokenizer;
    const auto tokens = tokenizer.tokenize("東京 systems");

    assert(!tokens.empty());
}

void test_positions() {
    atlas::Tokenizer tokenizer;
    const auto tokens = tokenizer.tokenize("one two three");

    assert(tokens.size() == 3);
    assert(tokens[0].position == 0);
    assert(tokens[1].position == 1);
    assert(tokens[2].position == 2);
}

int main() {
    test_casefolding();
    test_unicode_normalization();
    test_punctuation_and_numbers();
    test_non_latin_text();
    test_positions();

    std::cout << "All tokenizer tests passed.\\n";
    return 0;
}
