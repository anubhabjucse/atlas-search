
#include "atlas/search_engine.hpp"
#include "atlas/tokenizer.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <stdexcept>

namespace atlas
{

    SearchEngine::SearchEngine()
        : index_builder_(),
          query_parser_(),
          boolean_retriever_(
              index_builder_.index()),
          query_preparer_(
              index_builder_.index()),
          phrase_retriever_(
              index_builder_.index()),
          tfidf_ranker_(
              index_builder_.index()),
          bm25_ranker_(
              index_builder_.index()),
          top_k_retriever_(
              index_builder_.index()),
          wand_retriever_(
              index_builder_.index()),
          block_max_wand_retriever_(
              index_builder_.index())
    {
    }

    void SearchEngine::index_document(
        DocumentId document_id,
        std::string_view text)
    {
        if (index_builder_.is_finalized())
        {
            throw std::logic_error(
                "cannot index documents after search engine finalization");
        }

        index_builder_.add_document(
            document_id,
            text);
    }

    void SearchEngine::finalize()
    {
        index_builder_.finalize();
    }

    bool SearchEngine::is_finalized() const noexcept
    {
        return index_builder_.is_finalized();
    }

    void SearchEngine::save(
        const std::filesystem::path &path) const
    {
        index_builder_.save(path);
    }

    void SearchEngine::load(
        const std::filesystem::path &path)
    {
        if (index_builder_.is_finalized())
        {
            throw std::logic_error(
                "cannot load index after search engine finalization");
        }

        index_builder_.load(path);
    }

    std::vector<DocumentId>
    SearchEngine::search(
        std::string_view query) const
    {
        const auto ast =
            query_parser_.parse(query);

        return evaluate_boolean(*ast);
    }

    std::vector<ScoredResult>
    SearchEngine::search(
        std::string_view query,
        std::size_t k,
        const SearchOptions &options,
        RetrievalStats *stats) const
    {
        if (k == 0)
        {
            return {};
        }

        if (stats)
        {
            stats->reset();
        }

        /*
         * Explicit query syntax is interpreted through the AST.
         *
         * Ordinary free text:
         *     Tata cars
         *
         * remains a normal ranked lexical query.
         *
         * Explicit syntax:
         *     "Tata cars"
         *     Tata AND cars
         *     Tata OR cars
         *     Tata NOT electric
         *     Tata "electric cars"
         *
         * is evaluated according to the parsed query structure.
         */
        if (has_explicit_query_syntax(query))
        {
            const auto ast =
                query_parser_.parse(query);

            if (!ast)
            {
                return {};
            }

            const PreparedQuery prepared =
                query_preparer_.prepare(*ast);

            const ResultSet candidates =
                evaluate_boolean(*ast);

            if (stats)
            {
                stats->query_terms =
                    prepared.size();

                stats->candidates_considered =
                    candidates.size();

                for (const PreparedQueryTerm &term :
                     prepared.terms())
                {
                    if (term.document_frequency > 0)
                    {
                        ++stats->matched_terms;
                    }
                }
            }

            return rank_candidates(
                candidates,
                prepared,
                k,
                ranking_model(options.ranking),
                stats);
        }

        /*
         * Ordinary free-text search.
         *
         * Each token is an independent lexical term.
         * Terms do not need to occur next to each other.
         */
        Tokenizer tokenizer;

        const std::vector<Token> tokens =
            tokenizer.tokenize(query);

        QueryTerms query_terms;
        query_terms.reserve(tokens.size());

        for (const Token &token : tokens)
        {
            query_terms.push_back(token.term);
        }

        const PreparedQuery prepared =
            query_preparer_.prepare(query_terms);

        if (stats)
        {
            stats->query_terms =
                prepared.size();

            for (const PreparedQueryTerm &term :
                 prepared.terms())
            {
                if (term.document_frequency > 0)
                {
                    ++stats->matched_terms;
                }
            }
        }

        return ranked_search(
            prepared,
            k,
            options,
            stats);
    }

    std::vector<ScoredResult>
    SearchEngine::search(
        std::string_view query,
        std::size_t k) const
    {
        return search(
            query,
            k,
            SearchOptions{},
            nullptr);
    }

    std::vector<std::string>
    SearchEngine::normalize_phrase(
        std::string_view phrase) const
    {
        Tokenizer tokenizer;

        const auto tokens =
            tokenizer.tokenize(phrase);

        std::vector<std::string> result;

        result.reserve(
            tokens.size());

        for (const Token &token :
             tokens)
        {
            result.push_back(
                token.term);
        }

        return result;
    }

    bool
    SearchEngine::matches_phrase(
        DocumentId document_id,
        const std::vector<std::string> &terms) const
    {
        if (terms.empty())
        {
            return false;
        }

        if (terms.size() == 1)
        {
            return index_builder_.index().term_frequency(
                       terms.front(),
                       document_id) > 0;
        }

        const auto &first_positions =
            index_builder_.index().term_positions(
                terms.front(),
                document_id);

        for (const std::uint32_t start :
             first_positions)
        {
            bool matches = true;

            for (std::size_t i = 1;
                 i < terms.size();
                 ++i)
            {
                const auto &positions =
                    index_builder_.index().term_positions(
                        terms[i],
                        document_id);

                const std::uint32_t expected =
                    start +
                    static_cast<std::uint32_t>(i);

                if (!std::binary_search(
                        positions.begin(),
                        positions.end(),
                        expected))
                {
                    matches = false;
                    break;
                }
            }

            if (matches)
            {
                return true;
            }
        }

        return false;
    }

    std::vector<ScoredResult>
    SearchEngine::search_phrase(
        std::string_view phrase,
        std::size_t k,
        RetrievalStats *stats) const
    {
        if (k == 0)
        {
            return {};
        }

        const auto terms =
            normalize_phrase(phrase);

        if (terms.empty())
        {
            return {};
        }

        if (stats)
        {
            stats->reset();
            stats->query_terms =
                terms.size();
        }

        /*
         * Phrase retrieval is a positional candidate-generation step.
         *
         * Ranking remains the responsibility of RankingModel. This keeps
         * phrase semantics separate from ranking semantics while ensuring
         * phrase results use exactly the same ranking implementation as
         * ordinary lexical results.
         */
        const auto documents =
            phrase_retriever_.search(
                terms,
                stats);

        if (stats)
        {
            stats->candidates_considered =
                documents.size();

            stats->matched_terms =
                terms.size();
        }

        QueryTerms query_terms;
        query_terms.reserve(
            terms.size());

        for (const std::string &term : terms)
        {
            query_terms.push_back(term);
        }

        const PreparedQuery prepared =
            query_preparer_.prepare(query_terms);

        const RankingModel &ranker =
            bm25_ranker_;

        return rank_candidates(
            documents,
            prepared,
            k,
            ranker,
            stats);
    }

    SearchExplanation
    SearchEngine::explain(
        std::string_view query,
        DocumentId document_id,
        RankingType ranking) const
    {
        SearchExplanation explanation;

        explanation.document_id =
            document_id;

        const auto ast =
            query_parser_.parse(query);

        if (!ast)
        {
            return explanation;
        }

        const PreparedQuery prepared =
            query_preparer_.prepare(*ast);

        explanation.query_terms =
            prepared.size();

        const bool explicit_syntax =
            has_explicit_query_syntax(query);

        /*
         * Explain the prepared query terms.
         *
         * PreparedQueryTerm already contains the normalized term,
         * document frequency, and precomputed IDF.
         */
        for (const PreparedQueryTerm &term :
             prepared.terms())
        {
            if (term.document_frequency == 0)
            {
                continue;
            }

            const std::uint32_t tf =
                index_builder_.index().term_frequency(
                    term.term,
                    document_id);

            if (tf == 0)
            {
                continue;
            }

            explanation.matched_term_names.push_back(
                term.term);

            explanation.term_frequencies.push_back(
                tf);

            explanation.term_idfs.push_back(
                term.idf);

            ++explanation.matched_terms;
        }

        /*
         * matched_query means that the complete prepared lexical
         * query matches this document.
         *
         * matched_terms tells us how many query terms matched.
         * matched_query tells us whether all query terms matched.
         */
        explanation.matched_query =
            !prepared.terms().empty() &&
            explanation.matched_terms ==
                explanation.query_terms;

        /*
         * Always obtain the lexical score from the selected
         * RankingModel.
         */
        explanation.lexical_score =
            ranking_model(ranking).score(
                document_id,
                prepared);

        explanation.final_score =
            explanation.lexical_score;

        /*
         * Keep retrieval information consistent for every
         * explanation mode.
         */
        explanation.retrieval.query_terms =
            explanation.query_terms;

        explanation.retrieval.matched_terms =
            explanation.matched_terms;

        /*
         * Ordinary free-text queries are purely lexical.
         */
        if (!explicit_syntax)
        {
            explanation.mode =
                ExplanationMode::LEXICAL;

            explanation.phrase_match =
                false;

            return explanation;
        }

        /*
         * Explicit syntax must be evaluated through the AST so that
         * AND / OR / NOT / PHRASE semantics match normal search().
         */
        const ResultSet candidates =
            evaluate_boolean(*ast);

        explanation.matched_query =
            std::find(
                candidates.begin(),
                candidates.end(),
                document_id) != candidates.end();

        /*
         * Determine whether the AST contains a phrase that actually
         * matches this document positionally.
         */
        std::function<bool(const QueryNode &)>
            contains_matching_phrase =
                [&](const QueryNode &node) -> bool
        {
            if (node.type ==
                QueryNodeType::PHRASE)
            {
                const auto terms =
                    normalize_phrase(node.term);

                return matches_phrase(
                    document_id,
                    terms);
            }

            for (const auto &child :
                 node.children)
            {
                if (child &&
                    contains_matching_phrase(*child))
                {
                    return true;
                }
            }

            return false;
        };

        explanation.phrase_match =
            contains_matching_phrase(*ast);

        /*
         * Phrase mode takes precedence when the query contains a
         * phrase that matches positionally.
         *
         * Otherwise explicit Boolean syntax is represented as
         * BOOLEAN mode.
         */
        if (explanation.phrase_match)
        {
            explanation.mode =
                ExplanationMode::PHRASE;
        }
        else
        {
            explanation.mode =
                ExplanationMode::BOOLEAN;
        }

        return explanation;
    }

    std::size_t
    SearchEngine::vocabulary_size() const noexcept
    {
        return index_builder_.vocabulary_size();
    }

    std::size_t
    SearchEngine::document_count() const noexcept
    {
        return index_builder_.document_count();
    }

    DocumentId
    SearchEngine::max_document_id() const noexcept
    {
        return index_builder_.index().max_document_id();
    }

    double
    SearchEngine::average_document_length() const noexcept
    {
        return index_builder_.index().average_document_length();
    }

    const RankingModel &
    SearchEngine::ranking_model(
        RankingType type) const
    {
        switch (type)
        {
        case RankingType::TFIDF:
            return tfidf_ranker_;

        case RankingType::BM25:
            return bm25_ranker_;
        }

        throw std::logic_error(
            "unknown ranking type");
    }

    const RankedRetriever &
    SearchEngine::ranked_retriever(
        RetrievalType type) const
    {
        switch (type)
        {
        case RetrievalType::TOP_K:
            return top_k_retriever_;

        case RetrievalType::WAND:
            return wand_retriever_;

        case RetrievalType::BLOCK_MAX_WAND:
            return block_max_wand_retriever_;

        case RetrievalType::BOOLEAN:
            break;
        }

        throw std::invalid_argument(
            "Boolean retrieval is not a ranked retriever");
    }

    std::vector<ScoredResult>
    SearchEngine::rank_candidates(
        const std::vector<DocumentId> &candidates,
        const PreparedQuery &prepared,
        std::size_t k,
        const RankingModel &ranker,
        RetrievalStats *stats) const
    {
        if (k == 0 ||
            candidates.empty() ||
            prepared.empty())
        {
            return {};
        }

        std::vector<ScoredResult> results;

        results.reserve(
            std::min(
                k,
                candidates.size()));

        for (const DocumentId document_id :
             candidates)
        {
            if (stats)
            {
                ++stats->documents_scored;
            }

            const double score =
                ranker.score(
                    document_id,
                    prepared);

            if (score <= 0.0)
            {
                continue;
            }

            results.push_back(
                ScoredResult{
                    document_id,
                    score});
        }

        std::sort(
            results.begin(),
            results.end(),
            [](const ScoredResult &lhs,
               const ScoredResult &rhs)
            {
                if (lhs.score != rhs.score)
                {
                    return lhs.score > rhs.score;
                }

                return lhs.document_id <
                       rhs.document_id;
            });

        if (results.size() > k)
        {
            results.resize(k);
        }

        return results;
    }

    std::vector<ScoredResult>
    SearchEngine::ranked_search(
        const PreparedQuery &prepared,
        std::size_t k,
        const SearchOptions &options,
        RetrievalStats *stats) const
    {
        if (options.retrieval ==
            RetrievalType::BOOLEAN)
        {
            throw std::invalid_argument(
                "Boolean retrieval does not produce scored results");
        }

        const RankingModel &ranker =
            ranking_model(
                options.ranking);

        const RankedRetriever &retriever =
            ranked_retriever(
                options.retrieval);

        return retriever.search_prepared(
            prepared,
            ranker,
            k,
            stats);
    }

    ResultSet
    SearchEngine::evaluate_boolean(
        const QueryNode &node) const
    {
        switch (node.type)
        {
        case QueryNodeType::TERM:
        {
            const auto terms =
                normalize_phrase(node.term);

            if (terms.size() != 1)
            {
                return {};
            }

            return boolean_retriever_.term(
                terms.front());
        }

        case QueryNodeType::PHRASE:
        {
            const auto terms =
                normalize_phrase(node.term);

            if (terms.empty())
            {
                return {};
            }

            return phrase_retriever_.search(
                terms,
                nullptr);
        }

        case QueryNodeType::AND:
        {
            if (node.children.empty())
            {
                return {};
            }

            ResultSet result =
                evaluate_boolean(
                    *node.children.front());

            for (std::size_t i = 1;
                 i < node.children.size();
                 ++i)
            {
                result =
                    boolean_retriever_.intersect(
                        result,
                        evaluate_boolean(
                            *node.children[i]));
            }

            return result;
        }

        case QueryNodeType::OR:
        {
            ResultSet result;

            for (const auto &child :
                 node.children)
            {
                if (!child)
                {
                    continue;
                }

                result =
                    boolean_retriever_.unite(
                        result,
                        evaluate_boolean(*child));
            }

            return result;
        }

        case QueryNodeType::NOT:
        {
            if (node.children.size() != 1 ||
                !node.children.front())
            {
                return {};
            }

            return boolean_retriever_.complement(
                evaluate_boolean(
                    *node.children.front()));
        }
        }

        return {};
    }

    bool
    SearchEngine::has_explicit_query_syntax(
        std::string_view query) const
    {
        if (query.find('"') != std::string_view::npos ||
            query.find('(') != std::string_view::npos ||
            query.find(')') != std::string_view::npos)
        {
            return true;
        }

        std::size_t i = 0;

        while (i < query.size())
        {
            while (i < query.size() &&
                   std::isspace(
                       static_cast<unsigned char>(
                           query[i])))
            {
                ++i;
            }

            if (i >= query.size())
            {
                break;
            }

            const std::size_t begin =
                i;

            while (i < query.size() &&
                   !std::isspace(
                       static_cast<unsigned char>(
                           query[i])))
            {
                ++i;
            }

            const std::string_view word =
                query.substr(
                    begin,
                    i - begin);

            if (word == "AND" ||
                word == "and" ||
                word == "OR" ||
                word == "or" ||
                word == "NOT" ||
                word == "not")
            {
                return true;
            }
        }

        return false;
    }

} // namespace atlas
