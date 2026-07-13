#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace campaign_completion {

constexpr std::uint64_t kSettlementCaptureMs = 2'500u;
constexpr std::size_t kMaxSettlementFeatures = 256u;

struct SettlementFeature final {
    std::uint16_t gfxCollection = 0u;
    std::uint16_t containerType = 0u;
    std::uint16_t x = 0u;
    std::uint16_t y = 0u;
    std::uint16_t width = 0u;
    std::uint16_t height = 0u;
    std::uint16_t mainTexture = 0u;
    std::uint16_t valueLink = 0u;
    std::uint16_t buttonPressedTexture = 0u;
    std::uint16_t tooltipLink = 0u;
    std::uint16_t imageStyle = 0u;
    std::uint16_t effects = 0u;
    std::uint16_t textStyle = 0u;
    std::uint16_t showTexture = 0u;
    std::uint16_t backTexture = 0u;

    bool operator==(const SettlementFeature& other) const noexcept;
};

struct SettlementCapture final {
    std::uint64_t sessionId = 0u;
    std::vector<SettlementFeature> features;
    bool truncated = false;
};

class SettlementUiProbe final {
public:
    bool Begin(std::uint64_t sessionId, std::uint64_t nowMs) noexcept;
    void Observe(const SettlementFeature& feature) noexcept;
    std::optional<SettlementCapture> FinishIfDue(
        std::uint64_t nowMs) noexcept;
    void Disable() noexcept;
    bool Active() const noexcept { return active_; }

private:
    SettlementCapture capture_{};
    std::uint64_t startedAtMs_ = 0u;
    bool active_ = false;
    bool enabled_ = true;
};

}  // namespace campaign_completion
