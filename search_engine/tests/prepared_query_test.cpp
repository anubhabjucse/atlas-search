#include "atlas/index.hpp"
#include "atlas/prepared_query.hpp"
#include "atlas/query.hpp"
#include "atlas/tokenizer.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>

namespace {

atlas::InMemoryIndex make_index() {
    atlas::InMemoryIndex index;

    atlas::Tokenizer tokenizer;

    index.add(
        1,
        tokenizer.tokenize(
            "Cat dog cat"
        )
    );

    index.add(
        2,
        tokenizer.tokenize(
            "cat bird"
        )
    );

    index.add(
        3,
        tokenizer.tokenize(
            "dog bird"
        )
    );

    return index;
}

void test_query_terms_are_normalized_and_deduplicated() {
    auto index = make_index();

    atlas::QueryPreparer preparer(index);

    const atlas::QueryTerms terms{
        "CAT",
        "cat",
        "Dog",
        "DOG"
    };

    const atlas::PreparedQuery prepared =
        preparer.prepare(terms);

    assert(prepared.size() == 2);

    assert(prepared.terms()[0].term == "cat");
    assert(prepared.terms()[1].term == "dog");
}

void test_query_parser_terms_are_prepared() {
    auto index = make_index();

    atlas::QueryParser parser;

    const auto query =
        parser.parse(
            "CAT OR dog OR CAT"
        );

    atlas::QueryPreparer preparer(index);

    const atlas::PreparedQuery prepared =
        preparer.prepare(*query);

    assert(prepared.size() == 2);

    assert(prepared.terms()[0].term == "cat");
    assert(prepared.terms()[1].term == "dog");
}

void test_missing_terms_are_safe() {
    auto index = make_index();

    atlas::QueryPreparer preparer(index);

    const atlas::QueryTerms terms{
        "missing"
    };

    const atlas::PreparedQuery prepared =
        preparer.prepare(terms);

    assert(prepared.size() == 1);

    const auto& term =
        prepared.terms().front();

    assert(term.term == "missing");
    assert(term.postings != nullptr);
    assert(term.blocks != nullptr);
    assert(term.postings->empty());
    assert(term.blocks->empty());
    assert(term.document_frequency == 0);
    assert(term.idf == 0.0);
    assert(term.max_term_frequency == 0);
    assert(term.max_score == 0.0);
}

void test_metadata_is_cached() {
    auto index = make_index();

    atlas::QueryPreparer preparer(index);

    const atlas::QueryTerms terms{
        "cat"
    };

    const atlas::PreparedQuery prepared =
        preparer.prepare(terms);

    assert(prepared.size() == 1);

    const auto& term =
        prepared.terms().front();

    assert(term.document_frequency == 2);
    assert(term.max_term_frequency == 2);

    const double expected_idf =
         std::log(
        1.0 +
        (
            3.0 -
            2.0 +
            0.5
        ) /
        (
            2.0 +
            0.5
        )
    );

    assert(
        std::abs(
            term.idf - expected_idf
        ) < 1e-12
    );

    assert(
        std::abs(
            term.max_score -
            (2.0 * expected_idf)
        ) < 1e-12
    );
}

void test_average_document_length_is_cached() {
    auto index = make_index();

    atlas::QueryPreparer preparer(index);

    const atlas::QueryTerms terms{
        "cat"
    };

    const atlas::PreparedQuery prepared =
        preparer.prepare(terms);

    assert(
        prepared.average_document_length() ==
        index.average_document_length()
    );

    assert(
        prepared.document_count() ==
        index.document_count()
    );
}

void test_term_views() {
    auto index = make_index();

    atlas::QueryPreparer preparer(index);

    const atlas::QueryTerms terms{
        "CAT",
        "DOG"
    };

    const atlas::PreparedQuery prepared =
        preparer.prepare(terms);

    const atlas::QueryTerms views =
        prepared.term_views();

    assert(views.size() == 2);
    assert(views[0] == "cat");
    assert(views[1] == "dog");
}

void test_invalid_normalization_is_ignored() {
    auto index = make_index();

    atlas::QueryPreparer preparer(index);

    /*
     * This input contains two tokens, so it cannot become one
     * PreparedQueryTerm.
     */
    const atlas::QueryTerms terms{
        "hello world"
    };

    const atlas::PreparedQuery prepared =
        preparer.prepare(terms);

    assert(prepared.empty());
}

} // namespace

int main() {
    test_query_terms_are_normalized_and_deduplicated();
    test_query_parser_terms_are_prepared();
    test_missing_terms_are_safe();
    test_metadata_is_cached();
    test_average_document_length_is_cached();
    test_term_views();
    test_invalid_normalization_is_ignored();

    std::cout
        << "prepared_query_test passed\n";

    return 0;
}