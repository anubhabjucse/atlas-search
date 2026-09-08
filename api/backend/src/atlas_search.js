const { spawn } = require("child_process");


const readline =
    require("readline");

const path =
    require("path");
class AtlasSearch {
    constructor(workerPath, indexPath,runtimePath="") {
        this.workerPath = workerPath;
        this.indexPath = indexPath;
        this.runtimePath = runtimePath;

        this.process = null;
        this.readline = null;
        this.pending = null;
    }

    start() {
        if (this.process) {
            return;
        }

        const path = require("path");

const workerDirectory = path.dirname(this.workerPath);

const runtimePaths = [
    workerDirectory,
    this.runtimePath
].filter(Boolean);

this.process = spawn(
    this.workerPath,
    [this.indexPath],
    {
        stdio: ["pipe", "pipe", "inherit"],
        cwd: workerDirectory,
        env: {
            ...process.env,
            PATH: [
                ...runtimePaths,
                process.env.PATH || ""
            ].join(path.delimiter)
        }
    }
);

        this.readline =
            readline.createInterface({
                input: this.process.stdout
            });

        this.readline.on(
            "line",
            (line) => this.handleLine(line)
        );

        this.process.on(
            "exit",
            (code) => {
                this.process = null;
                this.readline = null;

                if (this.pending) {
                    this.pending.reject(
                        new Error(
                            `Atlas worker exited with code ${code}`
                        )
                    );

                    this.pending = null;
                }
            }
        );
    }

    handleLine(line) {
        if (!this.pending)
            return;

        if (line === "END") {
            const request = this.pending;
            this.pending = null;

            request.resolve(request.result);
            return;
        }

        if (line.startsWith("RESULT\t")) {
            const parts = line.split("\t");

            this.pending.result.results.push({
                documentId: Number(parts[1]),
                score: Number(parts[2])
            });

            return;
        }

        if (line.startsWith("STATS\t")) {
            const parts = line.split("\t");

            this.pending.result.stats = {
                documentsScored: Number(parts[1]),
                postingsVisited: Number(parts[2]),
                blocksSkipped: Number(parts[3]),
                queryTerms: Number(parts[4]),
                matchedTerms: Number(parts[5]),
                candidatesConsidered:
                    Number(parts[6])
            };

            return;
        }

        if (line.startsWith("ERROR\t")) {
            const request = this.pending;
            this.pending = null;

            request.reject(
                new Error(
                    line.substring(6)
                )
            );
        }
    }

    search(
        query,
        {
            limit = 10,
            ranking = "BM25",
            retrieval = "TOP_K",
            mode = "LEXICAL"
        } = {}
    ) {
        this.start();

        if (this.pending) {
            return Promise.reject(
                new Error(
                    "Another Atlas search is currently running"
                )
            );
        }

        return new Promise(
            (resolve, reject) => {
                this.pending = {
                    resolve,
                    reject,
                    result: {
                        results: [],
                        stats: null
                    }
                };

                this.process.stdin.write(
                    `SEARCH\t${limit}\t${ranking}\t${retrieval}\t${mode}\n`
                );

                this.process.stdin.write(
                    `QUERY\t${encodeURIComponent(query)}\n`
                );
            }
        );
    }

    close() {
        if (this.process) {
            this.process.kill();
        }

        this.process = null;
        this.readline = null;
        this.pending = null;
    }
}

module.exports = AtlasSearch;