#pragma once

#include "marker/BoundedMenuText.h"
#include "marker/CompletionMarkerIndex.h"
#include "marker/FixedMapMenuReader.h"
#include "runtime/PageObservation.h"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace campaign_completion {

inline constexpr std::size_t kMaximumVisibleFixedRows = 6u;
inline constexpr std::size_t kMaximumMarkerCommands = 36u;

struct FixedMapRowObservation final {
    DWORD surfaceWidth = 0u;
    DWORD surfaceHeight = 0u;
    WORD containerType = 0u;
    WORD x = 0u;
    WORD y = 0u;
    WORD width = 0u;
    WORD height = 0u;
    WORD valueLink = 0u;
    WORD textStyle = 0u;
    BoundedMenuText text{};
};

struct MarkerDrawCommand final {
    DWORD logicalSurfaceWidth = 0u;
    DWORD logicalSurfaceHeight = 0u;
    WORD x = 0u;
    WORD y = 0u;
    WORD width = 0u;
    WORD height = 0u;
};

struct MarkerFrameCommands final {
    std::array<MarkerDrawCommand, kMaximumMarkerCommands> commands{};
    std::size_t count = 0u;
    std::uint64_t generation = 0u;
};

class FixedMapRowObserver final {
public:
    explicit FixedMapRowObserver(const CompletionMarkerIndex& index) noexcept;

    void ObservePages(const PageSnapshot& snapshot) noexcept;
    void ObserveListKind(FixedMapListKind kind) noexcept;
    void ObserveElement(const FixedMapRowObservation& element) noexcept;
    void ObserveInternalMenu(const FixedMapMenuSnapshot& snapshot) noexcept;
    MarkerFrameCommands TakeFrame(DWORD page) noexcept;
    void Disable() noexcept;

private:
    struct PendingRow final {
        BoundedWideText label{};
        MarkerDrawCommand command{};
        bool ambiguous = false;
    };

    void ClearFrame() noexcept;
    void ClearPending() noexcept;
    void InvalidateFrame() noexcept;

    const CompletionMarkerIndex& index_;
    std::mutex mutex_;
    std::array<PendingRow, kMaximumVisibleFixedRows> pending_{};
    std::array<MarkerDrawCommand, kMaximumVisibleFixedRows> retained_{};
    std::array<bool, kMaximumVisibleFixedRows> retainedActive_{};
    std::array<bool, kMaximumVisibleFixedRows> observedSlots_{};
    std::size_t pendingCount_ = 0u;
    std::uint64_t generation_ = 0u;
    FixedMapListKind listKind_ = FixedMapListKind::Unknown;
    bool exactPages_ = false;
    bool invalidFrame_ = false;
    bool internalSnapshotActive_ = false;
    bool enabled_ = true;
};

}  // namespace campaign_completion
