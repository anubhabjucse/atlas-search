const {
    MongoClient
} = require("mongodb");

const Database =
    require("./database");

class MongoDBDatabase extends Database {
    constructor(
        uri,
        databaseName = "atlas",
        collectionName = "news"
    ) {
        super();

        if (!uri) {
            throw new Error(
                "MongoDB URI is required"
            );
        }

        this.uri = uri;
        this.databaseName = databaseName;
        this.collectionName = collectionName;

        this.client = null;
        this.collection = null;
    }

    async connect() {
        this.client =
            new MongoClient(this.uri);

        await this.client.connect();

        await this.client
            .db("admin")
            .command({ ping: 1 });

        this.collection =
            this.client
                .db(this.databaseName)
                .collection(this.collectionName);
    }

    async close() {
        if (this.client) {
            await this.client.close();
        }

        this.client = null;
        this.collection = null;
    }

    ensureConnected() {
        if (!this.collection) {
            throw new Error(
                "MongoDB adapter is not connected"
            );
        }
    }

    async getDocumentById(documentId) {
        this.ensureConnected();

        return this.collection.findOne({
            _id: Number(documentId)
        });
    }

    async getDocumentsByIds(documentIds) {
        this.ensureConnected();

        if (documentIds.length === 0)
            return [];

        const documents =
            await this.collection
                .find({
                    _id: {
                        $in: documentIds.map(Number)
                    }
                })
                .toArray();

        const map =
            new Map(
                documents.map(
                    (document) => [
                        Number(document._id),
                        document
                    ]
                )
            );

        return documentIds
            .map(
                (id) =>
                    map.get(Number(id))
            )
            .filter(Boolean);
    }
}

module.exports = MongoDBDatabase;