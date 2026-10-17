
const express = require("express");

const {
    mongodbUri,
    port,
    host,
    atlasWorker,
    atlasIndex,
    atlasVectorIndex,
    embeddingUrl,
    atlasRuntimePath
} = require("./config");

const {
    MongoDBDatabase
} = require(
    "../../database_adapter"
);

const AtlasSearch =
    require("./atlas_search");

const app = express();

app.use(express.json());

const database =
    new MongoDBDatabase(
        mongodbUri
    );

const searchEngine =
    new AtlasSearch(
        atlasWorker,
        atlasIndex,
        atlasVectorIndex,
        atlasRuntimePath
    );

const RRF_K = 60;
const HYBRID_CANDIDATE_LIMIT = 100;

function reciprocalRankFusion(lexicalResults, semanticResults, limit) {
    const fused = new Map();

    function addRankedList(results, source) {
        results.forEach((result, index) => {
            const id = Number(result.documentId);

            if (!Number.isSafeInteger(id) || id <= 0) {
                return;
            }

            let entry = fused.get(id);

            if (!entry) {
                entry = {
                    documentId: id,
                    score: 0,
                    lexicalScore: null,
                    semanticScore: null,
                    lexicalRank: null,
                    semanticRank: null,
                };

                fused.set(id, entry);
            }

            const rank = index + 1;
            entry.score += 1 / (RRF_K + rank);

            if (source === "lexical") {
                entry.lexicalScore = result.score;
                entry.lexicalRank = rank;
            } else {
                entry.semanticScore = result.score;
                entry.semanticRank = rank;
            }
        });
    }

    addRankedList(lexicalResults, "lexical");
    addRankedList(semanticResults, "semantic");

    return [...fused.values()]
        .sort((a, b) =>
            (b.score - a.score) ||
            (a.documentId - b.documentId)
        )
        .slice(0, limit);
}

app.get(
    "/api/health",
    async (req, res) => {
        try {
            const document =
                await database.getDocumentById(1);

            res.json({
                status: "ok",
                database: "connected",
                searchEngine: "ready",
                documentStore:
                    document
                        ? "available"
                        : "empty"
            });
        }
        catch (error) {
            res.status(503).json({
                status: "error",
                database: "unavailable",
                searchEngine: "unknown",
                error: error.message
            });
        }
    }
);


app.post("/api/search", async (req, res) => {
    try {
        const {
            query,
            limit = 10,
            mode = "HYBRID",
        } = req.body;

        if (typeof query !== "string" || !query.trim()) {
            return res.status(400).json({
                error: "query must be a non-empty string",
            });
        }

        if (!Number.isInteger(limit) || limit < 1 || limit > 100) {
            return res.status(400).json({
                error: "limit must be an integer from 1 to 100",
            });
        }

        if (!["HYBRID", "LEXICAL", "SEMANTIC"].includes(mode)) {
            return res.status(400).json({
                error: "mode must be HYBRID, LEXICAL or SEMANTIC",
            });
        }

        async function getQueryEmbedding(text) {
            const response = await fetch(`${embeddingUrl}/embedding`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ input: [text] }),
            });

            if (!response.ok) {
                throw new Error(
                    `Embedding service returned ${response.status}`
                );
            }

            const data = await response.json();

            // Support common llama.cpp embedding response shapes.
            let embedding;

            if (Array.isArray(data) && data[0]?.embedding) {
                embedding = data[0].embedding;
            } else if (Array.isArray(data?.embedding)) {
                embedding = data.embedding;
            } else if (Array.isArray(data?.data?.[0]?.embedding)) {
                embedding = data.data[0].embedding;
            }

            // Some responses wrap the vector in one additional array.
            if (
                Array.isArray(embedding) &&
                embedding.length === 1 &&
                Array.isArray(embedding[0])
            ) {
                embedding = embedding[0];
            }

            if (
                !Array.isArray(embedding) ||
                embedding.length !== 768 ||
                !embedding.every(
                    value => Number.isFinite(value)
                )
            ) {
                throw new Error(
                    "Invalid embedding response: expected 768 finite numbers"
                );
            }

            return embedding;
        }

        let search;
        let fusionStats = null;

        if (mode === "LEXICAL") {
            search = await searchEngine.search(query, { limit });
        } else if (mode === "SEMANTIC") {
            const embedding = await getQueryEmbedding(query);

            search = await searchEngine.semanticSearch(
                embedding,
                { limit }
            );
        } else {
            // The worker currently processes one request at a time,
            // so retrieve each candidate list sequentially.
            const candidateLimit = Math.max(
                limit,
                HYBRID_CANDIDATE_LIMIT
            );

            const lexical = await searchEngine.search(
                query,
                { limit: candidateLimit }
            );

            const embedding = await getQueryEmbedding(query);

            const semantic = await searchEngine.semanticSearch(
                embedding,
                { limit: candidateLimit }
            );

            const fusedResults = reciprocalRankFusion(
                lexical.results,
                semantic.results,
                limit
            );

            search = {
                results: fusedResults,
                stats: null,
            };

            fusionStats = {
                algorithm: "RRF",
                constant: RRF_K,
                lexicalCandidates: lexical.results.length,
                semanticCandidates: semantic.results.length,
                lexicalStats: lexical.stats,
                semanticStats: semantic.stats,
            };
        }

        const ids = search.results.map(result => result.documentId);

        const documents = await database.getDocumentsByIds(ids);

        const documentMap = new Map(
            documents.map(document => [
                Number(document._id),
                document,
            ])
        );

        const results = search.results.map(result => ({
            ...result,
            document: documentMap.get(result.documentId) || null,
        }));

        res.json({
            query,
            mode,
            results,
            stats: fusionStats || search.stats,
        });
    } catch (error) {
        console.error("Search failed:", error);

        res.status(500).json({
            error: error.message,
        });
    }
});

app.get(
    "/api/documents/:id",
    async (req, res) => {
        try {
            const id =
                Number(req.params.id);

            if (
                !Number.isInteger(id) ||
                id < 1
            ) {
                return res.status(400).json({
                    error:
                        "document id must be a positive integer"
                });
            }

            const document =
                await database
                    .getDocumentById(id);

            if (!document) {
                return res.status(404).json({
                    error:
                        "document not found"
                });
            }

            res.json(document);
        }
        catch (error) {
            res.status(500).json({
                error: error.message
            });
        }
    }
);

async function start() {
    await database.connect();

    searchEngine.start();

    app.listen(
        port,
        host,
        () => {
            console.log(
                `Atlas API listening on http://localhost:${port}`
            );
        }
    );
}

async function shutdown() {
    console.log(
        "Shutting down Atlas API..."
    );

    searchEngine.close();

    await database.close();

    process.exit(0);
}

process.on(
    "SIGINT",
    shutdown
);

process.on(
    "SIGTERM",
    shutdown
);

start().catch(
    (error) => {
        console.error(
            "Failed to start Atlas API:"
        );

        console.error(error);

        process.exit(1);
    }
);
