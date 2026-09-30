#include "music_repair/MusicEventTracker.h"

#include <utility>

namespace music_loop_repair {

void MusicEventTracker::ObserveOpen(std::uintptr_t stream, std::string path) {
    if (stream == 0u) return;
    streams_[stream] = {std::move(path), 0u, 0u};
}

StartObservation MusicEventTracker::ObserveStart(
    std::uintptr_t stream, std::uint64_t timestampMs) {
    StartObservation result;
    const auto found = streams_.find(stream);
    if (found == streams_.end()) {
        result.repeat = RepeatKind::UnknownStream;
        return result;
    }

    auto& state = found->second;
    result.path = state.path;
    result.streamStarts = state.starts + 1u;

    if (state.starts != 0u && timestampMs >= state.lastStartMs) {
        const auto interval = timestampMs - state.lastStartMs;
        if (interval <= duplicateWindowMs_) {
            result.repeat = RepeatKind::SameStream;
            result.intervalMs = interval;
        }
    }

    const auto pathStart = pathStarts_.find(state.path);
    if (result.repeat == RepeatKind::None &&
        pathStart != pathStarts_.end() && timestampMs >= pathStart->second) {
        const auto interval = timestampMs - pathStart->second;
        if (interval <= duplicateWindowMs_) {
            result.repeat = RepeatKind::SamePath;
            result.intervalMs = interval;
        }
    }

    state.lastStartMs = timestampMs;
    ++state.starts;
    pathStarts_[state.path] = timestampMs;
    return result;
}

void MusicEventTracker::ObserveClose(std::uintptr_t stream) {
    streams_.erase(stream);
}

const char* RepeatKindName(RepeatKind kind) noexcept {
    switch (kind) {
        case RepeatKind::None: return "none";
        case RepeatKind::UnknownStream: return "unknown-stream";
        case RepeatKind::SameStream: return "same-stream";
        case RepeatKind::SamePath: return "same-path";
    }
    return "unknown";
}

}  // namespace music_loop_repair
