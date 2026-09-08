#include "atlas/query.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

void test_term()
{
    atlas::QueryParser parser;

    auto query = parser.parse("cat");

    assert(query->type == atlas::QueryNodeType::TERM);
    assert(query->term == "cat");
    assert(query->children.empty());
}

void test_and()
{
    atlas::QueryParser parser;

    auto query = parser.parse("cat AND dog");

    assert(query->type == atlas::QueryNodeType::AND);
    assert(query->children.size() == 2);

    assert(
        query->children[0]->type ==
        atlas::QueryNodeType::TERM);

    assert(
        query->children[0]->term == "cat");

    assert(
        query->children[1]->term == "dog");
}

void test_or()
{
    atlas::QueryParser parser;

    auto query = parser.parse("cat OR dog");

    assert(query->type == atlas::QueryNodeType::OR);
    assert(query->children.size() == 2);

    assert(query->children[0]->term == "cat");
    assert(query->children[1]->term == "dog");
}

void test_not()
{
    atlas::QueryParser parser;

    auto query = parser.parse("NOT cat");

    assert(query->type == atlas::QueryNodeType::NOT);
    assert(query->children.size() == 1);

    assert(
        query->children[0]->type ==
        atlas::QueryNodeType::TERM);

    assert(
        query->children[0]->term == "cat");
}

void test_precedence()
{
    atlas::QueryParser parser;

    auto query =
        parser.parse("cat OR dog AND bird");

    assert(
        query->type ==
        atlas::QueryNodeType::OR);

    assert(
        query->children[0]->term == "cat");

    const auto &right =
        query->children[1];

    assert(
        right->type ==
        atlas::QueryNodeType::AND);

    assert(right->children[0]->term == "dog");
    assert(right->children[1]->term == "bird");
}

void test_parentheses()
{
    atlas::QueryParser parser;

    auto query =
        parser.parse("(cat OR dog) AND bird");

    assert(
        query->type ==
        atlas::QueryNodeType::AND);

    assert(
        query->children[0]->type ==
        atlas::QueryNodeType::OR);

    assert(
        query->children[0]->children[0]->term ==
        "cat");

    assert(
        query->children[0]->children[1]->term ==
        "dog");

    assert(
        query->children[1]->term ==
        "bird");
}

void test_invalid_query()
{
    atlas::QueryParser parser;

    bool threw = false;

    try
    {
        parser.parse("cat AND");
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }

    assert(threw);
}

void test_empty_query()
{
    atlas::QueryParser parser;

    bool threw = false;

    try
    {
        parser.parse("   ");
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }

    assert(threw);
}
void test_implicit_and()
{
    atlas::QueryParser parser;

    auto query =
        parser.parse("cat dog");

    assert(
        query->type ==
        atlas::QueryNodeType::AND);

    assert(
        query->children.size() == 2);

    assert(
        query->children[0]->term ==
        "cat");

    assert(
        query->children[1]->term ==
        "dog");
}

void test_phrase()
{
    atlas::QueryParser parser;

    auto query =
        parser.parse("\"cat dog\"");

    assert(
        query->type ==
        atlas::QueryNodeType::PHRASE);

    assert(
        query->term ==
        "cat dog");
}

void test_mixed_term_and_phrase()
{
    atlas::QueryParser parser;

    auto query =
        parser.parse(
            "cat \"dog bird\"");

    assert(
        query->type ==
        atlas::QueryNodeType::AND);

    assert(
        query->children.size() == 2);

    assert(
        query->children[0]->type ==
        atlas::QueryNodeType::TERM);

    assert(
        query->children[1]->type ==
        atlas::QueryNodeType::PHRASE);
}

void test_boolean_not_precedence()
{
    atlas::QueryParser parser;

    auto query =
        parser.parse(
            "cat AND NOT dog");

    assert(
        query->type ==
        atlas::QueryNodeType::AND);

    assert(
        query->children[0]->type ==
        atlas::QueryNodeType::TERM);

    assert(
        query->children[1]->type ==
        atlas::QueryNodeType::NOT);
}
int main()
{
    test_term();
    test_and();
    test_implicit_and();
    test_or();
    test_not();
    test_boolean_not_precedence();
    test_phrase();
    test_mixed_term_and_phrase();
    test_precedence();
    test_parentheses();
    test_invalid_query();
    test_empty_query();

    return 0;
}