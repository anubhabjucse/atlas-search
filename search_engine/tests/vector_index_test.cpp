#include "atlas/vector_index.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace {

struct LegacyHeader {
    char magic[8];
    uint32_t version;
    uint32_t dimensions;
    uint64_t document_count;
};

static_assert(sizeof(LegacyHeader) == 24);

void write_legacy_v1(const std::filesystem::path& path) {
    LegacyHeader header{
        {'A', 'T', 'L', 'V', 'E', 'C', '0', '1'},
        1,
        3,
        2
    };

    const uint32_t ids[] = {11, 12};

    // V1 stores raw float32 vectors; the loader normalizes and
    // quantizes them as it reads each row.
    const float vectors[] = {
        3.0f, 0.0f, 0.0f,
        0.0f, 4.0f, 0.0f
    };

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    assert(output);

    output.write(
        reinterpret_cast<const char*>(&header),
        sizeof(header)
    );

    output.write(
        reinterpret_cast<const char*>(ids),
        sizeof(ids)
    );

    output.write(
        reinterpret_cast<const char*>(vectors),
        sizeof(vectors)
    );

    assert(output);
}

} // namespace

int main() {
    const std::filesystem::path v2_path =
        "vector_index_test_v2.atlas";

    const std::filesystem::path v1_path =
        "vector_index_test_v1.atlas";

    const std::filesystem::path bad_path =
        "vector_index_test_truncated.atlas";

    {
        atlas::VectorIndex index;

        index.add(1, {1.0f, 0.0f, 0.0f});
        index.add(2, {0.0f, 1.0f, 0.0f});
        index.add(
            3,
            {0.70710677f, 0.70710677f, 0.0f}
        );

        const auto results = index.search(
            {1.0f, 0.0f, 0.0f},
            3
        );

        assert(results.size() == 3);
        assert(results[0].document_id == 1);
        assert(results[0].score > 0.99f);

        assert(results[1].document_id == 3);
        assert(results[1].score > 0.70f);
        assert(results[1].score < 0.72f);

        assert(results[2].document_id == 2);
        assert(std::fabs(results[2].score) < 1e-5f);

        const auto top_one = index.search(
            {0.0f, 1.0f, 0.0f},
            1
        );

        assert(top_one.size() == 1);
        assert(top_one[0].document_id == 2);

        index.save(v2_path.string());
    }

    {
        const auto loaded =
            atlas::VectorIndex::load(v2_path.string());

        assert(loaded.size() == 3);
        assert(loaded.dimensions() == 3);

        const auto results = loaded.search(
            {1.0f, 0.0f, 0.0f},
            2
        );

        assert(results.size() == 2);
        assert(results[0].document_id == 1);
        assert(results[1].document_id == 3);
    }

    {
        write_legacy_v1(v1_path);

        const auto loaded =
            atlas::VectorIndex::load(v1_path.string());

        assert(loaded.size() == 2);
        assert(loaded.dimensions() == 3);

        const auto results = loaded.search(
            {1.0f, 0.0f, 0.0f},
            2
        );

        assert(results.size() == 2);
        assert(results[0].document_id == 11);
        assert(results[0].score > 0.99f);
        assert(results[1].document_id == 12);
    }

    {
        std::ofstream output(
            bad_path,
            std::ios::binary | std::ios::trunc
        );

        output << "ATLVEC01";
        output.close();

        bool rejected = false;

        try {
            (void)atlas::VectorIndex::load(bad_path.string());
        } catch (const std::exception&) {
            rejected = true;
        }

        assert(rejected);
    }

    std::filesystem::remove(v2_path);
    std::filesystem::remove(v1_path);
    std::filesystem::remove(bad_path);

    return 0;
}