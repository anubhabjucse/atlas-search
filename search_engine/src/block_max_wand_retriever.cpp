#include "atlas/block_max_wand_retriever.hpp"

#include "atlas/prepared_query.hpp"

#include <algorithm>
#include <queue>
#include <utility>

namespace atlas {

BlockMaxWANDRetriever::BlockMaxWANDRetriever(
    const Index& index
)
    : index_(index) {
}

void BlockMaxWANDRetriever::refresh_block_bound(
    CursorState& state
) {
    if (!state.cursor.valid()) {
        state.current_block_max_score = 0.0;
        return;
    }

    const std::size_t block =
        state.cursor.current_block();

    if (state.blocks == nullptr ||
        block >= state.blocks->size()) {
        state.current_block_max_score = 0.0;
        return;
    }

    state.current_block_max_score =
        state.block_max_scores[block];
}

double BlockMaxWANDRetriever::range_upper_bound(
    const CursorState& state,
    DocumentId minimum_document,
    DocumentId maximum_document
) {
    if (state.blocks == nullptr ||
        state.blocks->empty() ||
        state.block_max_scores.empty()) {
        return 0.0;
    }

    const auto& blocks =
        *state.blocks;

    const auto first_it =
        std::lower_bound(
            blocks.begin(),
            blocks.end(),
            minimum_document,
            [](const PostingBlock& block,
               DocumentId document) {
                return block.last_document < document;
            }
        );

    if (first_it == blocks.end()) {
        return 0.0;
    }

    const auto last_it =
        std::upper_bound(
            first_it,
            blocks.end(),
            maximum_document,
            [](DocumentId document,
               const PostingBlock& block) {
                return document < block.first_document;
            }
        );

    const std::size_t first_block =
        static_cast<std::size_t>(
            first_it - blocks.begin()
        );

    const std::size_t end_block =
        static_cast<std::size_t>(
            last_it - blocks.begin()
        );

    /*
     * Return the maximum possible contribution of this
     * term anywhere in the requested document range.
     *
     * This is deliberately a SUM-independent per-term
     * bound. The caller combines bounds across terms.
     */
    double upper_bound = 0.0;

    for (std::size_t block = first_block;
         block < end_block;
         ++block) {

        upper_bound =
            std::max(
                upper_bound,
                state.block_max_scores[block]
            );
    }

    return upper_bound;
}

std::vector<ScoredResult>
BlockMaxWANDRetriever::search(
    const QueryTerms& query_terms,
    const RankingModel& ranking_model,
    std::size_t k,
    RetrievalStats* stats
) const {
    if (k == 0 ||
        query_terms.empty()) {
        return {};
    }

    QueryPreparer preparer(index_);

    PreparedQuery prepared =
        preparer.prepare(query_terms);

    return search_prepared(
        prepared,
        ranking_model,
        k,
        stats
    );
}

std::vector<ScoredResult>
BlockMaxWANDRetriever::search_prepared(
    const PreparedQuery& query,
    const RankingModel& ranking_model,
    std::size_t k,
    RetrievalStats* stats
) const {
    if (k == 0 ||
        query.empty()) {
        return {};
    }

    std::vector<CursorState> cursors;

    cursors.reserve(
        query.size()
    );

    for (const PreparedQueryTerm& term :
         query.terms()) {

        if (term.postings == nullptr ||
            term.postings->empty() ||
            term.blocks == nullptr ||
            term.blocks->empty()) {
            continue;
        }

        const auto& postings =
            *term.postings;

        const auto& blocks =
            *term.blocks;

        CursorState state{
            &term,
            PostingCursor(
                postings,
                index_.posting_block_size(),
                stats
            ),
            &blocks,
            ranking_model.max_score(term),
            std::vector<double>(
                blocks.size(),
                0.0
            ),
            0.0
        };

        for (std::size_t block = 0;
             block < blocks.size();
             ++block) {

            state.block_max_scores[block] =
                ranking_model.block_max_score(
                    term,
                    blocks[block]
                );
        }

        refresh_block_bound(state);

        cursors.push_back(
            std::move(state)
        );
    }

    if (cursors.empty()) {
        return {};
    }

    auto worse =
        [](const ScoredResult& lhs,
           const ScoredResult& rhs) {

            if (lhs.score != rhs.score) {
                return lhs.score > rhs.score;
            }

                 return lhs.document_id >
                   rhs.document_id;
        };

    std::priority_queue<
        ScoredResult,
        std::vector<ScoredResult>,
        decltype(worse)
    > top_k(worse);

    double threshold = 0.0;

    while (true) {

        /*
         * Remove exhausted posting lists.
         */
        cursors.erase(
            std::remove_if(
                cursors.begin(),
                cursors.end(),
                [](const CursorState& state) {
                    return !state.cursor.valid();
                }
            ),
            cursors.end()
        );

        if (cursors.empty()) {
            break;
        }

        /*
         * WAND requires cursors to be ordered by their
         * current document ID.
         */
        std::sort(
            cursors.begin(),
            cursors.end(),
            [](const CursorState& lhs,
               const CursorState& rhs) {

                return
                    lhs.cursor.current()
                        .document_id
                    <
                    rhs.cursor.current()
                        .document_id;
            }
        );

        /*
         * Compute the correctness-safe global upper bound.
         *
         * We intentionally use the global term bounds here.
         * The block metadata is still prepared and available,
         * but we do not perform speculative range-level skipping.
         */
        double upper_bound = 0.0;

        std::size_t pivot =
            cursors.size();

        for (std::size_t i = 0;
             i < cursors.size();
             ++i) {

            upper_bound +=
                cursors[i].max_score;

            if (upper_bound >= threshold) {
                pivot = i;
                break;
            }
        }

        /*
         * If even the maximum possible score from all
         * remaining terms cannot beat the current threshold,
         * retrieval is complete.
         */
        if (pivot == cursors.size()) {
            break;
        }

        const DocumentId pivot_document =
            cursors[pivot]
                .cursor
                .current()
                .document_id;

        const DocumentId smallest_document =
            cursors.front()
                .cursor
                .current()
                .document_id;

        if (smallest_document ==
            pivot_document) {

            /*
             * Candidate document has reached the WAND pivot.
             * Score it exactly.
             */
            if (stats) {
                ++stats->documents_scored;
            }

            const double score =
                ranking_model.score(
                    pivot_document,
                    query
                );

            if (score > 0.0) {

                const ScoredResult result{
                    pivot_document,
                    score
                };

                if (top_k.size() < k) {

                    top_k.push(result);

                } else {

                    const auto& worst =
                        top_k.top();

                    if (
                        result.score >
                            worst.score ||
                        (
                            result.score ==
                                worst.score &&
                            result.document_id <
                                worst.document_id
                        )
                    ) {
                        top_k.pop();
                        top_k.push(result);
                    }
                }

                if (top_k.size() == k) {
                    threshold =
                        top_k.top().score;
                }
            }

            /*
             * Advance every cursor that matched the
             * candidate document.
             */
            for (auto& state :
                 cursors) {

                if (
                    state.cursor.valid() &&
                    state.cursor.current()
                            .document_id ==
                        pivot_document
                ) {
                    state.cursor.next();

                    refresh_block_bound(
                        state
                    );
                }
            }

        } else {

            /*
             * Advance every cursor before the pivot.
             *
             * This is the standard WAND advancement step.
             */
            for (std::size_t i = 0;
                 i < pivot;
                 ++i) {

                cursors[i]
                    .cursor
                    .advance(
                        pivot_document
                    );

                refresh_block_bound(
                    cursors[i]
                );
            }
        }
    }

    std::vector<ScoredResult> results;

    results.reserve(
        top_k.size()
    );

    while (!top_k.empty()) {

        results.push_back(
            top_k.top()
        );

        top_k.pop();
    }

    std::reverse(
        results.begin(),
        results.end()
    );

    return results;
}

} // namespace atlas