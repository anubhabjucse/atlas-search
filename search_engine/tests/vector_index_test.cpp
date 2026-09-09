#include "atlas/vector_index.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <vector>

int main() {
    atlas::VectorIndex index;

    index.add(
        1,
        {1.0f, 0.0f, 0.0f}
    );

    index.add(
        2,
        {0.0f, 1.0f, 0.0f}
    );

    index.add(
        3,
        {0.70710677f, 0.70710677f, 0.0f}
    );

    const auto results =
        index.search(
            {1.0f, 0.0f, 0.0f},
            3
        );

    assert(results.size() == 3);

    assert(results[0].document_id == 1);
    assert(std::fabs(results[0].score - 1.0f) < 1e-5f);

    assert(results[1].document_id == 3);
    assert(results[1].score > 0.70f);
    assert(results[1].score < 0.72f);

    assert(results[2].document_id == 2);
    assert(std::fabs(results[2].score) < 1e-5f);

    const auto top_one =
        index.search(
            {0.0f, 1.0f, 0.0f},
            1
        );

    assert(top_one.size() == 1);
    assert(top_one[0].document_id == 2);

    const std::filesystem::path path =
        "vector_index_test.atlas";

    index.save(path.string());

    const auto loaded =
        atlas::VectorIndex::load(path.string());

    assert(loaded.size() == 3);
    assert(loaded.dimensions() == 3);

    const auto loaded_results =
        loaded.search(
            {1.0f, 0.0f, 0.0f},
            2
        );

    assert(loaded_results.size() == 2);
    assert(loaded_results[0].document_id == 1);
    assert(loaded_results[1].document_id == 3);

    std::filesystem::remove(path);

    return 0;
}