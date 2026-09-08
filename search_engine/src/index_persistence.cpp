#include "atlas/index.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <filesystem>
#include <system_error>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#endif
namespace atlas
{

    namespace
    {

        constexpr std::array<char, 8> kMagic = {
            'A', 'T', 'L', 'S', 'I', 'D', 'X', '\0'};

        constexpr std::uint32_t kFormatVersion = 2;
        constexpr std::uint32_t kEndianMarker = 0x01020304;

        constexpr std::uint64_t kMaxSerializedStringSize =
            1024ULL * 1024ULL * 1024ULL;

        constexpr std::uint64_t kMaxSerializedVectorSize =
            1000000000ULL;

        void write_bytes(
            std::ostream &output,
            const void *data,
            std::size_t size)
        {
            output.write(
                static_cast<const char *>(data),
                static_cast<std::streamsize>(size));

            if (!output)
            {
                throw std::runtime_error(
                    "failed while writing Atlas index");
            }
        }

        void read_bytes(
            std::istream &input,
            void *data,
            std::size_t size)
        {
            input.read(
                static_cast<char *>(data),
                static_cast<std::streamsize>(size));

            if (!input)
            {
                throw std::runtime_error(
                    "truncated or unreadable Atlas index");
            }
        }

        void write_u32(
            std::ostream &output,
            std::uint32_t value)
        {
            write_bytes(output, &value, sizeof(value));
        }

        void write_u64(
            std::ostream &output,
            std::uint64_t value)
        {
            write_bytes(output, &value, sizeof(value));
        }

        std::uint32_t read_u32(
            std::istream &input)
        {
            std::uint32_t value = 0;
            read_bytes(input, &value, sizeof(value));
            return value;
        }

        std::uint64_t read_u64(
            std::istream &input)
        {
            std::uint64_t value = 0;
            read_bytes(input, &value, sizeof(value));
            return value;
        }

        void write_size(
            std::ostream &output,
            std::size_t value)
        {
            if (value >
                static_cast<std::size_t>(
                    std::numeric_limits<std::uint64_t>::max()))
            {
                throw std::runtime_error(
                    "size cannot be represented in Atlas index format");
            }

            write_u64(
                output,
                static_cast<std::uint64_t>(value));
        }

        std::size_t read_size(
            std::istream &input,
            const char *field_name)
        {
            const std::uint64_t value =
                read_u64(input);

            if (value >
                static_cast<std::uint64_t>(
                    std::numeric_limits<std::size_t>::max()))
            {
                throw std::runtime_error(
                    std::string(
                        "serialized ") +
                    field_name +
                    " exceeds platform size limit");
            }

            return static_cast<std::size_t>(value);
        }

        void write_string(
            std::ostream &output,
            const std::string &value)
        {
            write_size(output, value.size());

            if (!value.empty())
            {
                write_bytes(
                    output,
                    value.data(),
                    value.size());
            }
        }

        std::string read_string(
            std::istream &input)
        {
            const std::uint64_t size =
                read_u64(input);

            if (size > kMaxSerializedStringSize)
            {
                throw std::runtime_error(
                    "serialized term is unreasonably large");
            }

            std::string value(
                static_cast<std::size_t>(size),
                '\0');

            if (!value.empty())
            {
                read_bytes(
                    input,
                    value.data(),
                    value.size());
            }

            return value;
        }

        void validate_count(
            std::uint64_t count,
            const char *field_name)
        {
            if (count > kMaxSerializedVectorSize)
            {
                throw std::runtime_error(
                    std::string(
                        "serialized ") +
                    field_name +
                    " count is unreasonably large");
            }
        }

    } // namespace
    void atomic_replace(
        const std::filesystem::path &temporary_path,
        const std::filesystem::path &destination_path)
    {
#ifdef _WIN32

        const BOOL result =
            MoveFileExW(
                temporary_path.wstring().c_str(),
                destination_path.wstring().c_str(),
                MOVEFILE_REPLACE_EXISTING |
                    MOVEFILE_WRITE_THROUGH);

        if (!result)
        {
            const DWORD error =
                GetLastError();

            throw std::runtime_error(
                "failed to atomically replace Atlas index " +
                destination_path.string() +
                " (Windows error " +
                std::to_string(error) +
                ")");
        }

#else

        std::error_code error;

        std::filesystem::rename(
            temporary_path,
            destination_path,
            error);

        if (error)
        {
            throw std::runtime_error(
                "failed to atomically replace Atlas index " +
                destination_path.string() +
                ": " +
                error.message());
        }

#endif
    }
    void InMemoryIndex::save(
        const std::filesystem::path &path) const
    {
        namespace fs = std::filesystem;

        if (path.empty())
        {
            throw std::runtime_error(
                "Atlas index save path is empty");
        }

        const fs::path destination =
            fs::absolute(path);

        const fs::path directory =
            destination.parent_path();

        std::error_code directory_error;

        fs::create_directories(
            directory,
            directory_error);

        if (directory_error)
        {
            throw std::runtime_error(
                "unable to create Atlas index directory " +
                directory.string() +
                ": " +
                directory_error.message());
        }

        /*
         * Generate the temporary filename beside the destination.
         *
         * We deliberately do not use the system temporary directory:
         * atomic replacement requires the temporary file and destination
         * to live on the same filesystem.
         */
        fs::path temporary_path;

        for (std::uint32_t attempt = 0;
             attempt < 1000;
             ++attempt)
        {

            temporary_path =
                destination;

            temporary_path +=
                ".tmp." +
                std::to_string(
                    static_cast<unsigned long long>(
                        std::chrono::high_resolution_clock::now()
                            .time_since_epoch()
                            .count())) +
                "." +
                std::to_string(attempt);

            std::error_code exists_error;

            const bool exists =
                fs::exists(
                    temporary_path,
                    exists_error);

            if (exists_error)
            {
                continue;
            }

            if (!exists)
            {
                break;
            }

            temporary_path.clear();
        }

        if (temporary_path.empty())
        {
            throw std::runtime_error(
                "unable to create unique temporary Atlas index path");
        }

        try
        {
            std::ofstream output(
                temporary_path,
                std::ios::binary |
                    std::ios::trunc);

            if (!output)
            {
                throw std::runtime_error(
                    "unable to open temporary Atlas index for writing: " +
                    temporary_path.string());
            }

            write_bytes(
                output,
                kMagic.data(),
                kMagic.size());

            write_u32(
                output,
                kFormatVersion);

            write_u32(
                output,
                kEndianMarker);

            write_u64(
                output,
                static_cast<std::uint64_t>(
                    posting_block_size_));

            write_u64(
                output,
                static_cast<std::uint64_t>(
                    document_count_));

            write_u64(
                output,
                static_cast<std::uint64_t>(
                    total_document_length_));

            write_u32(
                output,
                max_document_id_);

            write_u64(
                output,
                static_cast<std::uint64_t>(
                    dictionary_.size()));

            /*
             * Document statistics.
             */
            write_u64(
                output,
                static_cast<std::uint64_t>(
                    document_lengths_.size()));

            for (const auto &[document_id, length] :
                 document_lengths_)
            {

                write_u32(
                    output,
                    document_id);

                write_u64(
                    output,
                    static_cast<std::uint64_t>(
                        length));
            }

            /*
             * Dictionary and physical posting data.
             */
            for (const auto &[term, data] :
                 dictionary_)
            {

                write_string(
                    output,
                    term);

                write_u32(
                    output,
                    data.max_term_frequency);

                write_u64(
                    output,
                    static_cast<std::uint64_t>(
                        data.postings.size()));

                for (const Posting &posting :
                     data.postings)
                {

                    write_u32(
                        output,
                        posting.document_id);

                    write_u32(
                        output,
                        posting.term_frequency);
                    write_u64(
                        output,
                        static_cast<std::uint64_t>(
                            posting.positions.size()));

                    for (const std::uint32_t position :
                         posting.positions)
                    {

                        write_u32(
                            output,
                            position);
                    }
                }

                write_u64(
                    output,
                    static_cast<std::uint64_t>(
                        data.blocks.size()));

                for (const PostingBlock &block :
                     data.blocks)
                {

                    write_u64(
                        output,
                        static_cast<std::uint64_t>(
                            block.begin));

                    write_u64(
                        output,
                        static_cast<std::uint64_t>(
                            block.end));

                    write_u32(
                        output,
                        block.first_document);

                    write_u32(
                        output,
                        block.last_document);

                    write_u32(
                        output,
                        block.max_term_frequency);

                    write_u32(
                        output,
                        block.min_document_length);
                }
            }

            /*
             * Force all buffered stream data out before replacing the
             * destination.
             */
            output.flush();

            if (!output)
            {
                throw std::runtime_error(
                    "failed while flushing temporary Atlas index");
            }

            output.close();

            if (!output)
            {
                throw std::runtime_error(
                    "failed while closing temporary Atlas index");
            }

            /*
             * Only now does the existing destination get replaced.
             */
            atomic_replace(
                temporary_path,
                destination);

            /*
             * atomic_replace() consumed the temporary file.
             */
            temporary_path.clear();
        }
        catch (...)
        {
            /*
             * If anything failed before replacement, remove the partial
             * temporary file. Never remove the existing destination here.
             */
            if (!temporary_path.empty())
            {
                std::error_code cleanup_error;

                fs::remove(
                    temporary_path,
                    cleanup_error);
            }

            throw;
        }
    }
    void InMemoryIndex::load(
        const std::filesystem::path &path)
    {
        std::ifstream input(
            path,
            std::ios::binary);

        if (!input)
        {
            throw std::runtime_error(
                "unable to open Atlas index for reading: " +
                path.string());
        }

        std::array<char, kMagic.size()> magic{};

        read_bytes(
            input,
            magic.data(),
            magic.size());

        if (magic != kMagic)
        {
            throw std::runtime_error(
                "invalid Atlas index magic");
        }

        const std::uint32_t version =
            read_u32(input);

        if (version != kFormatVersion)
        {
            throw std::runtime_error(
                "unsupported Atlas index format version");
        }

        const std::uint32_t endian =
            read_u32(input);

        if (endian != kEndianMarker)
        {
            throw std::runtime_error(
                "unsupported Atlas index byte order");
        }

        const std::uint64_t serialized_block_size =
            read_u64(input);

        if (serialized_block_size == 0 ||
            serialized_block_size >
                static_cast<std::uint64_t>(
                    std::numeric_limits<std::size_t>::max()))
        {
            throw std::runtime_error(
                "invalid Atlas posting block size");
        }

        const std::size_t loaded_block_size =
            static_cast<std::size_t>(
                serialized_block_size);

        const std::uint64_t serialized_document_count =
            read_u64(input);

        if (serialized_document_count >
            static_cast<std::uint64_t>(
                std::numeric_limits<std::size_t>::max()))
        {
            throw std::runtime_error(
                "invalid Atlas document count");
        }

        const std::size_t loaded_document_count =
            static_cast<std::size_t>(
                serialized_document_count);

        const std::uint64_t serialized_total_length =
            read_u64(input);

        if (serialized_total_length >
            static_cast<std::uint64_t>(
                std::numeric_limits<std::size_t>::max()))
        {
            throw std::runtime_error(
                "invalid Atlas total document length");
        }

        const std::size_t loaded_total_length =
            static_cast<std::size_t>(
                serialized_total_length);

        const DocumentId loaded_max_document_id =
            read_u32(input);

        const std::uint64_t vocabulary_count =
            read_u64(input);

        validate_count(
            vocabulary_count,
            "vocabulary");

        const std::uint64_t document_stats_count =
            read_u64(input);

        validate_count(
            document_stats_count,
            "document statistics");

        /*
         * Deserialize into temporary containers.
         *
         * Nothing in the current index is modified until the complete
         * representation has been read and validated.
         */
        Dictionary loaded_dictionary;

        std::unordered_map<
            DocumentId,
            std::size_t>
            loaded_document_lengths;

        loaded_dictionary.reserve(
            static_cast<std::size_t>(
                vocabulary_count));

        loaded_document_lengths.reserve(
            static_cast<std::size_t>(
                document_stats_count));

        for (std::uint64_t i = 0;
             i < document_stats_count;
             ++i)
        {

            const DocumentId document_id =
                read_u32(input);

            const std::uint64_t length =
                read_u64(input);

            if (length >
                static_cast<std::uint64_t>(
                    std::numeric_limits<std::size_t>::max()))
            {
                throw std::runtime_error(
                    "invalid serialized document length");
            }

            const auto [it, inserted] =
                loaded_document_lengths.emplace(
                    document_id,
                    static_cast<std::size_t>(length));

            if (!inserted)
            {
                throw std::runtime_error(
                    "duplicate serialized document ID");
            }
        }

        for (std::uint64_t vocabulary_index = 0;
             vocabulary_index < vocabulary_count;
             ++vocabulary_index)
        {

            std::string term =
                read_string(input);

            if (term.empty())
            {
                throw std::runtime_error(
                    "serialized Atlas dictionary contains empty term");
            }

            if (loaded_dictionary.contains(term))
            {
                throw std::runtime_error(
                    "duplicate serialized dictionary term");
            }

            TermData data;

            data.max_term_frequency =
                read_u32(input);

            const std::uint64_t posting_count =
                read_u64(input);

            validate_count(
                posting_count,
                "posting");

            data.postings.reserve(
                static_cast<std::size_t>(
                    posting_count));

            DocumentId previous_document = 0;
            bool has_previous_document = false;

            for (std::uint64_t posting_index = 0;
                 posting_index < posting_count;
                 ++posting_index)
            {

                Posting posting;

                posting.document_id =
                    read_u32(input);

                posting.term_frequency =
                    read_u32(input);
                const std::uint64_t position_count =
                    read_u64(input);

                validate_count(
                    position_count,
                    "posting position");

                posting.positions.reserve(
                    static_cast<std::size_t>(
                        position_count));

                std::uint32_t previous_position = 0;
                bool has_previous_position = false;

                for (std::uint64_t position_index = 0;
                     position_index < position_count;
                     ++position_index)
                {

                    const std::uint32_t position =
                        read_u32(input);

                    if (has_previous_position &&
                        position <= previous_position)
                    {

                        throw std::runtime_error(
                            "serialized posting positions are not strictly ordered");
                    }

                    posting.positions.push_back(
                        position);

                    previous_position =
                        position;

                    has_previous_position = true;
                }

                if (posting.positions.size() !=
                    posting.term_frequency)
                {

                    throw std::runtime_error(
                        "serialized posting positions do not match term frequency");
                }
                if (posting.term_frequency == 0)
                {
                    throw std::runtime_error(
                        "serialized posting has zero term frequency");
                }

                if (has_previous_document &&
                    posting.document_id <= previous_document)
                {
                    throw std::runtime_error(
                        "serialized postings are not strictly ordered");
                }

                if (!loaded_document_lengths.contains(
                        posting.document_id))
                {
                    throw std::runtime_error(
                        "posting references unknown document");
                }

                previous_document =
                    posting.document_id;

                has_previous_document = true;

                data.postings.push_back(posting);

                data.max_term_frequency =
                    std::max(
                        data.max_term_frequency,
                        posting.term_frequency);
            }

            if (posting_count == 0 &&
                data.max_term_frequency != 0)
            {
                throw std::runtime_error(
                    "invalid term maximum for empty posting list");
            }

            if (posting_count > 0 &&
                data.max_term_frequency == 0)
            {
                throw std::runtime_error(
                    "invalid zero term maximum");
            }

            const std::uint64_t block_count =
                read_u64(input);

            validate_count(
                block_count,
                "posting block");

            data.blocks.reserve(
                static_cast<std::size_t>(
                    block_count));

            std::size_t expected_begin = 0;

            for (std::uint64_t block_index = 0;
                 block_index < block_count;
                 ++block_index)
            {

                PostingBlock block;

                block.begin =
                    read_size(
                        input,
                        "posting block begin");

                block.end =
                    read_size(
                        input,
                        "posting block end");

                block.first_document =
                    read_u32(input);

                block.last_document =
                    read_u32(input);

                block.max_term_frequency =
                    read_u32(input);

                block.min_document_length =
                    read_u32(input);

                if (block.begin != expected_begin)
                {
                    throw std::runtime_error(
                        "posting blocks contain a gap or overlap");
                }

                if (block.begin >= block.end ||
                    block.end > data.postings.size())
                {
                    throw std::runtime_error(
                        "invalid posting block range");
                }

                if (block.first_document >
                    block.last_document)
                {
                    throw std::runtime_error(
                        "invalid posting block document range");
                }

                const Posting &first =
                    data.postings[block.begin];

                const Posting &last =
                    data.postings[block.end - 1];

                if (first.document_id !=
                        block.first_document ||
                    last.document_id !=
                        block.last_document)
                {
                    throw std::runtime_error(
                        "posting block document bounds do not match postings");
                }

                std::uint32_t calculated_max_tf = 0;
                std::uint32_t calculated_min_length =
                    std::numeric_limits<std::uint32_t>::max();

                for (std::size_t posting_index =
                         block.begin;
                     posting_index < block.end;
                     ++posting_index)
                {

                    const Posting &posting =
                        data.postings[posting_index];

                    calculated_max_tf =
                        std::max(
                            calculated_max_tf,
                            posting.term_frequency);

                    const std::size_t length =
                        loaded_document_lengths.at(
                            posting.document_id);

                    if (length >
                        std::numeric_limits<
                            std::uint32_t>::max())
                    {
                        throw std::runtime_error(
                            "document length exceeds posting-block format limit");
                    }

                    calculated_min_length =
                        std::min(
                            calculated_min_length,
                            static_cast<std::uint32_t>(
                                length));
                }

                if (block.max_term_frequency !=
                        calculated_max_tf ||
                    block.min_document_length !=
                        calculated_min_length)
                {
                    throw std::runtime_error(
                        "posting block statistics are inconsistent");
                }

                expected_begin = block.end;

                data.blocks.push_back(block);
            }

            if (expected_begin != data.postings.size())
            {
                throw std::runtime_error(
                    "posting blocks do not cover the posting list");
            }

            loaded_dictionary.emplace(
                std::move(term),
                std::move(data));
        }

        /*
         * Validate global document metadata.
         */
        if (loaded_document_count !=
            loaded_document_lengths.size())
        {
            throw std::runtime_error(
                "document count does not match document statistics");
        }

        std::size_t calculated_total_length = 0;
        bool calculated_has_documents = false;
        DocumentId calculated_max_document_id = 0;

        for (const auto &[document_id, length] :
             loaded_document_lengths)
        {

            if (calculated_total_length >
                std::numeric_limits<std::size_t>::max() -
                    length)
            {
                throw std::runtime_error(
                    "document length total overflows");
            }

            calculated_total_length += length;

            if (!calculated_has_documents ||
                document_id > calculated_max_document_id)
            {

                calculated_max_document_id =
                    document_id;
            }

            calculated_has_documents = true;
        }

        if (calculated_total_length !=
            loaded_total_length)
        {
            throw std::runtime_error(
                "serialized total document length is inconsistent");
        }

        if (calculated_has_documents !=
            (loaded_document_count != 0))
        {
            throw std::runtime_error(
                "serialized document presence metadata is inconsistent");
        }

        if (calculated_has_documents &&
            calculated_max_document_id !=
                loaded_max_document_id)
        {
            throw std::runtime_error(
                "serialized maximum document ID is inconsistent");
        }

        if (!calculated_has_documents &&
            loaded_max_document_id != 0)
        {
            throw std::runtime_error(
                "empty index has non-zero maximum document ID");
        }
        char trailing_byte = 0;

        input.read(
            &trailing_byte,
            1);

        if (input.gcount() != 0)
        {
            throw std::runtime_error(
                "Atlas index contains unexpected trailing data");
        }

        if (!input.eof())
        {
            throw std::runtime_error(
                "failed while validating Atlas index end-of-file");
        }
        /*
         * Commit only after every validation has succeeded.
         *
         * The address of this InMemoryIndex remains unchanged, so all
         * SearchEngine components that reference it remain valid.
         */
        dictionary_ =
            std::move(loaded_dictionary);

        document_lengths_ =
            std::move(loaded_document_lengths);

        document_count_ =
            loaded_document_count;

        total_document_length_ =
            loaded_total_length;

        max_document_id_ =
            loaded_max_document_id;

        has_documents_ =
            calculated_has_documents;

        posting_block_size_ =
            loaded_block_size;
    }

} // namespace atlas