#pragma once

#include "diagnostics/Logger.h"
#include "music_repair/MusicEventTracker.h"
#include "music_repair/MusicIatHooks.h"

#include <windows.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>

namespace music_loop_repair {

enum class MusicEventKind : std::uint8_t {
    Open,
    Start,
    Close,
    Pause,
    Position,
};

struct MusicEvent final {
    std::uint64_t sequence = 0u;
    std::uint64_t timestampMs = 0u;
    std::uintptr_t stream = 0u;
    DWORD threadId = 0u;
    MusicEventKind kind = MusicEventKind::Open;
    std::int32_t value = 0;
    std::array<char, 260> path{};
};

class MusicRepairRuntime final {
public:
    bool Start(HMODULE module);
    void RunControlLoop();
    void RequestStop() noexcept;

private:
    struct EventSlot final {
        std::atomic<std::uint32_t> state{0u};
        MusicEvent event{};
    };

    static void OnOpen(void* stream, const char* path,
                       std::int32_t streamMemory) noexcept;
    static void OnStart(void* stream) noexcept;
    static void OnClose(void* stream) noexcept;
    static void OnPause(void* stream, std::int32_t paused) noexcept;
    static void OnPosition(void* stream, std::int32_t offset) noexcept;

    void Capture(MusicEventKind kind, void* stream, std::int32_t value,
                 const char* path = nullptr) noexcept;
    void DrainEvents();
    void LogEvent(const MusicEvent& event);
    void Stop() noexcept;

    static std::atomic<MusicRepairRuntime*> active_;

    static constexpr std::size_t kEventCapacity = 256u;
    campaign_completion::Logger logger_;
    MusicIatHooks hooks_;
    MusicEventTracker tracker_;
    std::filesystem::path stopPath_;
    std::array<EventSlot, kEventCapacity> events_{};
    std::atomic<std::uint64_t> writeSequence_{0u};
    std::atomic<std::uint64_t> dropped_{0u};
    std::atomic<bool> accepting_{false};
    std::atomic<bool> stopRequested_{false};
    std::atomic<std::uint32_t> capturesInFlight_{0u};
    std::uint64_t loggedDropped_ = 0u;
    bool started_ = false;

    friend DWORD WINAPI MusicRepairBootstrapThread(void* module);
};

MusicRepairRuntime& MusicRepairRuntimeInstance();
DWORD WINAPI MusicRepairBootstrapThread(void* module);

}  // namespace music_loop_repair
