#pragma once

#include <cstdint>
#include <mutex>
#include <optional>

namespace campaign_completion {

inline constexpr std::uint32_t kNativeTerminalEventId = 609u;

enum class NativeLocalResult {
    None,
    Won,
    Lost,
    Malformed,
    Conflict,
};

struct NativeEventFields final {
    std::uint32_t eventId = 0u;
    std::uint32_t wParam = 0u;
    std::uint32_t lParam = 0u;
    std::uint32_t gameTick = 0u;

    bool operator==(const NativeEventFields& other) const noexcept;
};

struct VictoryEventSnapshot final {
    std::uint64_t sessionId = 0u;
    NativeEventFields fields{};
    NativeLocalResult result = NativeLocalResult::None;
    std::uint32_t duplicates = 0u;
    std::uint32_t orphans = 0u;
};

class VictoryEventProbe final {
public:
    void BeginSession(std::uint64_t sessionId) noexcept;
    bool Observe(NativeEventFields fields) noexcept;
    std::optional<VictoryEventSnapshot> ConsumePending() noexcept;
    void Disable() noexcept;

private:
    std::mutex mutex_;
    VictoryEventSnapshot snapshot_{};
    bool pending_ = false;
    bool enabled_ = true;
};

}  // namespace campaign_completion
