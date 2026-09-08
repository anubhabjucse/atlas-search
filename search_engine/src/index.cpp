#include "atlas/index.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <unordered_map>

namespace atlas {

InMemoryIndex::InMemoryIndex(
    std::size_t posting_block_size
)
    : posting_block_size_(posting_block_size) {

    if (posting_block_size_ == 0) {
        throw std::invalid_argument(
            "posting block size must be greater than zero"
        );
    }
}

void InMemoryIndex::add(
    DocumentId document_id,
    const std::vector<Token>& tokens
) {
    if (has_documents_ &&
        document_id <= max_document_id_) {

        throw std::invalid_argument(
            "document IDs must be strictly increasing"
        );
    }

    /*
     * Build document-local term data first.
     *
     * Positions are retained because V4 now supports phrase and
     * proximity-aware retrieval.
     */
    std::unordered_map<
        std::string_view,
        std::vector<std::uint32_t>
    > term_positions;

    term_positions.reserve(tokens.size());

    for (const Token& token : tokens) {
        term_positions[token.term].push_back(
            static_cast<std::uint32_t>(token.position)
        );
    }

    for (const auto& [term, positions] :
         term_positions) {

        if (positions.empty()) {
            continue;
        }

        const std::uint32_t frequency =
            static_cast<std::uint32_t>(
                positions.size()
            );

        auto& term_data =
            dictionary_[std::string(term)];

        const std::size_t posting_position =
            term_data.postings.size();

        term_data.postings.push_back(
            Posting{
                document_id,
                frequency,
                positions
            }
        );

        term_data.max_term_frequency =
            std::max(
                term_data.max_term_frequency,
                frequency
            );

        const std::size_t block_index =
            posting_position /
            posting_block_size_;

        if (term_data.blocks.size() <= block_index) {

            PostingBlock block;

            block.begin =
                posting_position;

            block.end =
                posting_position + 1;

            block.first_document =
                document_id;

            block.last_document =
                document_id;

            block.max_term_frequency =
                frequency;

            block.min_document_length =
                static_cast<std::uint32_t>(
                    tokens.size()
                );

            term_data.blocks.push_back(
                block
            );

        } else {

            PostingBlock& block =
                term_data.blocks[block_index];

            block.end =
                posting_position + 1;

            block.max_term_frequency =
                std::max(
                    block.max_term_frequency,
                    frequency
                );

            block.min_document_length =
                std::min(
                    block.min_document_length,
                    static_cast<std::uint32_t>(
                        tokens.size()
                    )
                );

            block.last_document =
                document_id;
        }
    }

    document_lengths_[document_id] =
        tokens.size();

    ++document_count_;

    total_document_length_ +=
        tokens.size();

    if (!has_documents_ ||
        document_id > max_document_id_) {

        max_document_id_ = document_id;
    }

    has_documents_ = true;
}

const std::vector<Posting>&
InMemoryIndex::postings(
    std::string_view term
) const {
    static const std::vector<Posting> empty;

    const auto it =
        dictionary_.find(std::string(term));

    if (it == dictionary_.end()) {
        return empty;
    }

    return it->second.postings;
}

const std::vector<PostingBlock>&
InMemoryIndex::posting_blocks(
    std::string_view term
) const {
    static const std::vector<PostingBlock> empty;

    const auto it =
        dictionary_.find(std::string(term));

    if (it == dictionary_.end()) {
        return empty;
    }

    return it->second.blocks;
}

std::uint32_t
InMemoryIndex::term_frequency(
    std::string_view term,
    DocumentId document_id
) const {
    const auto it =
        dictionary_.find(std::string(term));

    if (it == dictionary_.end()) {
        return 0;
    }

    const auto& postings =
        it->second.postings;

    const auto posting_it =
        std::lower_bound(
            postings.begin(),
            postings.end(),
            document_id,
            [](const Posting& posting,
               DocumentId target) {

                return posting.document_id <
                       target;
            }
        );

    if (posting_it == postings.end() ||
        posting_it->document_id != document_id) {

        return 0;
    }

    return posting_it->term_frequency;
}

const std::vector<std::uint32_t>&
InMemoryIndex::term_positions(
    std::string_view term,
    DocumentId document_id
) const {
    static const std::vector<std::uint32_t> empty;

    const auto it =
        dictionary_.find(std::string(term));

    if (it == dictionary_.end()) {
        return empty;
    }

    const auto& postings =
        it->second.postings;

    const auto posting_it =
        std::lower_bound(
            postings.begin(),
            postings.end(),
            document_id,
            [](const Posting& posting,
               DocumentId target) {

                return posting.document_id <
                       target;
            }
        );

    if (posting_it == postings.end() ||
        posting_it->document_id != document_id) {

        return empty;
    }

    return posting_it->positions;
}

std::uint32_t
InMemoryIndex::max_term_frequency(
    std::string_view term
) const {
    const auto it =
        dictionary_.find(std::string(term));

    if (it == dictionary_.end()) {
        return 0;
    }

    return it->second.max_term_frequency;
}

std::size_t
InMemoryIndex::posting_block_size() const noexcept {
    return posting_block_size_;
}

std::size_t
InMemoryIndex::vocabulary_size() const {
    return dictionary_.size();
}

DocumentId
InMemoryIndex::max_document_id() const {
    return max_document_id_;
}

std::size_t
InMemoryIndex::document_count() const {
    return document_count_;
}

std::size_t
InMemoryIndex::document_length(
    DocumentId document_id
) const {
    const auto it =
        document_lengths_.find(document_id);

    if (it == document_lengths_.end()) {
        return 0;
    }

    return it->second;
}

std::size_t
InMemoryIndex::total_document_length() const {
    return total_document_length_;
}

double
InMemoryIndex::average_document_length() const {
    if (document_count_ == 0) {
        return 0.0;
    }

    return static_cast<double>(
        total_document_length_
    ) / static_cast<double>(
        document_count_
    );
}

std::size_t
InMemoryIndex::document_frequency(
    std::string_view term
) const {
    const auto it =
        dictionary_.find(std::string(term));

    if (it == dictionary_.end()) {
        return 0;
    }

    return it->second.postings.size();
}

} // namespace atlas