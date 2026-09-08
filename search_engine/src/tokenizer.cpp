#include "atlas/tokenizer.hpp"

#include <unicode/brkiter.h>
#include <unicode/errorcode.h>
#include <unicode/locid.h>
#include <unicode/normalizer2.h>
#include <unicode/unistr.h>
#include <unicode/ubrk.h>

#include <memory>
#include <stdexcept>

namespace atlas {

namespace {

icu::UnicodeString to_unicode(std::string_view utf8) {
    return icu::UnicodeString::fromUTF8(
        icu::StringPiece(utf8.data(), static_cast<int32_t>(utf8.size())));
}

std::string to_utf8(const icu::UnicodeString& text) {
    std::string out;
    text.toUTF8String(out);
    return out;
}

} // namespace

Tokenizer::Tokenizer() = default;
Tokenizer::~Tokenizer() = default;

std::vector<Token> Tokenizer::tokenize(std::string_view utf8_text) const {
    UErrorCode status = U_ZERO_ERROR;

    const icu::Normalizer2* normalizer =
        icu::Normalizer2::getNFKCCasefoldInstance(status);

    if (U_FAILURE(status) || normalizer == nullptr) {
        throw std::runtime_error("ICU NFKC_Casefold initialization failed");
    }

    const icu::UnicodeString input = to_unicode(utf8_text);
    icu::UnicodeString normalized;
    normalizer->normalize(input, normalized, status);

    if (U_FAILURE(status)) {
        throw std::runtime_error("ICU Unicode normalization failed");
    }

    std::unique_ptr<icu::BreakIterator> iterator(
        icu::BreakIterator::createWordInstance(icu::Locale::getRoot(), status));

    if (U_FAILURE(status) || !iterator) {
        throw std::runtime_error("ICU word BreakIterator initialization failed");
    }

    iterator->setText(normalized);

    std::vector<Token> tokens;
    std::uint32_t position = 0;

    int32_t start = iterator->first();

    while (start != icu::BreakIterator::DONE) {
        const int32_t end = iterator->next();
        if (end == icu::BreakIterator::DONE) {
            break;
        }

        const int32_t rule_status = iterator->getRuleStatus();

        // UBRK_WORD_NONE denotes boundaries that do not start a word/number.
        if (rule_status != UBRK_WORD_NONE && end > start) {
            const icu::UnicodeString term = normalized.tempSubStringBetween(start, end);

            if (!term.isEmpty()) {
                tokens.push_back(Token{
                    .term = to_utf8(term),
                    .position = position++
                });
            }
        }

        start = end;
    }

    return tokens;
}

} // namespace atlas
