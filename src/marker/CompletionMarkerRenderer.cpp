#include "marker/CompletionMarkerRenderer.h"

#include <array>
#include <mutex>
#include <sstream>
#include <utility>

namespace campaign_completion {
namespace {

std::string_view FailureLog(MarkerRenderFailureStage stage) noexcept {
    switch (stage) {
        case MarkerRenderFailureStage::Input:
            return "completion-marker-renderer frame-failed stage=input";
        case MarkerRenderFailureStage::Describe:
            return "completion-marker-renderer frame-failed stage=describe";
        case MarkerRenderFailureStage::Geometry:
            return "completion-marker-renderer frame-failed stage=geometry";
        case MarkerRenderFailureStage::Begin:
            return "completion-marker-renderer frame-failed stage=begin";
        case MarkerRenderFailureStage::Draw:
            return "completion-marker-renderer frame-failed stage=draw";
        case MarkerRenderFailureStage::End:
            return "completion-marker-renderer frame-failed stage=end";
    }
    return "completion-marker-renderer frame-failed stage=input";
}

std::string_view DisabledLog(MarkerRenderFailureStage stage) noexcept {
    switch (stage) {
        case MarkerRenderFailureStage::Input:
            return "completion-marker-renderer disabled failures=3 last-stage=input";
        case MarkerRenderFailureStage::Describe:
            return "completion-marker-renderer disabled failures=3 last-stage=describe";
        case MarkerRenderFailureStage::Geometry:
            return "completion-marker-renderer disabled failures=3 last-stage=geometry";
        case MarkerRenderFailureStage::Begin:
            return "completion-marker-renderer disabled failures=3 last-stage=begin";
        case MarkerRenderFailureStage::Draw:
            return "completion-marker-renderer disabled failures=3 last-stage=draw";
        case MarkerRenderFailureStage::End:
            return "completion-marker-renderer disabled failures=3 last-stage=end";
    }
    return "completion-marker-renderer disabled failures=3 last-stage=input";
}

}  // namespace

CompletionMarkerRenderer::CompletionMarkerRenderer(
    IMarkerDrawingSurface& surface, LogSink log)
    : surface_(surface), log_(std::move(log)) {}

MarkerRenderStatus CompletionMarkerRenderer::Render(
    LPDIRECTDRAWSURFACE7 destination, INT32 pillarboxWidth,
    const MarkerFrameCommands& frame, std::uint64_t nowMs) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (disabled_) return MarkerRenderStatus::Disabled;
        if (frame.count == 0u) return MarkerRenderStatus::Skipped;
        if (frame.count > frame.commands.size() || destination == nullptr) {
            return Fail(MarkerRenderFailureStage::Input, nowMs);
        }

        MarkerSurfaceExtent extent{};
        if (!surface_.Describe(destination, extent)) {
            return Fail(MarkerRenderFailureStage::Describe, nowMs);
        }
        std::array<MarkerCheckGeometry, kMaximumVisibleFixedRows> geometry{};
        for (std::size_t index = 0u; index < frame.count; ++index) {
            const auto built = BuildMarkerCheckGeometry(
                frame.commands[index], pillarboxWidth, extent.width,
                extent.height);
            if (!built.has_value()) {
                return Fail(MarkerRenderFailureStage::Geometry, nowMs,
                            &frame.commands[index], pillarboxWidth, extent);
            }
            geometry[index] = *built;
        }
        if (!surface_.Begin(destination)) {
            return Fail(MarkerRenderFailureStage::Begin, nowMs);
        }
        bool drawSuccess = true;
        for (std::size_t index = 0u; index < frame.count; ++index) {
            drawSuccess =
                surface_.DrawOutlinedCheck(geometry[index]) && drawSuccess;
        }
        const bool endSuccess = surface_.End();
        if (!drawSuccess) {
            return Fail(MarkerRenderFailureStage::Draw, nowMs);
        }
        if (!endSuccess) {
            return Fail(MarkerRenderFailureStage::End, nowMs);
        }
        failures_ = 0u;
        return MarkerRenderStatus::Drawn;
    } catch (...) {
        return MarkerRenderStatus::Failed;
    }
}

void CompletionMarkerRenderer::Disable() noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        disabled_ = true;
    } catch (...) {
    }
}

MarkerRenderStatus CompletionMarkerRenderer::Fail(
    MarkerRenderFailureStage stage, std::uint64_t nowMs,
    const MarkerDrawCommand* row, INT32 pillarboxWidth,
    MarkerSurfaceExtent extent) noexcept {
    ++failures_;
    if (!failureLogged_ || nowMs - lastFailureLogMs_ >= 5000u) {
        if (stage == MarkerRenderFailureStage::Geometry && row != nullptr) {
            try {
                std::ostringstream message;
                message << FailureLog(stage) << " pillarbox="
                        << pillarboxWidth << " destination=" << extent.width
                        << ',' << extent.height << " logical="
                        << row->logicalSurfaceWidth << ','
                        << row->logicalSurfaceHeight << " row=" << row->x
                        << ',' << row->y << ',' << row->width << ','
                        << row->height;
                SafeLog(message.str());
            } catch (...) {
                SafeLog(FailureLog(stage));
            }
        } else {
            SafeLog(FailureLog(stage));
        }
        failureLogged_ = true;
        lastFailureLogMs_ = nowMs;
    }
    if (failures_ >= 3u) {
        disabled_ = true;
        SafeLog(DisabledLog(stage));
        return MarkerRenderStatus::Disabled;
    }
    return MarkerRenderStatus::Failed;
}

void CompletionMarkerRenderer::SafeLog(std::string_view line) noexcept {
    try {
        if (log_) log_(LogLevel::Error, std::string(line));
    } catch (...) {
    }
}

}  // namespace campaign_completion
