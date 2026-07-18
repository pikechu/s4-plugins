#include "marker/BoundedMenuText.h"

#include <array>
#include <stdexcept>
#include <string_view>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int RunBoundedMenuTextTests() {
    using namespace campaign_completion;

    BoundedMenuText copied{};
    copied.bytes[0] = 'x';
    copied.length = 1u;
    Require(!CopyBoundedGuiText(nullptr, copied) &&
                copied.length == 1u && copied.bytes[0] == 'x',
            "null GUI text is rejected without mutating output");

    Require(CopyBoundedGuiText("Aeneas", copied) && copied.length == 6u &&
                std::string_view(copied.bytes.data(), copied.length) ==
                    "Aeneas" && copied.bytes[copied.length] == '\0',
            "bounded ASCII GUI text is copied with a terminator");

    std::array<char, 256u> maximum{};
    maximum.fill('a');
    maximum.back() = '\0';
    Require(CopyBoundedGuiText(maximum.data(), copied) &&
                copied.length == 255u && copied.bytes[255u] == '\0',
            "255 bytes plus NUL fit the fixed transport");

    std::array<char, 256u> unterminated{};
    unterminated.fill('b');
    const auto prior = copied;
    Require(!CopyBoundedGuiText(unterminated.data(), copied) &&
                copied.length == prior.length && copied.bytes == prior.bytes,
            "256 non-NUL bytes fail without mutating output");

    BoundedMenuText padded{};
    Require(CopyBoundedGuiText("  Aeneas  ", padded),
            "padded menu text is copied");
    BoundedWideText decoded{};
    Require(DecodeMenuTextLossless(padded, decoded) &&
                decoded.view() == L"Aeneas" &&
                decoded.units[decoded.length] == L'\0',
            "lossless ACP decode trims only presentation whitespace");

    BoundedMenuText plain{};
    Require(CopyBoundedGuiText("Antares", plain) &&
                DecodeMenuTextLossless(plain, decoded) &&
                decoded.view() == L"Antares",
            "ASCII survives the ACP round trip exactly");

    const char controlled[] = {'A', '\x01', 'B', '\0'};
    BoundedMenuText invalid{};
    Require(CopyBoundedGuiText(controlled, invalid),
            "transport preserves bytes before validation");
    const auto decodedPrior = decoded;
    Require(!DecodeMenuTextLossless(invalid, decoded) &&
                decoded.length == decodedPrior.length &&
                decoded.units == decodedPrior.units,
            "embedded controls fail without mutating decoded output");

    BoundedMenuText spaces{};
    Require(CopyBoundedGuiText("   ", spaces) &&
                !DecodeMenuTextLossless(spaces, decoded) &&
                decoded.length == decodedPrior.length,
            "an empty trimmed label is rejected");

    return 0;
}
