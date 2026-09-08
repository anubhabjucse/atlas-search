const express = require("express");

const {
    mongodbUri,
    port,
    atlasWorker,
    atlasIndex,
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
        atlasRuntimePath
    );

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

app.post(
    "/api/search",
    async (req, res) => {
        try {
            const {
                query,
                limit = 10
            } = req.body;

            if (
                typeof query !== "string" ||
                query.trim().length === 0
            ) {
                return res.status(400).json({
                    error:
                        "query must be a non-empty string"
                });
            }

            if (
                !Number.isInteger(limit) ||
                limit < 1 ||
                limit > 100
            ) {
                return res.status(400).json({
                    error:
                        "limit must be an integer from 1 to 100"
                });
            }

            const search =
                await searchEngine.search(
                    query,
                    { limit }
                );

            const ids =
                search.results.map(
                    (result) =>
                        result.documentId
                );

            const documents =
                await database
                    .getDocumentsByIds(ids);

            const documentMap =
                new Map(
                    documents.map(
                        (document) => [
                            Number(document._id),
                            document
                        ]
                    )
                );

            const results =
                search.results.map(
                    (result) => ({
                        documentId:
                            result.documentId,
                        score:
                            result.score,
                        document:
                            documentMap.get(
                                result.documentId
                            ) || null
                    })
                );

            res.json({
                query,
                results,
                stats: search.stats
            });
        }
        catch (error) {
            console.error(
                "Search failed:",
                error
            );

            res.status(500).json({
                error: error.message
            });
        }
    }
);

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