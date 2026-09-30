#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace music_loop_repair {

enum class RepeatKind {
    None,
    UnknownStream,
    SameStream,
    SamePath,
};

struct StartObservation final {
    RepeatKind repeat = RepeatKind::None;
    std::uint64_t intervalMs = 0u;
    std::uint32_t streamStarts = 0u;
    std::string path;
};

class MusicEventTracker final {
public:
    explicit MusicEventTracker(std::uint64_t duplicateWindowMs = 15'000u)
        : duplicateWindowMs_(duplicateWindowMs) {}

    void ObserveOpen(std::uintptr_t stream, std::string path);
    StartObservation ObserveStart(std::uintptr_t stream,
                                  std::uint64_t timestampMs);
    void ObserveClose(std::uintptr_t stream);

private:
    struct StreamState final {
        std::string path;
        std::uint64_t lastStartMs = 0u;
        std::uint32_t starts = 0u;
    };

    std::uint64_t duplicateWindowMs_;
    std::unordered_map<std::uintptr_t, StreamState> streams_;
    std::unordered_map<std::string, std::uint64_t> pathStarts_;
};

const char* RepeatKindName(RepeatKind kind) noexcept;

}  // namespace music_loop_repair
