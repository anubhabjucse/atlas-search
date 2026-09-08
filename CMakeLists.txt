cmake_minimum_required(VERSION 3.20)

project(atlas VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ICU REQUIRED COMPONENTS uc i18n)

add_library(search_engine
    search_engine/src/tokenizer.cpp
    search_engine/src/index.cpp
    search_engine/src/boolean_retriever.cpp
    search_engine/src/query.cpp
    search_engine/src/search_engine.cpp
    search_engine/src/tfidf_ranker.cpp
    search_engine/src/bm25_ranker.cpp
    search_engine/src/top_k.cpp
    search_engine/src/posting_cursor.cpp
    search_engine/src/wand_retriever.cpp
    search_engine/src/block_max_wand_retriever.cpp
    search_engine/src/index_builder.cpp
    search_engine/src/prepared_query.cpp
    search_engine/src/index_persistence.cpp
    search_engine/src/phrase_retriever.cpp
)

target_include_directories(search_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/search_engine/include
)

target_link_libraries(search_engine
    PUBLIC
        ICU::uc
        ICU::i18n
)

target_compile_options(search_engine PRIVATE
    -Wall -Wextra -Wpedantic
)

add_library(database_adapter
    api/database_adapter/src/database_adapter.cpp
)

target_include_directories(database_adapter
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/api/database_adapter/include
)

target_link_libraries(database_adapter
    PUBLIC
        search_engine
)

target_compile_options(database_adapter PRIVATE
    -Wall -Wextra -Wpedantic
)

enable_testing()

function(add_search_engine_test test_name)
    add_executable(
        ${test_name}
        search_engine/tests/${test_name}.cpp
    )

    target_link_libraries(
        ${test_name}
        PRIVATE
            search_engine
    )

    add_test(
        NAME ${test_name}
        COMMAND ${test_name}
    )
endfunction()

add_search_engine_test(tokenizer_test)
add_search_engine_test(index_test)
add_search_engine_test(boolean_retriever_test)
add_search_engine_test(query_test)
add_search_engine_test(search_engine_test)
add_search_engine_test(end_to_end_test)
add_search_engine_test(ranking_test)
add_search_engine_test(top_k_test)
add_search_engine_test(posting_cursor_test)
add_search_engine_test(wand_retriever_test)
add_search_engine_test(block_max_wand_retriever_test)
add_search_engine_test(index_builder_test)
add_search_engine_test(prepared_query_test)
add_search_engine_test(index_persistence_test)
add_search_engine_test(search_engine_persistence_test)
add_executable(
    atlas_search_worker
    search_engine/tools/atlas_search_worker.cpp
)

target_link_libraries(
    atlas_search_worker
    PRIVATE
        search_engine
)

target_compile_options(atlas_search_worker PRIVATE
    -Wall -Wextra -Wpedantic
)
add_executable(
    atlas_index_builder
    search_engine/tools/build_news_index.cpp
)

target_link_libraries(
    atlas_index_builder
    PRIVATE
        search_engine
)

target_compile_options(atlas_index_builder PRIVATE
    -Wall -Wextra -Wpedantic
)
add_executable(
    database_adapter_test
    api/database_adapter/tests/database_adapter_test.cpp
)

target_link_libraries(
    database_adapter_test
    PRIVATE
        database_adapter
)

add_test(
    NAME database_adapter_test
    COMMAND database_adapter_test
)

# ---------------------------------------------------------------------------
# V2.4 Retrieval Benchmark
# ---------------------------------------------------------------------------

add_executable(
    retrieval_benchmark
    search_engine/tests/retrieval_benchmark.cpp
)

target_link_libraries(
    retrieval_benchmark
    PRIVATE
        search_engine
)

target_compile_options(retrieval_benchmark PRIVATE
    -Wall -Wextra -Wpedantic
)

if (WIN32)
    target_link_libraries(
        retrieval_benchmark
        PRIVATE
            Psapi
    )
endif()

# The benchmark is intentionally NOT registered with CTest.
#
# It is a performance workload, not a correctness unit test.
#
# This custom target runs the benchmark with multiple corpus sizes,
# block sizes, and K values. The benchmark itself writes the final
# results to benchmark.txt at the Atlas project root.
#
# The news JSON is expected at:
#
#   database_adapter/data/news/News_Category_Dataset_v3.json
#
# Relative to the Atlas source tree.

add_custom_target(
    run_retrieval_benchmark
    COMMAND
        $<TARGET_FILE:retrieval_benchmark>
        --dataset-path
        "${CMAKE_CURRENT_SOURCE_DIR}/api/database_adapter/data/news/News_Category_Dataset_v3.json"
        --documents
        100000,210000
        --blocks
        64,128,256
        --k
        1,10,50
        --iterations
        20
        --warmup
        5
        --output
        "${CMAKE_CURRENT_SOURCE_DIR}/benchmark.txt"

    DEPENDS
        retrieval_benchmark

    WORKING_DIRECTORY
        "${CMAKE_CURRENT_SOURCE_DIR}"

    USES_TERMINAL

    COMMENT
        "Running Atlas V2.4 retrieval benchmark"
)