#pragma once

#include "atlas/search_engine.hpp"

#include <filesystem>

namespace atlas {

class DatabaseAdapter {
public:
    virtual ~DatabaseAdapter() = default;

    virtual void index_into(SearchEngine& search_engine) const = 0;
};

class TextFileDatabaseAdapter final : public DatabaseAdapter {
public:
    explicit TextFileDatabaseAdapter(
        std::filesystem::path data_directory
    );

    void index_into(SearchEngine& search_engine) const override;

private:
    std::filesystem::path data_directory_;
};

} // namespace atlas