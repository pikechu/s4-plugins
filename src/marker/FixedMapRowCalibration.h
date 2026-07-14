#pragma once

#include "marker/CompletionMarkerIndex.h"
#include "runtime/PageObservation.h"

#include <windows.h>

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace campaign_completion {

struct MarkerGuiElement final {
    DWORD surfaceWidth = 0u;
    DWORD surfaceHeight = 0u;
    WORD containerType = 0u;
    WORD x = 0u;
    WORD y = 0u;
    WORD width = 0u;
    WORD height = 0u;
    WORD valueLink = 0u;
    WORD textStyle = 0u;
    std::string text;
};

bool CopyBoundedGuiText(const char* text, std::string& output) noexcept;
std::optional<std::wstring> DecodeMenuTextLossless(
    std::string_view text) noexcept;

class FixedMapRowCalibration final {
public:
    using TraceSink = std::function<bool(std::string_view)>;

    FixedMapRowCalibration(const CompletionMarkerIndex& index,
                           TraceSink trace);
    void ObservePages(const PageSnapshot& snapshot) noexcept;
    void ObserveListKind(FixedMapListKind kind) noexcept;
    void ObserveFrame(DWORD page, INT32 pillarboxWidth) noexcept;
    void ObserveElement(const MarkerGuiElement& element) noexcept;
    void Disable() noexcept;

private:
    void AdvanceGeneration() noexcept;
    void SafeWrite(std::string record) noexcept;

    const CompletionMarkerIndex& index_;
    TraceSink trace_;
    std::mutex mutex_;
    FixedMapListKind listKind_ = FixedMapListKind::Unknown;
    std::uint64_t sequence_ = 0u;
    std::uint64_t generation_ = 0u;
    std::uint64_t firstCandidateSequence_ = 0u;
    std::uint64_t lastCandidateSequence_ = 0u;
    bool exactPages_ = false;
    bool enabled_ = true;
};

}  // namespace campaign_completion
