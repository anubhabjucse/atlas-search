#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace atlas {

struct VectorSearchResult {
    uint32_t document_id;
    float score;
};

class VectorIndex {
public:
    static constexpr uint32_t kFormatVersion = 1;

    VectorIndex() = default;

    void reserve(std::size_t document_count);

    void add(uint32_t document_id,
             const std::vector<float>& embedding);

    std::vector<VectorSearchResult> search(
        const std::vector<float>& query,
        std::size_t top_k) const;

    void save(const std::string& path) const;

    static VectorIndex load(const std::string& path);

    std::size_t size() const;
    std::size_t dimensions() const;

private:
    std::size_t dimensions_ = 0;

    std::vector<uint32_t> document_ids_;

    // Row-major:
    //
    // [doc0_dim0, doc0_dim1, ...,
    //  doc1_dim0, doc1_dim1, ...]
    //
    std::vector<float> embeddings_;
};

} // namespace atlas