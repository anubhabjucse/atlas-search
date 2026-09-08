const fs = require("fs");
const path = require("path");
const readline = require("readline");
const { performance } = require("perf_hooks");

function formatBytes(bytes) {
    const units = ["B", "KB", "MB", "GB"];

    let value = bytes;
    let unit = 0;

    while (value >= 1024 && unit < units.length - 1) {
        value /= 1024;
        unit++;
    }

    return `${value.toFixed(2)} ${units[unit]}`;
}

function sanitizeText(value) {
    if (value === null || value === undefined) {
        return "";
    }

    return String(value)
        .replace(/\r\n/g, "\n")
        .replace(/\r/g, "\n")
        .trim();
}

async function main() {
    const inputPath = "D:\\ANUBHAB\\Documents\\atlas\\api\\database_adapter\\data\\news\\News_Category_Dataset_v3.json";
    const outputDirectory = "D:\\ANUBHAB\\Documents\\atlas\\api\\database_adapter\\data\\news\\text\\";

    if (!inputPath || !outputDirectory) {
        console.error(
            "Usage: node json_to_text.js <input-jsonl> <output-directory>"
        );
        process.exit(1);
    }

    const startTime = performance.now();

    const resolvedInput = path.resolve(inputPath);
    const resolvedOutput = path.resolve(outputDirectory);

    console.log("Atlas News Dataset Converter");
    console.log("-----------------------------");
    console.log(`Input : ${resolvedInput}`);
    console.log(`Output: ${resolvedOutput}`);
    console.log();

    if (!fs.existsSync(resolvedInput)) {
        console.error(`Input file does not exist: ${resolvedInput}`);
        process.exit(1);
    }

    fs.mkdirSync(resolvedOutput, { recursive: true });

    const inputStats = fs.statSync(resolvedInput);

    console.log(`Input size: ${formatBytes(inputStats.size)}`);
    console.log("Reading JSON Lines...");
    console.log();

    const input = fs.createReadStream(resolvedInput, {
        encoding: "utf8"
    });

    const rl = readline.createInterface({
        input,
        crlfDelay: Infinity
    });

    let lineNumber = 0;
    let documentsWritten = 0;
    let invalidRecords = 0;
    let missingHeadlines = 0;

    let totalBytes = 0;
    let totalHeadlineBytes = 0;

    let largestBytes = 0;
    let largestDocumentId = 0;

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

            console.warn(
                `Skipping invalid record at line ${lineNumber}`
            );

            continue;
        }

        const headline = sanitizeText(record.headline);
        const description = sanitizeText(record.short_description);

        if (!headline) {
            missingHeadlines++;

            console.warn(
                `Skipping record at line ${lineNumber}: missing headline`
            );

            continue;
        }

        documentsWritten++;

        /*
         * The document ID is deterministic and starts at 1.
         *
         * This ID will later be useful when Atlas returns
         * search results and the Node.js API needs to retrieve
         * the corresponding MongoDB document.
         */
        const documentId = documentsWritten;

        const content = `${headline}\n\n${description}\n`;

        const outputPath = path.join(
            resolvedOutput,
            `${String(documentId).padStart(6, "0")}.txt`
        );

        fs.writeFileSync(outputPath, content, "utf8");

        const bytes = Buffer.byteLength(content, "utf8");
        const headlineBytes = Buffer.byteLength(headline, "utf8");

        totalBytes += bytes;
        totalHeadlineBytes += headlineBytes;

        if (bytes > largestBytes) {
            largestBytes = bytes;
            largestDocumentId = documentId;
        }

        if (documentsWritten % 10000 === 0) {
            console.log(
                `  ${documentsWritten.toLocaleString()} documents written`
            );
        }
    }

    const elapsedSeconds =
        (performance.now() - startTime) / 1000;

    const averageBytes =
        documentsWritten === 0
            ? 0
            : totalBytes / documentsWritten;

    const averageHeadlineBytes =
        documentsWritten === 0
            ? 0
            : totalHeadlineBytes / documentsWritten;

    console.log();
    console.log("Conversion complete.");
    console.log("-------------------");
    console.log(`Input lines         : ${lineNumber.toLocaleString()}`);
    console.log(`Documents written   : ${documentsWritten.toLocaleString()}`);
    console.log(`Invalid JSON lines  : ${invalidRecords.toLocaleString()}`);
    console.log(`Missing headlines   : ${missingHeadlines.toLocaleString()}`);
    console.log(`Total text size     : ${formatBytes(totalBytes)}`);
    console.log(`Average document    : ${formatBytes(averageBytes)}`);
    console.log(`Average headline    : ${formatBytes(averageHeadlineBytes)}`);
    console.log(`Largest document    : ${formatBytes(largestBytes)}`);
    console.log(`Largest document ID : ${largestDocumentId}`);
    console.log(`Processing time     : ${elapsedSeconds.toFixed(2)} seconds`);
    console.log();
    console.log("Output directory:");
    console.log(resolvedOutput);
}

main().catch((error) => {
    console.error();
    console.error("Conversion failed:");
    console.error(error);
    process.exit(1);
});