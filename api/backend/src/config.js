const path = require("path");
const dotenv = require("dotenv");

const PROJECT_ROOT = path.resolve(__dirname, "../../..");

dotenv.config({
  path: path.join(PROJECT_ROOT, ".env"),
});

if (!process.env.MONGODB_URI) {
  throw new Error("MONGODB_URI is not set in the root .env");
}

const config = {
  port: Number(process.env.PORT || process.env.ATLAS_API_PORT || 3000),

  host: process.env.ATLAS_API_HOST || "0.0.0.0",

  mongodbUri: process.env.MONGODB_URI,

  atlasWorker: path.resolve(
    PROJECT_ROOT,
    process.env.ATLAS_WORKER_PATH || "./build/atlas_search_worker",
  ),

  atlasIndex: path.resolve(
    PROJECT_ROOT,
    process.env.ATLAS_INDEX_PATH || "./data/index/news.atlas",
  ),

  atlasVectorIndex: path.resolve(
    PROJECT_ROOT,
    process.env.ATLAS_VECTOR_INDEX_PATH || "./data/index/news.vector",
  ),

  embeddingUrl: process.env.ATLAS_EMBEDDING_URL || "http://127.0.0.1:8080",

  atlasRuntimePath: process.env.ATLAS_RUNTIME_PATH || "",
};

module.exports = config;
