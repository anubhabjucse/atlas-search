const fs = require("fs");
const path = require("path");
const readline = require("readline");
const { MongoClient } = require("mongodb");
const dotenv = require("dotenv");
const { performance } = require("perf_hooks");

const PROJECT_ROOT = path.resolve(__dirname, "../../..");

dotenv.config({
    path: path.join(PROJECT_ROOT, ".env")
});

const MONGODB_URI = process.env.MONGODB_URI;

const DATABASE_NAME = "atlas";
const COLLECTION_NAME = "news";

const INPUT_FILE = path.join(
    PROJECT_ROOT,
    "api",
    "database_adapter",
    "data",
    "news",
    "News_Category_Dataset_v3.json"
);

if (!MONGODB_URI) {
    console.error("MONGODB_URI is not set in the root .env file.");
    process.exit(1);
}

function sanitizeText(value) {
    if (value === null || value === undefined) {
        return "";
    }

    return String(value).trim();
}

async function main() {
    const startTime = performance.now();

    console.log("Atlas MongoDB News Importer");
    console.log("---------------------------");
    console.log(`Input file : ${INPUT_FILE}`);
    console.log(`Database   : ${DATABASE_NAME}`);
    console.log(`Collection : ${COLLECTION_NAME}`);
    console.log();

    if (!fs.existsSync(INPUT_FILE)) {
        console.error(`Input file does not exist: ${INPUT_FILE}`);
        process.exit(1);
    }

    const client = new MongoClient(MONGODB_URI);

    try {
        console.log("Connecting to MongoDB...");

        await client.connect();

        await client.db("admin").command({
            ping: 1
        });

        console.log("MongoDB connection successful.");
        console.log();

        const database = client.db(DATABASE_NAME);
        const collection = database.collection(COLLECTION_NAME);

        /*
         * Start clean for this first controlled dataset import.
         *
         * This makes the result deterministic while we are
         * developing the database adapter.
         */
        console.log("Clearing existing news collection...");

        await collection.deleteMany({});

        console.log("Collection cleared.");
        console.log();

        const input = fs.createReadStream(INPUT_FILE, {
            encoding: "utf8"
        });

        const rl = readline.createInterface({
            input,
            crlfDelay: Infinity
        });

        let lineNumber = 0;
        let documentId = 0;
        let invalidRecords = 0;
        let missingHeadlines = 0;
        let insertedDocuments = 0;

        const batch = [];
        const BATCH_SIZE = 1000;

        async function flushBatch() {
            if (batch.length === 0) {
                return;
            }

            const operations = batch.map((document) => ({
                insertOne: {
                    document
                }
            }));

            await collection.bulkWrite(operations, {
                ordered: true
            });

            insertedDocuments += batch.length;
            batch.length = 0;

            if (insertedDocuments % 10000 === 0) {
                console.log(
                    `  ${insertedDocuments.toLocaleString()} documents inserted`
                );
            }
        }

        for await (const line of rl) {
            lineNumber++;

            const trimmedLine = line.trim();

            if (!trimmedLine) {
                continue;
            }

            let record;

            try {
                record = JSON.parse(trimmedLine);
            } catch (error) {
                invalidRecords++;

                console.warn(
                    `Skipping invalid JSON at line ${lineNumber}: ${error.message}`
                );

                continue;
            }

            if (!record || typeof record !== "object") {
                invalidRecords++;
                continue;
            }

            const headline = sanitizeText(record.headline);

            if (!headline) {
                missingHeadlines++;

                console.warn(
                    `Skipping record at line ${lineNumber}: missing headline`
                );

                continue;
            }

            documentId++;

            batch.push({
                _id: documentId,
                headline,
                short_description: sanitizeText(
                    record.short_description
                ),
                category: sanitizeText(record.category),
                authors: sanitizeText(record.authors),
                date: sanitizeText(record.date),
                link: sanitizeText(record.link)
            });

            if (batch.length >= BATCH_SIZE) {
                await flushBatch();
            }
        }

        await flushBatch();

        /*
         * Indexes we'll need for the application layer.
         *
         * Atlas itself remains responsible for full-text search.
         * MongoDB is primarily our document store.
         */
        console.log();
        console.log("Creating MongoDB indexes...");

        await collection.createIndex({
            date: 1
        });

        await collection.createIndex({
            category: 1
        });

        console.log("Indexes created.");

        const totalDocuments =
            await collection.countDocuments();

        const elapsedSeconds =
            (performance.now() - startTime) / 1000;

        console.log();
        console.log("Import complete.");
        console.log("----------------");
        console.log(`Input lines        : ${lineNumber.toLocaleString()}`);
        console.log(`Documents inserted : ${insertedDocuments.toLocaleString()}`);
        console.log(`Documents in DB    : ${totalDocuments.toLocaleString()}`);
        console.log(`Invalid JSON       : ${invalidRecords.toLocaleString()}`);
        console.log(`Missing headlines  : ${missingHeadlines.toLocaleString()}`);
        console.log(`Processing time    : ${elapsedSeconds.toFixed(2)} seconds`);
        console.log();
        console.log(`MongoDB database   : ${DATABASE_NAME}`);
        console.log(`MongoDB collection : ${COLLECTION_NAME}`);
    } catch (error) {
        console.error();
        console.error("MongoDB import failed:");
        console.error(error);

        process.exitCode = 1;
    } finally {
        await client.close();
        console.log();
        console.log("MongoDB connection closed.");
    }
}

main().catch((error) => {
    console.error(error);
    process.exit(1);
});