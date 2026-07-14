#include "marker/FixedMapRowCalibration.h"

#include "completion/CompletionRecord.h"

#include <algorithm>
#include <array>
#include <limits>
#include <sstream>
#include <utility>
#include <vector>

namespace campaign_completion {
namespace {

constexpr std::size_t kMaximumGuiTextBytes = 256u;
constexpr std::size_t kMaximumGuiTextUnits = 256u;

bool HasExactFixedMapPages(const PageSnapshot& snapshot) {
    static const std::vector<DWORD> expected{4u, 22u, 23u, 25u};
    return snapshot.activePages == expected;
}

bool HasControlCharacter(std::wstring_view value) noexcept {
    return std::any_of(value.begin(), value.end(), [](wchar_t unit) {
        const auto code = static_cast<unsigned>(unit);
        return code <= 0x1Fu || (code >= 0x7Fu && code <= 0x9Fu);
    });
}

bool CopyGuiTextBytes(const char* text,
                      std::array<char, kMaximumGuiTextBytes + 1u>& buffer,
                      std::size_t& length) noexcept {
    if (text == nullptr) {
        return false;
    }
#if defined(_MSC_VER)
    __try {
#endif
        for (std::size_t index = 0u; index < kMaximumGuiTextBytes; ++index) {
            const char value = text[index];
            buffer[index] = value;
            if (value == '\0') {
                length = index;
                return true;
            }
        }
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
    return false;
}

bool ValidGeometry(const MarkerGuiElement& element) noexcept {
    if (element.surfaceWidth == 0u || element.surfaceHeight == 0u ||
        element.width == 0u || element.height == 0u) {
        return false;
    }
    const auto right = static_cast<std::uint64_t>(element.x) + element.width;
    const auto bottom = static_cast<std::uint64_t>(element.y) + element.height;
    return right <= element.surfaceWidth && bottom <= element.surfaceHeight;
}

}  // namespace

bool CopyBoundedGuiText(const char* text, std::string& output) noexcept {
    try {
        std::array<char, kMaximumGuiTextBytes + 1u> buffer{};
        std::size_t length = 0u;
        if (!CopyGuiTextBytes(text, buffer, length)) {
            return false;
        }
        output.assign(buffer.data(), length);
        return true;
    } catch (...) {
        return false;
    }
}

std::optional<std::wstring> DecodeMenuTextLossless(
    std::string_view text) noexcept {
    try {
        if (text.empty() ||
            text.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
            return std::nullopt;
        }
        const int sourceLength = static_cast<int>(text.size());
        const int wideLength = MultiByteToWideChar(
            CP_ACP, MB_PRECOMPOSED, text.data(), sourceLength, nullptr, 0);
        if (wideLength <= 0 ||
            static_cast<std::size_t>(wideLength) > kMaximumGuiTextUnits) {
            return std::nullopt;
        }
        std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
        if (MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, text.data(),
                                sourceLength, wide.data(), wideLength) !=
                wideLength ||
            HasControlCharacter(wide)) {
            return std::nullopt;
        }

        BOOL usedDefault = FALSE;
        const int roundTripLength = WideCharToMultiByte(
            CP_ACP, WC_NO_BEST_FIT_CHARS, wide.data(), wideLength, nullptr, 0,
            nullptr, &usedDefault);
        if (roundTripLength != sourceLength || usedDefault != FALSE) {
            return std::nullopt;
        }
        std::string roundTrip(static_cast<std::size_t>(roundTripLength), '\0');
        usedDefault = FALSE;
        if (WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wide.data(),
                                wideLength, roundTrip.data(), roundTripLength,
                                nullptr, &usedDefault) != roundTripLength ||
            usedDefault != FALSE || std::string_view(roundTrip) != text) {
            return std::nullopt;
        }
        return wide;
    } catch (...) {
        return std::nullopt;
    }
}

FixedMapRowCalibration::FixedMapRowCalibration(
    const CompletionMarkerIndex& index, TraceSink trace)
    : index_(index), trace_(std::move(trace)) {}

void FixedMapRowCalibration::ObservePages(
    const PageSnapshot& snapshot) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) {
            return;
        }
        const bool exact = HasExactFixedMapPages(snapshot);
        if (exact == exactPages_) {
            return;
        }
        exactPages_ = exact;
        listKind_ = FixedMapListKind::Unknown;
        AdvanceGeneration();
        std::ostringstream line;
        line << "marker-state=pages-" << (exactPages_ ? "fixed" : "other")
             << ";list-unknown;generation-" << generation_;
        SafeWrite(line.str());
    } catch (...) {
    }
}

void FixedMapRowCalibration::ObserveListKind(
    FixedMapListKind kind) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) {
            return;
        }
        const auto admitted = exactPages_ ? kind : FixedMapListKind::Unknown;
        if (admitted == listKind_) {
            return;
        }
        listKind_ = admitted;
        AdvanceGeneration();
        std::ostringstream line;
        line << "marker-state=pages-" << (exactPages_ ? "fixed" : "other")
             << ";list-" << FixedMapListKindName(listKind_)
             << ";generation-" << generation_;
        SafeWrite(line.str());
    } catch (...) {
    }
}

void FixedMapRowCalibration::ObserveFrame(DWORD page,
                                          INT32 pillarboxWidth) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) {
            return;
        }
        ++sequence_;
        if (!exactPages_ || listKind_ == FixedMapListKind::Unknown ||
            firstCandidateSequence_ == 0u) {
            return;
        }
        std::ostringstream line;
        line << "marker-frame=sequence-" << sequence_
             << ";generation-" << generation_ << ";page-" << page
             << ";pillarbox-" << pillarboxWidth << ";candidate-first-"
             << firstCandidateSequence_ << ";candidate-last-"
             << lastCandidateSequence_;
        SafeWrite(line.str());
        firstCandidateSequence_ = 0u;
        lastCandidateSequence_ = 0u;
    } catch (...) {
    }
}

void FixedMapRowCalibration::ObserveElement(
    const MarkerGuiElement& element) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) {
            return;
        }
        ++sequence_;
        if (!exactPages_ || listKind_ == FixedMapListKind::Unknown ||
            !ValidGeometry(element)) {
            return;
        }
        const auto decoded = DecodeMenuTextLossless(element.text);
        if (!decoded.has_value()) {
            return;
        }
        const auto matches = index_.Find(listKind_, *decoded);
        if (matches.empty()) {
            return;
        }
        const auto utf8 = WideToStrictUtf8(*decoded);
        if (!utf8.has_value()) {
            return;
        }

        std::ostringstream line;
        line << "marker-candidate=sequence-" << sequence_
             << ";generation-" << generation_ << ";list-"
             << FixedMapListKindName(listKind_) << ";name-" << *utf8
             << ";rect-" << element.x << ',' << element.y << ','
             << element.width << ',' << element.height << ";surface-"
             << element.surfaceWidth << ',' << element.surfaceHeight
             << ";value-link-" << element.valueLink << ";container-"
             << element.containerType << ";text-style-" << element.textStyle
             << ";match-" << (matches.size() == 1u ? "unique" : "ambiguous");
        SafeWrite(line.str());
        if (firstCandidateSequence_ == 0u) {
            firstCandidateSequence_ = sequence_;
        }
        lastCandidateSequence_ = sequence_;
    } catch (...) {
    }
}

void FixedMapRowCalibration::Disable() noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        exactPages_ = false;
        listKind_ = FixedMapListKind::Unknown;
        firstCandidateSequence_ = 0u;
        lastCandidateSequence_ = 0u;
    } catch (...) {
    }
}

void FixedMapRowCalibration::AdvanceGeneration() noexcept {
    ++generation_;
    firstCandidateSequence_ = 0u;
    lastCandidateSequence_ = 0u;
}

void FixedMapRowCalibration::SafeWrite(std::string record) noexcept {
    try {
        if (trace_) {
            trace_(record);
        }
    } catch (...) {
    }
}

}  // namespace campaign_completion
