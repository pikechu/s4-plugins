#include "marker/FixedMapRowObserver.h"

#include <windows.h>

#include <cstddef>
#include <limits>
#include <mutex>
#include <string_view>

namespace campaign_completion {
namespace {

constexpr DWORD kLogicalSurfaceWidth = 800u;
constexpr DWORD kLogicalSurfaceHeight = 600u;
constexpr WORD kRowContainerType = 0u;
constexpr WORD kRowX = 115u;
constexpr WORD kFirstRowY = 142u;
constexpr WORD kRowStride = 31u;
constexpr WORD kRowWidth = 271u;
constexpr WORD kRowHeight = 30u;
constexpr WORD kFirstValueLink = 2417u;

bool HasExactFixedMapPages(const PageSnapshot& snapshot) noexcept {
    return snapshot.activePages.size() == 4u &&
           snapshot.activePages[0] == 4u &&
           snapshot.activePages[1] == 22u &&
           snapshot.activePages[2] == 23u &&
           snapshot.activePages[3] == 25u;
}

bool HasBaseRowSignature(const FixedMapRowObservation& element) noexcept {
    return element.surfaceWidth == kLogicalSurfaceWidth &&
           element.surfaceHeight == kLogicalSurfaceHeight &&
           element.containerType == kRowContainerType &&
           element.x == kRowX && element.width == kRowWidth &&
           element.height == kRowHeight &&
           (element.textStyle == 1u || element.textStyle == 2u);
}

bool TryGetRowSlot(const FixedMapRowObservation& element,
                   std::size_t& slot) noexcept {
    if (element.y < kFirstRowY) {
        return false;
    }
    const auto delta = static_cast<unsigned>(element.y - kFirstRowY);
    if (delta % kRowStride != 0u) {
        return false;
    }
    const auto candidate = delta / kRowStride;
    const auto expectedLink =
        static_cast<unsigned>(kFirstValueLink) + candidate;
    if (expectedLink > (std::numeric_limits<WORD>::max)() ||
        element.valueLink != static_cast<WORD>(expectedLink)) {
        return false;
    }
    slot = static_cast<std::size_t>(candidate);
    return true;
}

bool EqualLabel(std::wstring_view left,
                std::wstring_view right) noexcept {
    if (left.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()) ||
        right.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return false;
    }
    return CompareStringOrdinal(
               left.data(), static_cast<int>(left.size()), right.data(),
               static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
}

bool EqualCommand(const MarkerDrawCommand& left,
                  const MarkerDrawCommand& right) noexcept {
    return left.logicalSurfaceWidth == right.logicalSurfaceWidth &&
           left.logicalSurfaceHeight == right.logicalSurfaceHeight &&
           left.x == right.x && left.y == right.y &&
           left.width == right.width && left.height == right.height;
}

MarkerDrawCommand MakeCommand(
    const FixedMapRowObservation& element) noexcept {
    return {element.surfaceWidth, element.surfaceHeight, element.x, element.y,
            element.width, element.height};
}

MarkerDrawCommand MakeCommand(std::size_t slot) noexcept {
    return {kLogicalSurfaceWidth, kLogicalSurfaceHeight, kRowX,
            static_cast<WORD>(kFirstRowY + kRowStride * slot),
            kRowWidth, kRowHeight};
}

}  // namespace

FixedMapRowObserver::FixedMapRowObserver(
    const CompletionMarkerIndex& index) noexcept
    : index_(index) {}

void FixedMapRowObserver::ObservePages(
    const PageSnapshot& snapshot) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return;
        const bool exact = HasExactFixedMapPages(snapshot);
        if (!exact) {
            exactPages_ = false;
            listKind_ = FixedMapListKind::Unknown;
            ClearFrame();
            return;
        }
        if (exactPages_) return;
        exactPages_ = exact;
        listKind_ = FixedMapListKind::Single;
        retainedActive_.fill(false);
    } catch (...) {
    }
}

void FixedMapRowObserver::ObserveListKind(
    FixedMapListKind kind) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return;
        const auto admitted = exactPages_ ? kind : FixedMapListKind::Unknown;
        if (admitted == listKind_) return;
        listKind_ = admitted;
        ClearFrame();
    } catch (...) {
    }
}

void FixedMapRowObserver::ObserveElement(
    const FixedMapRowObservation& element) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_ || internalSnapshotActive_ || invalidFrame_ ||
            !HasBaseRowSignature(element)) {
            return;
        }

        const auto matchKind = exactPages_ ? listKind_
                                           : FixedMapListKind::Single;
        if (matchKind == FixedMapListKind::Unknown) return;

        std::size_t slot = 0u;
        if (!TryGetRowSlot(element, slot)) {
            return;
        }
        if (slot >= kMaximumVisibleFixedRows) {
            InvalidateFrame();
            return;
        }
        observedSlots_[slot] = true;

        BoundedWideText label{};
        if (!DecodeMenuTextLossless(element.text, label) ||
            index_.Match(matchKind, label.view()) !=
                MarkerMatchStatus::Unique) {
            return;
        }

        const auto command = MakeCommand(element);
        for (std::size_t index = 0u; index < pendingCount_; ++index) {
            auto& pending = pending_[index];
            if (EqualLabel(pending.label.view(), label.view())) {
                if (!EqualCommand(pending.command, command)) {
                    pending.ambiguous = true;
                }
                return;
            }
            if (EqualCommand(pending.command, command)) {
                InvalidateFrame();
                return;
            }
        }

        if (pendingCount_ >= pending_.size()) {
            InvalidateFrame();
            return;
        }
        pending_[pendingCount_] = {label, command, false};
        ++pendingCount_;
    } catch (...) {
    }
}

void FixedMapRowObserver::ObserveInternalMenu(
    const FixedMapMenuSnapshot& snapshot) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_ || !exactPages_) return;

        retainedActive_.fill(false);
        ClearPending();
        internalSnapshotActive_ = true;
        if (snapshot.status != FixedMapMenuReadStatus::Success ||
            listKind_ == FixedMapListKind::Unknown ||
            snapshot.rowCount > kMaximumVisibleFixedRows) {
            return;
        }
        for (std::size_t slot = 0u; slot < snapshot.rowCount; ++slot) {
            if (index_.MatchRelative(
                    listKind_,
                    snapshot.relativeIdentifiers[slot].view()) ==
                MarkerMatchStatus::Unique) {
                retained_[slot] = MakeCommand(slot);
                retainedActive_[slot] = true;
            }
        }
    } catch (...) {
    }
}

MarkerFrameCommands FixedMapRowObserver::TakeFrame(DWORD page) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        MarkerFrameCommands frame{};
        frame.generation = generation_;
        if (!enabled_ || !exactPages_ || page != 25u) return frame;

        if (internalSnapshotActive_) {
            for (std::size_t slot = 0u; slot < retained_.size(); ++slot) {
                if (retainedActive_[slot]) {
                    frame.commands[frame.count] = retained_[slot];
                    ++frame.count;
                }
            }
            return frame;
        }

        if (invalidFrame_) {
            retainedActive_.fill(false);
        } else {
            for (std::size_t slot = 0u; slot < observedSlots_.size(); ++slot) {
                if (observedSlots_[slot]) retainedActive_[slot] = false;
            }
            for (std::size_t index = 0u; index < pendingCount_; ++index) {
                if (!pending_[index].ambiguous) {
                    const auto& command = pending_[index].command;
                    const auto delta =
                        static_cast<unsigned>(command.y - kFirstRowY);
                    const auto slot =
                        static_cast<std::size_t>(delta / kRowStride);
                    retained_[slot] = command;
                    retainedActive_[slot] = true;
                }
            }
        }
        for (std::size_t slot = 0u; slot < retained_.size(); ++slot) {
            if (retainedActive_[slot]) {
                frame.commands[frame.count] = retained_[slot];
                ++frame.count;
            }
        }
        ClearPending();
        return frame;
    } catch (...) {
        return {};
    }
}

void FixedMapRowObserver::Disable() noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return;
        enabled_ = false;
        exactPages_ = false;
        listKind_ = FixedMapListKind::Unknown;
        ClearFrame();
    } catch (...) {
    }
}

void FixedMapRowObserver::ClearFrame() noexcept {
    retainedActive_.fill(false);
    internalSnapshotActive_ = false;
    ClearPending();
}

void FixedMapRowObserver::ClearPending() noexcept {
    pendingCount_ = 0u;
    observedSlots_.fill(false);
    invalidFrame_ = false;
    ++generation_;
}

void FixedMapRowObserver::InvalidateFrame() noexcept {
    pendingCount_ = 0u;
    invalidFrame_ = true;
}

}  // namespace campaign_completion
