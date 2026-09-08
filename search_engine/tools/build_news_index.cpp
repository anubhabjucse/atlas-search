#include "atlas/search_engine.hpp"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{

bool extract_json_string(
    const std::string& line,
    std::string_view key,
    std::string& value)
{
    const std::string key_pattern =
        "\"" + std::string(key) + "\"";

    const std::size_t key_position =
        line.find(key_pattern);

    if (key_position == std::string::npos)
        return false;

    std::size_t colon =
        line.find(
            ':',
            key_position + key_pattern.size());

    if (colon == std::string::npos)
        return false;

    std::size_t start = colon + 1;

    while (
        start < line.size() &&
        std::isspace(
            static_cast<unsigned char>(line[start])))
    {
        ++start;
    }

    if (
        start >= line.size() ||
        line[start] != '"')
    {
        return false;
    }

    ++start;

    std::string result;
    result.reserve(256);

    bool escaped = false;

    for (std::size_t i = start;
         i < line.size();
         ++i)
    {
        const char c = line[i];

        if (!escaped)
        {
            if (c == '"')
            {
                value = std::move(result);
                return true;
            }

            if (c == '\\')
            {
                escaped = true;
                result.push_back(c);
                continue;
            }

            result.push_back(c);
            continue;
        }

        escaped = false;
        result.push_back(c);
    }

    return false;
}

std::string extract_field(
    const std::string& line,
    std::string_view key)
{
    std::string value;

    if (!extract_json_string(line, key, value))
        return {};

    return value;
}

}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 3)
        {
            std::cerr
                << "Usage: build_news_index "
                << "<dataset-jsonl> <output-index>\n";

            return EXIT_FAILURE;
        }

        const std::filesystem::path dataset_path(argv[1]);
        const std::filesystem::path output_path(argv[2]);

        if (!std::filesystem::exists(dataset_path))
        {
            throw std::runtime_error(
                "dataset does not exist: " +
                dataset_path.string());
        }

        std::ifstream input(
            dataset_path,
            std::ios::in | std::ios::binary);

        if (!input)
        {
            throw std::runtime_error(
                "unable to open dataset: " +
                dataset_path.string());
        }

        atlas::SearchEngine search_engine;

        std::string line;

        std::size_t line_number = 0;
        std::size_t documents_indexed = 0;
        std::size_t invalid_records = 0;
        std::size_t missing_headlines = 0;

        std::cout
            << "Atlas News Index Builder\n"
            << "========================\n"
            << "Dataset : "
            << dataset_path
            << "\n"
            << "Output  : "
            << output_path
            << "\n\n";

        while (std::getline(input, line))
        {
            ++line_number;

            if (line.empty())
                continue;

            const std::string headline =
                extract_field(line, "headline");

            if (headline.empty())
            {
                ++missing_headlines;
                continue;
            }

            const std::string description =
                extract_field(
                    line,
                    "short_description");

            /*
             * The MongoDB importer assigns IDs sequentially
             * starting at 1, skipping records without headlines.
             *
             * We deliberately reproduce that exact rule.
             */
            const atlas::DocumentId document_id =
                static_cast<atlas::DocumentId>(
                    documents_indexed + 1);

            std::string content;
            content.reserve(
                headline.size() +
                description.size() +
                2);

            content += headline;
            content += '\n';
            content += description;

            search_engine.index_document(
                document_id,
                content);

            ++documents_indexed;

            if (documents_indexed % 5000 == 0)
            {
                std::cout
                    << "  Indexed "
                    << documents_indexed
                    << " documents...\n";

                std::cout.flush();
            }
        }

        search_engine.finalize();

        std::filesystem::create_directories(
            output_path.parent_path());

        search_engine.save(output_path);

        std::cout
            << "\nIndex build complete.\n"
            << "--------------------\n"
            << "Input lines       : "
            << line_number
            << '\n'
            << "Documents indexed : "
            << documents_indexed
            << '\n'
            << "Missing headlines : "
            << missing_headlines
            << '\n'
            << "Vocabulary        : "
            << search_engine.vocabulary_size()
            << '\n'
            << "Output            : "
            << output_path
            << '\n';

        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "Index build failed: "
            << error.what()
            << '\n';

        return EXIT_FAILURE;
    }
}