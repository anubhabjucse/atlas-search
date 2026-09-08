#include "atlas/query.hpp"

#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>

namespace atlas {

std::unique_ptr<QueryNode>
QueryNode::term_node(std::string term) {
    auto node =
        std::make_unique<QueryNode>();

    node->type =
        QueryNodeType::TERM;

    node->term =
        std::move(term);

    return node;
}

std::unique_ptr<QueryNode>
QueryNode::phrase_node(std::string phrase) {
    auto node =
        std::make_unique<QueryNode>();

    node->type =
        QueryNodeType::PHRASE;

    node->term =
        std::move(phrase);

    return node;
}

std::unique_ptr<QueryNode>
QueryNode::not_node(
    std::unique_ptr<QueryNode> child
) {
    auto node =
        std::make_unique<QueryNode>();

    node->type =
        QueryNodeType::NOT;

    node->children.push_back(
        std::move(child)
    );

    return node;
}

std::unique_ptr<QueryNode>
QueryNode::and_node(
    std::unique_ptr<QueryNode> left,
    std::unique_ptr<QueryNode> right
) {
    auto node =
        std::make_unique<QueryNode>();

    node->type =
        QueryNodeType::AND;

    node->children.push_back(
        std::move(left)
    );

    node->children.push_back(
        std::move(right)
    );

    return node;
}

std::unique_ptr<QueryNode>
QueryNode::or_node(
    std::unique_ptr<QueryNode> left,
    std::unique_ptr<QueryNode> right
) {
    auto node =
        std::make_unique<QueryNode>();

    node->type =
        QueryNodeType::OR;

    node->children.push_back(
        std::move(left)
    );

    node->children.push_back(
        std::move(right)
    );

    return node;
}

namespace {

enum class TokenType {
    TERM,
    PHRASE,
    AND,
    OR,
    NOT,
    LPAREN,
    RPAREN
};

struct Token {
    TokenType type;
    std::string text;
};

std::string uppercase_ascii(
    std::string_view value
) {
    std::string result;

    result.reserve(
        value.size()
    );

    for (const unsigned char ch :
         value) {

        result.push_back(
            static_cast<char>(
                std::toupper(ch)
            )
        );
    }

    return result;
}

bool is_operator(
    std::string_view value,
    std::string_view op
) {
    return uppercase_ascii(value) == op;
}

std::vector<Token> tokenize_query(
    std::string_view query
) {
    std::vector<Token> tokens;

    std::size_t i = 0;

    while (i < query.size()) {

        while (i < query.size() &&
               std::isspace(
                   static_cast<unsigned char>(
                       query[i]
                   )
               )) {
            ++i;
        }

        if (i >= query.size()) {
            break;
        }

        if (query[i] == '(') {
            tokens.push_back(
                {TokenType::LPAREN, {}}
            );
            ++i;
            continue;
        }

        if (query[i] == ')') {
            tokens.push_back(
                {TokenType::RPAREN, {}}
            );
            ++i;
            continue;
        }

        if (query[i] == '"') {

            ++i;

            std::string phrase;

            while (i < query.size() &&
                   query[i] != '"') {

                phrase.push_back(
                    query[i]
                );

                ++i;
            }

            if (i >= query.size()) {
                throw std::invalid_argument(
                    "unterminated query phrase"
                );
            }

            ++i;

            if (phrase.empty()) {
                throw std::invalid_argument(
                    "empty query phrase"
                );
            }

            tokens.push_back(
                {TokenType::PHRASE,
                 std::move(phrase)}
            );

            continue;
        }

        const std::size_t begin = i;

        while (i < query.size() &&
               !std::isspace(
                   static_cast<unsigned char>(
                       query[i]
                   )
               ) &&
               query[i] != '(' &&
               query[i] != ')') {

            ++i;
        }

        std::string word(
            query.substr(
                begin,
                i - begin
            )
        );

        if (is_operator(word, "AND")) {

            tokens.push_back(
                {TokenType::AND, {}}
            );

        } else if (is_operator(word, "OR")) {

            tokens.push_back(
                {TokenType::OR, {}}
            );

        } else if (is_operator(word, "NOT")) {

            tokens.push_back(
                {TokenType::NOT, {}}
            );

        } else {

            tokens.push_back(
                {TokenType::TERM,
                 std::move(word)}
            );
        }
    }

    return tokens;
}

class Parser {
public:
    explicit Parser(
        std::vector<Token> tokens
    )
        : tokens_(std::move(tokens)) {
    }

    std::unique_ptr<QueryNode> parse() {

        if (tokens_.empty()) {
            throw std::invalid_argument(
                "query is empty"
            );
        }

        auto result =
            parse_or();

        if (position_ != tokens_.size()) {
            throw std::invalid_argument(
                "unexpected token in query"
            );
        }

        return result;
    }

private:
    bool at(
        TokenType type
    ) const {
        return position_ < tokens_.size() &&
               tokens_[position_].type == type;
    }

    std::unique_ptr<QueryNode> parse_or() {

        auto left =
            parse_and();

        while (at(TokenType::OR)) {

            ++position_;

            auto right =
                parse_and();

            left =
                QueryNode::or_node(
                    std::move(left),
                    std::move(right)
                );
        }

        return left;
    }

    bool starts_primary() const {

        if (position_ >= tokens_.size()) {
            return false;
        }

        const TokenType type =
            tokens_[position_].type;

        return
            type == TokenType::TERM ||
            type == TokenType::PHRASE ||
            type == TokenType::NOT ||
            type == TokenType::LPAREN;
    }

    std::unique_ptr<QueryNode> parse_and() {

        auto left =
            parse_unary();

        while (true) {

            if (at(TokenType::AND)) {

                ++position_;

                auto right =
                    parse_unary();

                left =
                    QueryNode::and_node(
                        std::move(left),
                        std::move(right)
                    );

                continue;
            }

            /*
             * Adjacent primary expressions imply AND.
             */
            if (starts_primary()) {

                auto right =
                    parse_unary();

                left =
                    QueryNode::and_node(
                        std::move(left),
                        std::move(right)
                    );

                continue;
            }

            break;
        }

        return left;
    }

    std::unique_ptr<QueryNode> parse_unary() {

        if (at(TokenType::NOT)) {

            ++position_;

            if (position_ >= tokens_.size()) {
                throw std::invalid_argument(
                    "NOT requires an operand"
                );
            }

            return QueryNode::not_node(
                parse_unary()
            );
        }

        return parse_primary();
    }

    std::unique_ptr<QueryNode> parse_primary() {

        if (at(TokenType::TERM)) {

            std::string value =
                tokens_[position_].text;

            ++position_;

            return QueryNode::term_node(
                std::move(value)
            );
        }

        if (at(TokenType::PHRASE)) {

            std::string value =
                tokens_[position_].text;

            ++position_;

            return QueryNode::phrase_node(
                std::move(value)
            );
        }

        if (at(TokenType::LPAREN)) {

            ++position_;

            auto result =
                parse_or();

            if (!at(TokenType::RPAREN)) {
                throw std::invalid_argument(
                    "missing closing parenthesis"
                );
            }

            ++position_;

            return result;
        }

        throw std::invalid_argument(
            "expected query term"
        );
    }

    std::vector<Token> tokens_;
    std::size_t position_ = 0;
};

} // namespace

std::unique_ptr<QueryNode>
QueryParser::parse(
    std::string_view query
) const {
    return Parser(
        tokenize_query(query)
    ).parse();
}

} // namespace atlas