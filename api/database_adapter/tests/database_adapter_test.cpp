#include "atlas/database_adapter.hpp"

#include <cassert>
#include <filesystem>
#include <vector>

int main() {
    atlas::SearchEngine search_engine;
    atlas::TextFileDatabaseAdapter adapter(
        std::filesystem::path("../api/database_adapter/data/tiny")
    );

    adapter.index_into(search_engine);

    assert(search_engine.max_document_id() == 4);
    assert(
        (search_engine.search("distributed") ==
            std::vector<atlas::DocumentId>{1, 4})
    );

    return 0;
}