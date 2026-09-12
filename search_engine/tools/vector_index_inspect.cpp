#include "atlas/vector_index.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr
            << "Usage: vector_index_inspect "
            << "<vector-index> <query-vector-file>\n";

        return EXIT_FAILURE;
    }

    try
    {
        const std::string index_path = argv[1];
        const std::string query_path = argv[2];

        const atlas::VectorIndex index =
            atlas::VectorIndex::load(index_path);

        std::ifstream input(query_path);

        if (!input)
        {
            throw std::runtime_error(
                "Failed to open query vector: " +
                query_path);
        }

        std::vector<float> query;
        float value = 0.0f;

        while (input >> value)
        {
            query.push_back(value);
        }

        std::cout
            << "Vector index loaded successfully.\n"
            << "Documents  : "
            << index.size()
            << '\n'
            << "Dimensions : "
            << index.dimensions()
            << '\n'
            << "Query dims : "
            << query.size()
            << "\n\n";

        const auto results =
            index.search(query, 10);

        std::cout
            << "Top results:\n"
            << "------------\n";

        for (const auto& result : results)
        {
            std::cout
                << "Document "
                << result.document_id
                << "  score="
                << result.score
                << '\n';
        }

        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "Vector search failed: "
            << error.what()
            << '\n';

        return EXIT_FAILURE;
    }
}