#include "atlas/wand_retriever.hpp"

#include "atlas/prepared_query.hpp"

#include <algorithm>
#include <queue>
#include <utility>

namespace atlas {

WANDRetriever::WANDRetriever(
    const Index& index
)
    : index_(index) {
}

std::vector<ScoredResult>
WANDRetriever::search(
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
WANDRetriever::search_prepared(
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
            term.postings->empty()) {
            continue;
        }

        cursors.push_back(
            CursorState{
                &term,
                PostingCursor(
                    *term.postings,
                    index_.posting_block_size(),
                    stats
                ),
                ranking_model.max_score(term)
            }
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

            for (auto& state :
                 cursors) {

                if (
                    state.cursor.valid() &&
                    state.cursor.current()
                            .document_id ==
                        pivot_document
                ) {
                    state.cursor.next();
                }
            }

        } else {

            for (std::size_t i = 0;
                 i < pivot;
                 ++i) {

                cursors[i]
                    .cursor
                    .advance(
                        pivot_document
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