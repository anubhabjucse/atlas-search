#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace atlas {

struct Token {
    std::string term;
    std::uint32_t position;
};

class Tokenizer {
public:
    Tokenizer();
    ~Tokenizer();

    Tokenizer(const Tokenizer&) = delete;
    Tokenizer& operator=(const Tokenizer&) = delete;

    std::vector<Token> tokenize(std::string_view utf8_text) const;
};

} // namespace atlas
