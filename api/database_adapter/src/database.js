class Database {
    async connect() {
        throw new Error(
            "Database.connect() is not implemented"
        );
    }

    async close() {
        throw new Error(
            "Database.close() is not implemented"
        );
    }

    async getDocumentById(documentId) {
        throw new Error(
            "Database.getDocumentById() is not implemented"
        );
    }

    async getDocumentsByIds(documentIds) {
        throw new Error(
            "Database.getDocumentsByIds() is not implemented"
        );
    }
}

module.exports = Database;