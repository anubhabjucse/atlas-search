#include "atlas/index.hpp"
#include "atlas/tokenizer.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace
{

    void test_round_trip()
    {
        const std::filesystem::path path =
            "atlas_v4_round_trip.idx";

        atlas::Tokenizer tokenizer;

        atlas::InMemoryIndex original(2);

        original.add(
            1,
            tokenizer.tokenize(
                "cat cat dog"));

        original.add(
            2,
            tokenizer.tokenize(
                "cat bird"));

        original.add(
            3,
            tokenizer.tokenize(
                "dog bird bird"));

        original.save(path);

        /*
         * Deliberately construct the destination index with a
         * different block size. The persisted metadata must win.
         */
        atlas::InMemoryIndex loaded(64);

        loaded.load(path);

        assert(
            loaded.posting_block_size() == 2);

        assert(
            loaded.vocabulary_size() ==
            original.vocabulary_size());

        assert(
            loaded.document_count() ==
            original.document_count());

        assert(
            loaded.max_document_id() ==
            original.max_document_id());

        assert(
            loaded.total_document_length() ==
            original.total_document_length());

        assert(
            loaded.average_document_length() ==
            original.average_document_length());

        assert(
            loaded.document_length(1) == 3);

        assert(
            loaded.document_length(2) == 2);

        assert(
            loaded.document_length(3) == 3);

        /*
         * cat
         */
        const auto &cat =
            loaded.postings("cat");

        assert(cat.size() == 2);

        assert(cat[0].document_id == 1);
        assert(cat[0].term_frequency == 2);

        assert(cat[1].document_id == 2);
        assert(cat[1].term_frequency == 1);

        assert(
            loaded.document_frequency("cat") == 2);

        assert(
            loaded.max_term_frequency("cat") == 2);

        /*
         * dog
         */
        const auto &dog =
            loaded.postings("dog");

        assert(dog.size() == 2);

        assert(dog[0].document_id == 1);
        assert(dog[0].term_frequency == 1);

        assert(dog[1].document_id == 3);
        assert(dog[1].term_frequency == 1);

        /*
         * bird
         */
        const auto &bird =
            loaded.postings("bird");

        assert(bird.size() == 2);

        assert(bird[0].document_id == 2);
        assert(bird[0].term_frequency == 1);

        assert(bird[1].document_id == 3);
        assert(bird[1].term_frequency == 2);

        assert(
            loaded.max_term_frequency("bird") == 2);

        /*
         * Missing term.
         */
        assert(
            loaded.postings("missing").empty());

        assert(
            loaded.posting_blocks("missing").empty());

        assert(
            loaded.term_frequency("missing", 1) == 0);

        assert(
            loaded.document_frequency("missing") == 0);

        std::filesystem::remove(path);
    }

    void test_empty_index_round_trip()
    {
        const std::filesystem::path path =
            "atlas_v4_empty.idx";

        atlas::InMemoryIndex original(128);

        original.save(path);

        atlas::InMemoryIndex loaded(64);

        loaded.load(path);

        assert(
            loaded.posting_block_size() == 128);

        assert(
            loaded.vocabulary_size() == 0);

        assert(
            loaded.document_count() == 0);

        assert(
            loaded.max_document_id() == 0);

        assert(
            loaded.total_document_length() == 0);

        assert(
            loaded.average_document_length() == 0.0);

        assert(
            loaded.postings("anything").empty());

        assert(
            loaded.posting_blocks("anything").empty());

        assert(
            loaded.document_length(12345) == 0);

        std::filesystem::remove(path);
    }

    void test_multiple_posting_blocks()
    {
        const std::filesystem::path path =
            "atlas_v4_multiple_blocks.idx";

        atlas::Tokenizer tokenizer;

        /*
         * Block size = 2.
         *
         * The term "atlas" appears in five documents, so we expect:
         *
         *   block 0 -> postings [0, 2)
         *   block 1 -> postings [2, 4)
         *   block 2 -> postings [4, 5)
         */
        atlas::InMemoryIndex original(2);

        original.add(
            10,
            tokenizer.tokenize(
                "atlas atlas"));

        original.add(
            20,
            tokenizer.tokenize(
                "atlas search"));

        original.add(
            30,
            tokenizer.tokenize(
                "atlas atlas atlas"));

        original.add(
            40,
            tokenizer.tokenize(
                "search atlas"));

        original.add(
            50,
            tokenizer.tokenize(
                "atlas engine"));

        original.save(path);

        atlas::InMemoryIndex loaded;

        loaded.load(path);

        assert(
            loaded.posting_block_size() == 2);

        const auto &postings =
            loaded.postings("atlas");

        assert(postings.size() == 5);

        assert(postings[0].document_id == 10);
        assert(postings[0].term_frequency == 2);

        assert(postings[1].document_id == 20);
        assert(postings[1].term_frequency == 1);

        assert(postings[2].document_id == 30);
        assert(postings[2].term_frequency == 3);

        assert(postings[3].document_id == 40);
        assert(postings[3].term_frequency == 1);

        assert(postings[4].document_id == 50);
        assert(postings[4].term_frequency == 1);

        assert(
            loaded.max_term_frequency("atlas") == 3);

        const auto &blocks =
            loaded.posting_blocks("atlas");

        assert(blocks.size() == 3);

        /*
         * Block 0.
         */
        assert(blocks[0].begin == 0);
        assert(blocks[0].end == 2);

        assert(
            blocks[0].first_document == 10);

        assert(
            blocks[0].last_document == 20);

        assert(
            blocks[0].max_term_frequency == 2);

        /*
         * Documents 10 and 20 have lengths 2 and 2.
         */
        assert(
            blocks[0].min_document_length == 2);

        /*
         * Block 1.
         */
        assert(blocks[1].begin == 2);
        assert(blocks[1].end == 4);

        assert(
            blocks[1].first_document == 30);

        assert(
            blocks[1].last_document == 40);

        assert(
            blocks[1].max_term_frequency == 3);

        assert(
            blocks[1].min_document_length == 2);

        /*
         * Block 2.
         */
        assert(blocks[2].begin == 4);
        assert(blocks[2].end == 5);

        assert(
            blocks[2].first_document == 50);

        assert(
            blocks[2].last_document == 50);

        assert(
            blocks[2].max_term_frequency == 1);

        assert(
            blocks[2].min_document_length == 2);

        /*
         * Verify all document statistics.
         */
        assert(
            loaded.document_count() == 5);

        assert(
            loaded.max_document_id() == 50);

        assert(
            loaded.total_document_length() == 11);

        assert(
            loaded.document_length(10) == 2);

        assert(
            loaded.document_length(20) == 2);

        assert(
            loaded.document_length(30) == 3);

        assert(
            loaded.document_length(40) == 2);

        assert(
            loaded.document_length(50) == 2);

        std::filesystem::remove(path);
    }

    void test_term_statistics_round_trip()
    {
        const std::filesystem::path path =
            "atlas_v4_term_statistics.idx";

        atlas::Tokenizer tokenizer;

        atlas::InMemoryIndex original(3);

        original.add(
            1,
            tokenizer.tokenize(
                "alpha alpha alpha beta"));

        original.add(
            2,
            tokenizer.tokenize(
                "alpha beta beta"));

        original.add(
            3,
            tokenizer.tokenize(
                "beta beta beta beta"));

        original.save(path);

        atlas::InMemoryIndex loaded;

        loaded.load(path);

        /*
         * alpha:
         *
         * doc 1 -> 3
         * doc 2 -> 1
         */
        assert(
            loaded.term_frequency("alpha", 1) == 3);

        assert(
            loaded.term_frequency("alpha", 2) == 1);

        assert(
            loaded.term_frequency("alpha", 3) == 0);

        assert(
            loaded.document_frequency("alpha") == 2);

        assert(
            loaded.max_term_frequency("alpha") == 3);

        /*
         * beta:
         *
         * doc 1 -> 1
         * doc 2 -> 2
         * doc 3 -> 4
         */
        assert(
            loaded.term_frequency("beta", 1) == 1);

        assert(
            loaded.term_frequency("beta", 2) == 2);

        assert(
            loaded.term_frequency("beta", 3) == 4);

        assert(
            loaded.document_frequency("beta") == 3);

        assert(
            loaded.max_term_frequency("beta") == 4);

        std::filesystem::remove(path);
    }

    void test_missing_file()
    {
        atlas::InMemoryIndex index;

        bool threw = false;

        try
        {
            index.load(
                "does_not_exist_atlas_v4.idx");
        }
        catch (const std::runtime_error &)
        {
            threw = true;
        }

        assert(threw);
    }

    void test_invalid_magic()
    {
        const std::filesystem::path path =
            "atlas_v4_invalid_magic.idx";

        {
            std::ofstream output(
                path,
                std::ios::binary |
                    std::ios::trunc);

            output << "NOTATLAS";
        }

        atlas::InMemoryIndex index;

        bool threw = false;

        try
        {
            index.load(path);
        }
        catch (const std::runtime_error &)
        {
            threw = true;
        }

        assert(threw);

        std::filesystem::remove(path);
    }

    void test_failed_load_does_not_destroy_existing_index()
    {
        const std::filesystem::path path =
            "atlas_v4_bad_load.idx";

        atlas::Tokenizer tokenizer;

        atlas::InMemoryIndex index;

        index.add(
            1,
            tokenizer.tokenize(
                "cat dog"));

        {
            std::ofstream output(
                path,
                std::ios::binary |
                    std::ios::trunc);

            output << "BAD";
        }

        bool threw = false;

        try
        {
            index.load(path);
        }
        catch (const std::runtime_error &)
        {
            threw = true;
        }

        assert(threw);

        /*
         * The original index must still be intact.
         */
        assert(
            index.document_count() == 1);

        assert(
            index.vocabulary_size() == 2);

        assert(
            index.term_frequency("cat", 1) == 1);

        assert(
            index.term_frequency("dog", 1) == 1);

        std::filesystem::remove(path);
    }
    void test_save_replaces_existing_index()
    {
        const std::filesystem::path path =
            "atlas_v4_atomic_replace.idx";

        atlas::Tokenizer tokenizer;

        /*
         * First version.
         */
        atlas::InMemoryIndex first;

        first.add(
            1,
            tokenizer.tokenize(
                "cat"));

        first.save(path);

        /*
         * Second version has completely different contents.
         */
        atlas::InMemoryIndex second;

        second.add(
            10,
            tokenizer.tokenize(
                "elephant elephant"));

        second.add(
            20,
            tokenizer.tokenize(
                "zebra"));

        second.save(path);

        /*
         * Loading the destination must give us the second index.
         */
        atlas::InMemoryIndex loaded;

        loaded.load(path);

        assert(
            loaded.document_count() == 2);

        assert(
            loaded.max_document_id() == 20);

        assert(
            loaded.vocabulary_size() == 2);

        assert(
            loaded.term_frequency(
                "elephant",
                10) == 2);

        assert(
            loaded.term_frequency(
                "zebra",
                20) == 1);

        /*
         * Old data must no longer be present.
         */
        assert(
            loaded.term_frequency(
                "cat",
                1) == 0);

        assert(
            loaded.postings("cat").empty());

        std::filesystem::remove(path);
    }
    void test_truncated_file_is_rejected()
    {
        const std::filesystem::path path =
            "atlas_v4_truncated.idx";

        atlas::Tokenizer tokenizer;

        atlas::InMemoryIndex original;

        original.add(
            1,
            tokenizer.tokenize(
                "cat dog bird"));

        original.add(
            2,
            tokenizer.tokenize(
                "cat cat"));

        original.save(path);

        /*
         * Deliberately truncate the persisted representation.
         */
        {
            std::ifstream input(
                path,
                std::ios::binary);

            assert(input);

            std::vector<char> bytes{
                std::istreambuf_iterator<char>(input),
                std::istreambuf_iterator<char>()};

            assert(bytes.size() > 16);

            bytes.resize(
                bytes.size() / 2);

            std::ofstream output(
                path,
                std::ios::binary |
                    std::ios::trunc);

            assert(output);

            output.write(
                bytes.data(),
                static_cast<std::streamsize>(
                    bytes.size()));

            assert(output);
        }

        atlas::InMemoryIndex loaded;

        bool threw = false;

        try
        {
            loaded.load(path);
        }
        catch (const std::runtime_error &)
        {
            threw = true;
        }

        assert(threw);

        std::filesystem::remove(path);
    }
    void test_trailing_data_is_rejected()
    {
        const std::filesystem::path path =
            "atlas_v4_trailing_data.idx";

        atlas::Tokenizer tokenizer;

        atlas::InMemoryIndex original;

        original.add(
            1,
            tokenizer.tokenize(
                "cat dog"));

        original.save(path);

        /*
         * Append arbitrary garbage after an otherwise valid index.
         */
        {
            std::ofstream output(
                path,
                std::ios::binary |
                    std::ios::app);

            assert(output);

            const char garbage[] =
                "ATLAS_TRAILING_GARBAGE";

            output.write(
                garbage,
                sizeof(garbage) - 1);

            assert(output);
        }

        atlas::InMemoryIndex loaded;

        bool threw = false;

        try
        {
            loaded.load(path);
        }
        catch (const std::runtime_error &)
        {
            threw = true;
        }

        assert(threw);

        std::filesystem::remove(path);
    }

} // namespace

int main()
{
    test_round_trip();

    test_empty_index_round_trip();

    test_multiple_posting_blocks();

    test_term_statistics_round_trip();

    test_missing_file();

    test_invalid_magic();

    test_failed_load_does_not_destroy_existing_index();
    test_save_replaces_existing_index();

    test_truncated_file_is_rejected();

    test_trailing_data_is_rejected();

    return 0;
}