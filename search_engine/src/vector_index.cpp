
#include "atlas/vector_index.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace atlas {
namespace {

struct VectorIndexHeader {
    char magic[8];
    uint32_t version;
    uint32_t dimensions;
    uint64_t document_count;
};

constexpr char kMagic[8] = {
    'A', 'T', 'L', 'V', 'E', 'C', '0', '1'
};

float dot_product(
    const float* lhs,
    const float* rhs,
    std::size_t dimensions
) {
    float result = 0.0f;

    for (std::size_t i = 0; i < dimensions; ++i) {
        result += lhs[i] * rhs[i];
    }

    return result;
}

float vector_norm(
    const float* values,
    std::size_t dimensions
) {
    float squared_norm = 0.0f;

    for (std::size_t i = 0; i < dimensions; ++i) {
        squared_norm += values[i] * values[i];
    }

    return std::sqrt(squared_norm);
}

} // namespace

void VectorIndex::reserve(std::size_t document_count) {
    document_ids_.reserve(document_count);

    if (dimensions_ != 0) {
        embeddings_.reserve(document_count * dimensions_);
    }
}

void VectorIndex::add(
    uint32_t document_id,
    const std::vector<float>& embedding
) {
    if (embedding.empty()) {
        throw std::invalid_argument(
            "VectorIndex cannot add an empty embedding"
        );
    }

    if (dimensions_ == 0) {
        dimensions_ = embedding.size();
    }

    if (embedding.size() != dimensions_) {
        throw std::invalid_argument(
            "Embedding dimensions do not match VectorIndex dimensions"
        );
    }

    const float norm = vector_norm(
        embedding.data(),
        dimensions_
    );

    if (!std::isfinite(norm) || norm <= 0.0f) {
        throw std::invalid_argument(
            "Embedding must have a finite non-zero norm"
        );
    }

    document_ids_.push_back(document_id);

    const std::size_t offset =
        embeddings_.size();

    embeddings_.resize(
        offset + dimensions_
    );

    for (std::size_t i = 0;
         i < dimensions_;
         ++i) {
        embeddings_[offset + i] =
            embedding[i] / norm;
    }
}

std::vector<VectorSearchResult> VectorIndex::search(
    const std::vector<float>& query,
    std::size_t top_k
) const {
    if (query.empty() || top_k == 0 || document_ids_.empty()) {
        return {};
    }

    if (query.size() != dimensions_) {
        throw std::invalid_argument(
            "Query dimensions do not match VectorIndex dimensions"
        );
    }

    const float query_norm = vector_norm(
        query.data(),
        dimensions_
    );

    if (!std::isfinite(query_norm) || query_norm <= 0.0f) {
        throw std::invalid_argument(
            "Query embedding must have a finite non-zero norm"
        );
    }

    top_k = std::min(top_k, document_ids_.size());

    std::vector<VectorSearchResult> results;
    results.reserve(document_ids_.size());

    for (std::size_t i = 0; i < document_ids_.size(); ++i) {
        const float* document_embedding =
            embeddings_.data() + (i * dimensions_);

        const float similarity =
            dot_product(
                query.data(),
                document_embedding,
                dimensions_
            ) / query_norm;

        results.push_back({
            document_ids_[i],
            similarity
        });
    }

    if (results.size() > top_k) {
        std::nth_element(
            results.begin(),
            results.begin() + top_k,
            results.end(),
            [](const VectorSearchResult& lhs,
               const VectorSearchResult& rhs) {
                return lhs.score > rhs.score;
            }
        );

        results.resize(top_k);
    }

    std::sort(
        results.begin(),
        results.end(),
        [](const VectorSearchResult& lhs,
           const VectorSearchResult& rhs) {
            if (lhs.score != rhs.score) {
                return lhs.score > rhs.score;
            }

            return lhs.document_id < rhs.document_id;
        }
    );

    return results;
}

void VectorIndex::save(const std::string& path) const {
    if (dimensions_ == 0 && !document_ids_.empty()) {
        throw std::runtime_error(
            "VectorIndex has documents but no dimensions"
        );
    }

    if (
        embeddings_.size() !=
        document_ids_.size() * dimensions_
    ) {
        throw std::runtime_error(
            "VectorIndex internal storage is inconsistent"
        );
    }

    std::ofstream output(
        path,
        std::ios::binary | std::ios::trunc
    );

    if (!output) {
        throw std::runtime_error(
            "Failed to open vector index for writing: " + path
        );
    }

    VectorIndexHeader header{};

    std::copy(
        std::begin(kMagic),
        std::end(kMagic),
        std::begin(header.magic)
    );

    header.version = kFormatVersion;
    header.dimensions =
        static_cast<uint32_t>(dimensions_);
    header.document_count =
        static_cast<uint64_t>(document_ids_.size());

    output.write(
        reinterpret_cast<const char*>(&header),
        sizeof(header)
    );

    if (!output) {
        throw std::runtime_error(
            "Failed to write vector index header"
        );
    }

    if (!document_ids_.empty()) {
        output.write(
            reinterpret_cast<const char*>(
                document_ids_.data()
            ),
            static_cast<std::streamsize>(
                document_ids_.size() *
                sizeof(uint32_t)
            )
        );
    }

    if (!embeddings_.empty()) {
        output.write(
            reinterpret_cast<const char*>(
                embeddings_.data()
            ),
            static_cast<std::streamsize>(
                embeddings_.size() *
                sizeof(float)
            )
        );
    }

    if (!output) {
        throw std::runtime_error(
            "Failed to write vector index data"
        );
    }
}

VectorIndex VectorIndex::load(const std::string& path) {
    std::ifstream input(
        path,
        std::ios::binary
    );

    if (!input) {
        throw std::runtime_error(
            "Failed to open vector index: " + path
        );
    }

    VectorIndexHeader header{};

    input.read(
        reinterpret_cast<char*>(&header),
        sizeof(header)
    );

    if (!input) {
        throw std::runtime_error(
            "Failed to read vector index header"
        );
    }

    if (
        !std::equal(
            std::begin(kMagic),
            std::end(kMagic),
            std::begin(header.magic)
        )
    ) {
        throw std::runtime_error(
            "Invalid vector index magic"
        );
    }

    if (header.version != kFormatVersion) {
        throw std::runtime_error(
            "Unsupported vector index version"
        );
    }

    VectorIndex index;

    index.dimensions_ = header.dimensions;

    index.document_ids_.resize(
        static_cast<std::size_t>(
            header.document_count
        )
    );

    index.embeddings_.resize(
        static_cast<std::size_t>(
            header.document_count
        ) * index.dimensions_
    );

    if (!index.document_ids_.empty()) {
        input.read(
            reinterpret_cast<char*>(
                index.document_ids_.data()
            ),
            static_cast<std::streamsize>(
                index.document_ids_.size() *
                sizeof(uint32_t)
            )
        );
    }

    if (!index.embeddings_.empty()) {
        input.read(
            reinterpret_cast<char*>(
                index.embeddings_.data()
            ),
            static_cast<std::streamsize>(
                index.embeddings_.size() *
                sizeof(float)
            )
        );
    }

    if (!input) {
        throw std::runtime_error(
            "Failed to read vector index data"
        );
    }

    /*
     * Existing vector indexes may contain raw,
     * unnormalized embeddings. Normalize every
     * document vector once while loading so that
     * VectorIndex maintains the invariant:
     *
     *     ||document_embedding|| = 1
     *
     * This allows search() to calculate cosine
     * similarity as:
     *
     *     dot(query, document) / ||query||
     */
    for (std::size_t i = 0;
         i < index.document_ids_.size();
         ++i) {
        float* embedding =
            index.embeddings_.data() +
            (i * index.dimensions_);

        const float norm = vector_norm(
            embedding,
            index.dimensions_
        );

        if (!std::isfinite(norm) || norm <= 0.0f) {
            throw std::runtime_error(
                "Vector index contains an invalid embedding"
            );
        }

        for (std::size_t j = 0;
             j < index.dimensions_;
             ++j) {
            embedding[j] /= norm;
        }
    }

    return index;
}

std::size_t VectorIndex::size() const {
    return document_ids_.size();
}

std::size_t VectorIndex::dimensions() const {
    return dimensions_;
}

} // namespace atlas

