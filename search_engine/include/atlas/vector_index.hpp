
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <memory>
#include <vector>

namespace atlas {

struct VectorSearchResult {
    uint32_t document_id;
    float score;
};

class VectorIndex {
public:
    // V1: float32 payload.
    // V2: normalized, signed int8 payload.
    static constexpr uint32_t kFormatVersion = 2;

    VectorIndex() = default;

    void reserve(std::size_t document_count);

    void add(
        uint32_t document_id,
        const std::vector<float>& embedding
    );

    std::vector<VectorSearchResult> search(
        const std::vector<float>& query,
        std::size_t top_k
    ) const;

    void save(const std::string& path) const;

    static VectorIndex load(const std::string& path);

    std::size_t size() const;
    std::size_t dimensions() const;

private:
    std::size_t dimensions_ = 0;

    std::vector<uint32_t> document_ids_;

    // Row-major, one signed byte per dimension.
    // Each document vector is normalized before quantization.
    // V2 loaded indexes can point directly into a read-only mmap. The
    // shared owner keeps the mapping alive across VectorIndex moves/copies.
    std::vector<int8_t> embeddings_;
    std::shared_ptr<void> mapping_owner_;
    const int8_t* mapped_embeddings_ = nullptr;
};

} // namespace atlas
