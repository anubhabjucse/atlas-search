
#include "atlas/index_builder.hpp"

#include <stdexcept>

namespace atlas {

IndexBuilder::IndexBuilder(
    std::size_t posting_block_size
)
    : tokenizer_(),
      index_(posting_block_size),
      finalized_(false) {
}

void IndexBuilder::add_document(
    DocumentId document_id,
    std::string_view text
) {
    if (finalized_) {
        throw std::logic_error(
            "cannot add documents after index builder finalization"
        );
    }

    const auto tokens =
        tokenizer_.tokenize(text);

    index_.add(
        document_id,
        tokens
    );
}

void IndexBuilder::finalize() {
    finalized_ = true;
}

bool IndexBuilder::is_finalized() const noexcept {
    return finalized_;
}

const Index&
IndexBuilder::index() const noexcept {
    return index_;
}

InMemoryIndex&
IndexBuilder::index() noexcept {
    return index_;
}

std::size_t
IndexBuilder::document_count() const noexcept {
    return index_.document_count();
}

std::size_t
IndexBuilder::vocabulary_size() const noexcept {
    return index_.vocabulary_size();
}
void IndexBuilder::save(
    const std::filesystem::path& path
) const {
    index_.save(path);
}

void IndexBuilder::load(
    const std::filesystem::path& path
) {
    if (finalized_) {
        throw std::logic_error(
            "cannot load index after index builder finalization"
        );
    }

    index_.load(path);

    /*
     * A loaded index is already built and searchable.
     *
     * Treat loading exactly like the completion of indexing.
     */
    finalized_ = true;
}

} // namespace atlas

