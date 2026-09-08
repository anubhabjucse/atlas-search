const path = require("path");
const dotenv = require("dotenv");

const PROJECT_ROOT =
    path.resolve(__dirname, "../../..");

dotenv.config({
    path: path.join(PROJECT_ROOT, ".env")
});

if (!process.env.MONGODB_URI) {
    throw new Error(
        "MONGODB_URI is not set in the root .env"
    );
}

const config = {
    port:
        Number(process.env.ATLAS_API_PORT || 3000),

    mongodbUri:
        process.env.MONGODB_URI,

    atlasWorker:
        path.resolve(
            PROJECT_ROOT,
            process.env.ATLAS_WORKER_PATH ||
                "./build/atlas_search_worker.exe"
        ),

    atlasIndex:
        path.resolve(
            PROJECT_ROOT,
            process.env.ATLAS_INDEX_PATH ||
                "./data/index/news.atlas"
        ),

    atlasRuntimePath:
        process.env.ATLAS_RUNTIME_PATH || ""
};

module.exports = config;