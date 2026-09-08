#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace atlas
{

    enum class QueryNodeType
    {
        TERM,
        PHRASE,
        AND,
        OR,
        NOT
    };

    struct QueryNode
    {
        QueryNodeType type;

        /*
         * TERM:
         *   one normalized/raw term
         *
         * PHRASE:
         *   the complete phrase text
         */
        std::string term;

        std::vector<std::unique_ptr<QueryNode>> children;
        bool is_leaf() const noexcept
        {
            return type == QueryNodeType::TERM ||
                   type == QueryNodeType::PHRASE;
        }
        static std::unique_ptr<QueryNode>
        term_node(std::string term);

        static std::unique_ptr<QueryNode>
        phrase_node(std::string phrase);

        static std::unique_ptr<QueryNode>
        not_node(std::unique_ptr<QueryNode> child);

        static std::unique_ptr<QueryNode>
        and_node(
            std::unique_ptr<QueryNode> left,
            std::unique_ptr<QueryNode> right);

        static std::unique_ptr<QueryNode>
        or_node(
            std::unique_ptr<QueryNode> left,
            std::unique_ptr<QueryNode> right);
    };

    class QueryParser
    {
    public:
        std::unique_ptr<QueryNode>
        parse(std::string_view query) const;
    };

} // namespace atlas