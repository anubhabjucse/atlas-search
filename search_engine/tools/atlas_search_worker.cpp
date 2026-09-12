#include "atlas/search_engine.hpp"
#include "atlas/vector_index.hpp"

#include <cctype>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sstream>
#include <vector>
#include <cmath>

namespace
{

std::string percent_decode(
    std::string_view input)
{
    std::string output;
    output.reserve(input.size());

    auto hex_value = [](char c) -> int
    {
        if (c >= '0' && c <= '9')
            return c - '0';

        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;

        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;

        return -1;
    };

    for (std::size_t i = 0;
         i < input.size();
         ++i)
    {
        if (input[i] != '%')
        {
            output.push_back(input[i]);
            continue;
        }

        if (i + 2 >= input.size())
            throw std::runtime_error(
                "invalid percent-encoded query");

        const int high =
            hex_value(input[i + 1]);

        const int low =
            hex_value(input[i + 2]);

        if (high < 0 || low < 0)
            throw std::runtime_error(
                "invalid percent-encoded query");

        output.push_back(
            static_cast<char>(
                (high << 4) | low));

        i += 2;
    }

    return output;
}

std::vector<float> parse_vector(
    std::string_view value)
{
    std::vector<float> vector;

    std::istringstream stream{
        std::string(value)
    };

    float component;

    while (stream >> component)
    {
        if (!std::isfinite(component))
        {
            throw std::runtime_error(
                "vector contains non-finite value"
            );
        }

        vector.push_back(component);
    }

    if (vector.empty())
    {
        throw std::runtime_error(
            "empty semantic query vector"
        );
    }

    return vector;
}

std::size_t parse_size(
    std::string_view value)
{
    if (value.empty())
        throw std::runtime_error(
            "empty numeric value");

    std::size_t result = 0;

    for (char c : value)
    {
        if (
            !std::isdigit(
                static_cast<unsigned char>(c)))
        {
            throw std::runtime_error(
                "invalid numeric value");
        }

        const std::size_t digit =
            static_cast<std::size_t>(c - '0');

        if (
            result >
            (std::numeric_limits<std::size_t>::max()
             - digit) / 10)
        {
            throw std::runtime_error(
                "numeric value is too large");
        }

        result = result * 10 + digit;
    }

    return result;
}

atlas::RankingType parse_ranking(
    std::string_view value)
{
    if (value == "TFIDF")
        return atlas::RankingType::TFIDF;

    if (value == "BM25")
        return atlas::RankingType::BM25;

    throw std::runtime_error(
        "unknown ranking type");
}

atlas::RetrievalType parse_retrieval(
    std::string_view value)
{
    if (value == "BOOLEAN")
        return atlas::RetrievalType::BOOLEAN;

    if (value == "TOP_K")
        return atlas::RetrievalType::TOP_K;

    if (value == "WAND")
        return atlas::RetrievalType::WAND;

    if (value == "BLOCK_MAX_WAND")
        return atlas::RetrievalType::BLOCK_MAX_WAND;

    throw std::runtime_error(
        "unknown retrieval type");
}

atlas::SearchMode parse_mode(
    std::string_view value)
{
    if (value == "LEXICAL")
        return atlas::SearchMode::LEXICAL;

    if (value == "PHRASE")
        return atlas::SearchMode::PHRASE;

    if (value == "BOOLEAN")
        return atlas::SearchMode::BOOLEAN;

    throw std::runtime_error(
        "unknown search mode");
}

}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 3)
        {
            std::cerr
                << "Usage: atlas_search_worker "
                << "<lexical-index-path> "
                << "<vector-index-path>\n";

            return EXIT_FAILURE;
        }

        atlas::SearchEngine search_engine;
        atlas::VectorIndex vector_index;

        search_engine.load(argv[1]);

        vector_index =
            atlas::VectorIndex::load(argv[2]);

        std::cerr
            << "Atlas search worker ready\n"
            << "Vector index: "
            << vector_index.size()
            << " documents, "
            << vector_index.dimensions()
            << " dimensions\n";

        std::string line;

        while (std::getline(
            std::cin,
            line))
        {
            if (line.empty())
                continue;

            /*
             * Semantic vector search
             *
             * Protocol:
             *
             * SEMANTIC <k>
             * VECTOR <f1> <f2> ... <fn>
             */
            if (line.rfind(
                    "SEMANTIC\t",
                    0) == 0)
            {
                const std::string parameters =
                    line.substr(9);

                const std::size_t k =
                    parse_size(parameters);

                std::string vector_line;

                if (!std::getline(
                        std::cin,
                        vector_line))
                {
                    break;
                }

                if (
                    vector_line.rfind(
                        "VECTOR\t",
                        0) != 0)
                {
                    std::cout
                        << "ERROR\texpected VECTOR line\n"
                        << std::flush;

                    continue;
                }

                const std::vector<float> query =
                    parse_vector(
                        std::string_view(vector_line)
                            .substr(7)
                    );

                const auto results =
                    vector_index.search(
                        query,
                        k
                    );

                for (const auto& result :
                     results)
                {
                    std::cout
                        << "RESULT\t"
                        << result.document_id
                        << '\t'
                        << result.score
                        << '\n';
                }

                std::cout
                    << "SEMANTIC_STATS\t"
                    << vector_index.size()
                    << '\t'
                    << vector_index.dimensions()
                    << '\n';

                std::cout
                    << "END\n"
                    << std::flush;

                continue;
            }

            /*
             * Existing lexical search
             *
             * Protocol:
             *
             * SEARCH <k> <ranking> <retrieval> <mode>
             * QUERY <query>
             */
            if (line.rfind(
                    "SEARCH\t",
                    0) != 0)
            {
                std::cout
                    << "ERROR\tinvalid SEARCH request\n"
                    << std::flush;

                continue;
            }

            const std::string parameters =
                line.substr(7);

            std::size_t p1 =
                parameters.find('\t');

            std::size_t p2 =
                parameters.find(
                    '\t',
                    p1 + 1);

            std::size_t p3 =
                parameters.find(
                    '\t',
                    p2 + 1);

            if (
                p1 == std::string::npos ||
                p2 == std::string::npos ||
                p3 == std::string::npos)
            {
                std::cout
                    << "ERROR\tinvalid SEARCH parameters\n"
                    << std::flush;

                continue;
            }

            const std::size_t k =
                parse_size(
                    std::string_view(
                        parameters)
                        .substr(0, p1));

            const atlas::RankingType ranking =
                parse_ranking(
                    std::string_view(
                        parameters)
                        .substr(
                            p1 + 1,
                            p2 - p1 - 1));

            const atlas::RetrievalType retrieval =
                parse_retrieval(
                    std::string_view(
                        parameters)
                        .substr(
                            p2 + 1,
                            p3 - p2 - 1));

            const atlas::SearchMode mode =
                parse_mode(
                    std::string_view(
                        parameters)
                        .substr(p3 + 1));

            std::string query_line;

            if (!std::getline(
                    std::cin,
                    query_line))
            {
                break;
            }

            if (
                query_line.rfind(
                    "QUERY\t",
                    0) != 0)
            {
                std::cout
                    << "ERROR\texpected QUERY line\n"
                    << std::flush;

                continue;
            }

            const std::string query =
                percent_decode(
                    std::string_view(
                        query_line)
                        .substr(6));

            atlas::SearchOptions options;

            options.ranking = ranking;
            options.retrieval = retrieval;
            options.mode = mode;

            atlas::RetrievalStats stats;

            const auto results =
                search_engine.search(
                    query,
                    k,
                    options,
                    &stats);

            for (const auto& result :
                 results)
            {
                std::cout
                    << "RESULT\t"
                    << result.document_id
                    << '\t'
                    << result.score
                    << '\n';
            }

            std::cout
                << "STATS\t"
                << stats.documents_scored
                << '\t'
                << stats.postings_visited
                << '\t'
                << stats.blocks_skipped
                << '\t'
                << stats.query_terms
                << '\t'
                << stats.matched_terms
                << '\t'
                << stats.candidates_considered
                << '\n';

            std::cout
                << "END\n"
                << std::flush;
        }

        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "Atlas search worker failed: "
            << error.what()
            << '\n';

        return EXIT_FAILURE;
    }
}