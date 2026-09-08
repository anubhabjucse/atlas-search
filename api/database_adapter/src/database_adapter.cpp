#include "atlas/database_adapter.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace atlas {

TextFileDatabaseAdapter::TextFileDatabaseAdapter(
    std::filesystem::path data_directory
)
    : data_directory_(std::move(data_directory)) {
}

void TextFileDatabaseAdapter::index_into(
    SearchEngine& search_engine
) const {
    if (!std::filesystem::is_directory(data_directory_)) {
        throw std::invalid_argument(
            "Database adapter data directory does not exist"
        );
    }

    std::vector<std::filesystem::path> documents;
    for (const auto& entry :
         std::filesystem::directory_iterator(data_directory_)) {
        if (entry.is_regular_file()) {
            documents.push_back(entry.path());
        }
    }

    std::sort(documents.begin(), documents.end());

    DocumentId document_id = 1;
    for (const auto& document_path : documents) {
        std::ifstream document(document_path);
        if (!document) {
            throw std::runtime_error(
                "Unable to open database adapter document"
            );
        }

        std::ostringstream text;
        text << document.rdbuf();

        search_engine.index_document(document_id++, text.str());
    }
}

} // namespace atlas