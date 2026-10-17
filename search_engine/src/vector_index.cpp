#include "atlas/vector_index.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <queue>
#include <stdexcept>
#include <vector>

namespace atlas {
namespace {

struct VectorIndexHeader {
    char magic[8];
    uint32_t version;
    uint32_t dimensions;
    uint64_t document_count;
};

static_assert(sizeof(VectorIndexHeader) == 24);

constexpr char kMagic[8] = {
    'A', 'T', 'L', 'V', 'E', 'C', '0', '1'
};

constexpr std::size_t kHeaderSize = sizeof(VectorIndexHeader);

float vector_norm(
    const float* values,
    std::size_t dimensions
) {
    double squared_norm = 0.0;

    for (std::size_t i = 0; i < dimensions; ++i) {
        if (!std::isfinite(values[i])) {
            throw std::invalid_argument(
                "Embedding contains a non-finite value"
            );
        }

        squared_norm +=
            static_cast<double>(values[i]) * values[i];
    }

    return static_cast<float>(std::sqrt(squared_norm));
}

std::vector<int8_t> normalize_and_quantize(
    const float* values,
    std::size_t dimensions
) {
    const float norm = vector_norm(values, dimensions);

    if (!std::isfinite(norm) || norm <= 0.0f) {
        throw std::invalid_argument(
            "Embedding must have a finite non-zero norm"
        );
    }

    std::vector<int8_t> result(dimensions);

    for (std::size_t i = 0; i < dimensions; ++i) {
        const float normalized = values[i] / norm;

        const long quantized = std::lround(
            normalized * 127.0f
        );

        result[i] = static_cast<int8_t>(
            std::clamp(quantized, -127L, 127L)
        );
    }

    return result;
}

void validate_header(
    const VectorIndexHeader& header,
    uint64_t file_size
) {
    if (!std::equal(
            std::begin(kMagic),
            std::end(kMagic),
            std::begin(header.magic))) {
        throw std::runtime_error(
            "Invalid vector index magic"
        );
    }

    if (header.version != 1 && header.version != 2) {
        throw std::runtime_error(
            "Unsupported vector index version"
        );
    }

    if (header.document_count >
        static_cast<uint64_t>(
            std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error(
            "Vector index document count exceeds platform limits"
        );
    }

    const uint64_t count = header.document_count;
    const uint64_t dimensions = header.dimensions;
    const uint64_t bytes_per_component =
        header.version == 1 ? sizeof(float) : sizeof(int8_t);

    if (count > 0 && dimensions == 0) {
        throw std::runtime_error(
            "Non-empty vector index has zero dimensions"
        );
    }

    // Guard every multiplication and addition before computing
    // the expected payload length.
    constexpr uint64_t max_u64 =
        std::numeric_limits<uint64_t>::max();

    if (count > max_u64 / sizeof(uint32_t)) {
        throw std::runtime_error(
            "Vector index document-ID payload overflows"
        );
    }

    const uint64_t ids_bytes = count * sizeof(uint32_t);

    if (dimensions != 0 &&
        count > max_u64 / dimensions) {
        throw std::runtime_error(
            "Vector index component count overflows"
        );
    }

    const uint64_t components = count * dimensions;

    if (components > max_u64 / bytes_per_component) {
        throw std::runtime_error(
            "Vector index vector payload overflows"
        );
    }

    const uint64_t vectors_bytes =
        components * bytes_per_component;

    if (ids_bytes > max_u64 - kHeaderSize ||
        vectors_bytes > max_u64 - kHeaderSize - ids_bytes) {
        throw std::runtime_error(
            "Vector index expected file size overflows"
        );
    }

    const uint64_t expected_size =
        kHeaderSize + ids_bytes + vectors_bytes;

    if (file_size != expected_size) {
        throw std::runtime_error(
            "Vector index file length does not match its header"
        );
    }
}

float dot_product_quantized(
    const float* query,
    const int8_t* document,
    std::size_t dimensions
) {
    double result = 0.0;

    for (std::size_t i = 0; i < dimensions; ++i) {
        result += static_cast<double>(query[i]) *
                  static_cast<double>(document[i]);
    }

    return static_cast<float>(result / 127.0);
}

} // namespace

void VectorIndex::reserve(std::size_t document_count) {
    document_ids_.reserve(document_count);

    if (dimensions_ != 0) {
        if (document_count >
            std::numeric_limits<std::size_t>::max() / dimensions_) {
            throw std::length_error(
                "Vector index reserve size overflows"
            );
        }

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

    const auto quantized = normalize_and_quantize(
        embedding.data(),
        dimensions_
    );

    document_ids_.push_back(document_id);
    embeddings_.insert(
        embeddings_.end(),
        quantized.begin(),
        quantized.end()
    );
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

    // Keep only the best top_k results while scoring every document.
    // The heap's top is the worst currently retained result, so a new
    // candidate replaces it only when the candidate ranks higher.
    const auto ranks_better = [](const VectorSearchResult& lhs,
                                 const VectorSearchResult& rhs) {
        if (lhs.score != rhs.score) {
            return lhs.score > rhs.score;
        }
        return lhs.document_id < rhs.document_id;
    };

    const auto heap_compare = [&ranks_better](
        const VectorSearchResult& lhs,
        const VectorSearchResult& rhs) {
        return ranks_better(lhs, rhs);
    };

    std::priority_queue<
        VectorSearchResult,
        std::vector<VectorSearchResult>,
        decltype(heap_compare)
    > best(heap_compare);

    for (std::size_t i = 0; i < document_ids_.size(); ++i) {
        const int8_t* document_embedding =
            embeddings_.data() + i * dimensions_;

        const float similarity =
            dot_product_quantized(
                query.data(),
                document_embedding,
                dimensions_
            ) / query_norm;

        const VectorSearchResult candidate{
            document_ids_[i],
            similarity
        };

        if (best.size() < top_k) {
            best.push(candidate);
        } else if (ranks_better(candidate, best.top())) {
            best.pop();
            best.push(candidate);
        }
    }

    std::vector<VectorSearchResult> results;
    results.reserve(best.size());
    while (!best.empty()) {
        results.push_back(best.top());
        best.pop();
    }

    std::sort(results.begin(), results.end(), ranks_better);
    return results;
}

void VectorIndex::save(const std::string& path) const {
    if (dimensions_ > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error(
            "Vector index dimensions exceed the file format limit"
        );
    }

    if (document_ids_.size() >
        std::numeric_limits<std::size_t>::max() /
            std::max<std::size_t>(dimensions_, 1)) {
        throw std::runtime_error(
            "Vector index component count overflows"
        );
    }

    if (embeddings_.size() != document_ids_.size() * dimensions_) {
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
    header.dimensions = static_cast<uint32_t>(dimensions_);
    header.document_count =
        static_cast<uint64_t>(document_ids_.size());

    output.write(
        reinterpret_cast<const char*>(&header),
        sizeof(header)
    );

    if (!document_ids_.empty()) {
        output.write(
            reinterpret_cast<const char*>(document_ids_.data()),
            static_cast<std::streamsize>(
                document_ids_.size() * sizeof(uint32_t)
            )
        );
    }

    if (!embeddings_.empty()) {
        output.write(
            reinterpret_cast<const char*>(embeddings_.data()),
            static_cast<std::streamsize>(embeddings_.size())
        );
    }

    if (!output) {
        throw std::runtime_error(
            "Failed to write vector index data"
        );
    }
}

VectorIndex VectorIndex::load(const std::string& path) {
    std::ifstream input(path, std::ios::binary);

    if (!input) {
        throw std::runtime_error(
            "Failed to open vector index: " + path
        );
    }

    const uint64_t file_size =
        std::filesystem::file_size(path);

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

    validate_header(header, file_size);

    VectorIndex index;
    index.dimensions_ = header.dimensions;

    const std::size_t count =
        static_cast<std::size_t>(header.document_count);

    if (count > 0 &&
        index.dimensions_ >
            std::numeric_limits<std::size_t>::max() / count) {
        throw std::runtime_error(
            "Vector index allocation size overflows"
        );
    }

    index.document_ids_.resize(count);

    if (!index.document_ids_.empty()) {
        input.read(
            reinterpret_cast<char*>(index.document_ids_.data()),
            static_cast<std::streamsize>(
                index.document_ids_.size() * sizeof(uint32_t)
            )
        );
    }

    if (!input) {
        throw std::runtime_error(
            "Failed to read vector index document IDs"
        );
    }

    const std::size_t component_count =
        count * index.dimensions_;

    index.embeddings_.resize(component_count);

    if (header.version == 2) {
        if (!index.embeddings_.empty()) {
            input.read(
                reinterpret_cast<char*>(index.embeddings_.data()),
                static_cast<std::streamsize>(
                    index.embeddings_.size()
                )
            );
        }

        if (!input) {
            throw std::runtime_error(
                "Failed to read quantized vector payload"
            );
        }

        for (const int8_t value : index.embeddings_) {
            if (value == std::numeric_limits<int8_t>::min()) {
                throw std::runtime_error(
                    "Quantized vector payload contains invalid -128 value"
                );
            }
        }
    } else {
        // Backward-compatible V1 conversion.
        // Read one float32 vector at a time so the old matrix is
        // never duplicated as a full float32 heap allocation.
        std::vector<float> row(index.dimensions_);

        for (std::size_t i = 0; i < count; ++i) {
            input.read(
                reinterpret_cast<char*>(row.data()),
                static_cast<std::streamsize>(
                    row.size() * sizeof(float)
                )
            );

            if (!input) {
                throw std::runtime_error(
                    "Failed to read legacy float32 vector payload"
                );
            }

            std::vector<int8_t> quantized;

            try {
                quantized = normalize_and_quantize(
                    row.data(),
                    row.size()
                );
            } catch (const std::exception& error) {
                throw std::runtime_error(
                    std::string("Invalid legacy vector at row ") +
                    std::to_string(i) + ": " + error.what()
                );
            }

            std::copy(
                quantized.begin(),
                quantized.end(),
                index.embeddings_.begin() +
                    static_cast<std::ptrdiff_t>(
                        i * index.dimensions_
                    )
            );
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