#include "atlas/top_k.hpp"

#include "atlas/prepared_query.hpp"

#include <algorithm>
#include <queue>

namespace atlas {

TopKRetriever::TopKRetriever(
    const Index& index
)
    : index_(index) {
}

std::vector<ScoredResult>
TopKRetriever::search(
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
TopKRetriever::search_prepared(
    const PreparedQuery& query,
    const RankingModel& ranking_model,
    std::size_t k,
    RetrievalStats* stats
) const {
    if (k == 0 ||
        query.empty()) {
        return {};
    }

    /*
     * Build a contiguous candidate buffer.
     *
     * Every PreparedQueryTerm already has its posting list
     * resolved, so no additional dictionary lookup is needed.
     */
    std::size_t total_postings = 0;

    for (const PreparedQueryTerm& term :
         query.terms()) {

        if (term.postings != nullptr) {
            total_postings +=
                term.postings->size();
        }
    }

    if (total_postings == 0) {
        return {};
    }

    std::vector<DocumentId> candidates;

    candidates.reserve(
        total_postings
    );

    for (const PreparedQueryTerm& term :
         query.terms()) {

        if (term.postings == nullptr) {
            continue;
        }

        const auto& postings =
            *term.postings;

        if (stats) {
            stats->postings_visited +=
                postings.size();
        }

        for (const Posting& posting :
             postings) {

            candidates.push_back(
                posting.document_id
            );
        }
    }

    std::sort(
        candidates.begin(),
        candidates.end()
    );

    candidates.erase(
        std::unique(
            candidates.begin(),
            candidates.end()
        ),
        candidates.end()
    );

    if (candidates.empty()) {
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

    for (const DocumentId document_id :
         candidates) {

        if (stats) {
            ++stats->documents_scored;
        }

        const double score =
            ranking_model.score(
                document_id,
                query
            );

        if (score <= 0.0) {
            continue;
        }

        const ScoredResult result{
            document_id,
            score
        };

        if (top_k.size() < k) {

            top_k.push(result);

            continue;
        }

        const auto& worst =
            top_k.top();

        if (result.score > worst.score ||
            (
                result.score == worst.score &&
                result.document_id <
                    worst.document_id
            )) {

            top_k.pop();
            top_k.push(result);
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