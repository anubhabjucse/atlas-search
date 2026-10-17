#include "atlas/vector_index.hpp"

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr
            << "Usage: quantize_vector_index <input-index> <output-index>\n";
        return 2;
    }

    try {
        const auto index = atlas::VectorIndex::load(argv[1]);
        index.save(argv[2]);

        std::cout
            << "Compact vector index written successfully\n"
            << "Documents: " << index.size() << '\n'
            << "Dimensions: " << index.dimensions() << '\n'
            << "Output: " << argv[2] << '\n';

        return 0;
    } catch (const std::exception& error) {
        std::cerr
            << "Vector index conversion failed: "
            << error.what() << '\n';
        return 1;
    }
}