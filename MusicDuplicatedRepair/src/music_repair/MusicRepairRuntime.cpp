#include "music_repair/MusicRepairRuntime.h"

#include "diagnostics/ModuleInventory.h"

#include <algorithm>
#include <cstring>
#include <cwctype>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace music_loop_repair {
namespace {

using campaign_completion::CheckTargetExecutable;
using campaign_completion::CompatibilityResult;
using campaign_completion::EnumerateLoadedModules;
using campaign_completion::LogLevel;
using campaign_completion::ModuleInfo;

std::filesystem::path ModulePath(HMODULE module) {
    std::wstring path(32768u, L'\0');
    const DWORD length = GetModuleFileNameW(
        module, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0u || length == path.size()) return {};
    path.resize(length);
    return std::filesystem::path(path);
}

bool EqualInsensitive(std::wstring value, const wchar_t* expected) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value == expected;
}

const char* CompatibilityName(CompatibilityResult result) noexcept {
    switch (result) {
        case CompatibilityResult::Compatible: return "compatible";
        case CompatibilityResult::VersionMismatch: return "version-mismatch";
        case CompatibilityResult::HashMismatch: return "hash-mismatch";
    }
    return "unknown";
}

const char* EventKindName(MusicEventKind kind) noexcept {
    switch (kind) {
        case MusicEventKind::Open: return "open";
        case MusicEventKind::Start: return "start";
        case MusicEventKind::Close: return "close";
        case MusicEventKind::Pause: return "pause";
        case MusicEventKind::Position: return "position";
    }
    return "unknown";
}

std::string EscapePath(const char* path) {
    std::string result;
    if (path == nullptr) return result;
    result.reserve(260u);
    for (std::size_t index = 0u; index < 259u && path[index] != '\0';
         ++index) {
        const unsigned char ch = static_cast<unsigned char>(path[index]);
        if (ch == '\\' || ch == '"') {
            result.push_back('\\');
            result.push_back(static_cast<char>(ch));
        } else if (ch >= 0x20u && ch != 0x7fu) {
            result.push_back(static_cast<char>(ch));
        } else {
            result.push_back('?');
        }
    }
    return result;
}

void CopyPath(std::array<char, 260>& destination,
              const char* source) noexcept {
    destination.fill('\0');
    if (source == nullptr) return;
#if defined(_MSC_VER)
    __try {
        for (std::size_t index = 0u; index + 1u < destination.size();
             ++index) {
            const char ch = source[index];
            destination[index] = ch;
            if (ch == '\0') break;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        destination.fill('\0');
    }
#else
    std::strncpy(destination.data(), source, destination.size() - 1u);
#endif
}

}  // namespace

std::atomic<MusicRepairRuntime*> MusicRepairRuntime::active_{nullptr};

MusicRepairRuntime& MusicRepairRuntimeInstance() {
    static auto* const runtime = new MusicRepairRuntime();
    return *runtime;
}

bool MusicRepairRuntime::Start(HMODULE module) {
    static_assert(sizeof(void*) == 4u,
                  "MusicLoopRepair must be built as 32-bit");
    if (module == nullptr || started_) return false;

    const auto modulePath = ModulePath(module);
    if (modulePath.empty()) return false;
    const auto dataDirectory = modulePath.parent_path() / L"MusicLoopRepair";
    stopPath_ = dataDirectory / L"MusicLoopRepair.stop";
    if (!logger_.Open(dataDirectory / L"MusicLoopRepair.log")) return false;
    logger_.Write(
        LogLevel::Info,
        "MusicLoopRepair bootstrap version=0.1.0 mode=diagnostic-only");

    const auto modules = EnumerateLoadedModules();
    const ModuleInfo* executable = nullptr;
    for (const auto& loaded : modules) {
        if (EqualInsensitive(loaded.name, L"s4_main.exe")) {
            executable = &loaded;
            break;
        }
    }
    if (executable == nullptr) {
        logger_.Write(LogLevel::Error, "S4_Main.exe was not found");
        logger_.Close();
        return false;
    }
    const auto compatibility = CheckTargetExecutable(*executable);
    logger_.Write(LogLevel::Info,
                  std::string("executable version=") + executable->version +
                      " sha256=" + executable->sha256 +
                      " process-id=" + std::to_string(GetCurrentProcessId()));
    logger_.Write(compatibility == CompatibilityResult::Compatible
                      ? LogLevel::Info
                      : LogLevel::Error,
                  std::string("executable compatibility=") +
                      CompatibilityName(compatibility));
    if (compatibility != CompatibilityResult::Compatible) {
        logger_.Close();
        return false;
    }

    const auto iniPath = dataDirectory / L"MusicLoopRepair.ini";
    const auto window = GetPrivateProfileIntW(
        L"Diagnostic", L"DuplicateWindowMs", 15000, iniPath.c_str());
    const auto duplicateWindow =
        window >= 1000 && window <= 300000 ? window : 15000;
    tracker_ = MusicEventTracker(static_cast<std::uint64_t>(duplicateWindow));

    active_.store(this, std::memory_order_release);
    accepting_.store(true, std::memory_order_release);
    const MusicHookCallbacks callbacks{
        &OnOpen, &OnStart, &OnClose, &OnPause, &OnPosition};
    if (!hooks_.Start(reinterpret_cast<HMODULE>(executable->baseAddress),
                      executable->size, callbacks)) {
        accepting_.store(false, std::memory_order_release);
        active_.store(nullptr, std::memory_order_release);
        logger_.Write(
            LogLevel::Error,
            std::string("music hook admission failed reason=") +
                MusicHookFailureName(hooks_.failure()) +
                " hooks-remaining=" + std::to_string(hooks_.installed()));
        logger_.Close();
        return false;
    }

    started_ = true;
    std::ostringstream started;
    started << "runtime started hooks=" << hooks_.installed()
            << " duplicate-window-ms=" << duplicateWindow
            << " repair-enabled=false";
    logger_.Write(LogLevel::Info, started.str());
    return true;
}

void MusicRepairRuntime::Capture(MusicEventKind kind, void* stream,
                                 std::int32_t value,
                                 const char* path) noexcept {
    // A delayed callback may have loaded active_ before Stop clears it.
    // Sequentially consistent admission makes it either visible to the drain
    // or reject itself before touching a queue slot.
    capturesInFlight_.fetch_add(1u);
    if (!accepting_.load()) {
        capturesInFlight_.fetch_sub(1u);
        return;
    }
    const auto sequence =
        writeSequence_.fetch_add(1u, std::memory_order_relaxed);
    auto& slot = events_[sequence % events_.size()];
    std::uint32_t expected = 0u;
    if (!slot.state.compare_exchange_strong(
            expected, 1u, std::memory_order_acq_rel,
            std::memory_order_relaxed)) {
        dropped_.fetch_add(1u, std::memory_order_relaxed);
        capturesInFlight_.fetch_sub(1u);
        return;
    }
    slot.event.sequence = sequence;
    slot.event.timestampMs = GetTickCount64();
    slot.event.stream = reinterpret_cast<std::uintptr_t>(stream);
    slot.event.threadId = GetCurrentThreadId();
    slot.event.kind = kind;
    slot.event.value = value;
    CopyPath(slot.event.path, path);
    slot.state.store(2u, std::memory_order_release);
    capturesInFlight_.fetch_sub(1u);
}

void MusicRepairRuntime::OnOpen(void* stream, const char* path,
                                std::int32_t streamMemory) noexcept {
    auto* runtime = active_.load(std::memory_order_acquire);
    if (runtime != nullptr) {
        runtime->Capture(MusicEventKind::Open, stream, streamMemory, path);
    }
}

void MusicRepairRuntime::OnStart(void* stream) noexcept {
    auto* runtime = active_.load(std::memory_order_acquire);
    if (runtime != nullptr) {
        runtime->Capture(MusicEventKind::Start, stream, 0);
    }
}

void MusicRepairRuntime::OnClose(void* stream) noexcept {
    auto* runtime = active_.load(std::memory_order_acquire);
    if (runtime != nullptr) {
        runtime->Capture(MusicEventKind::Close, stream, 0);
    }
}

void MusicRepairRuntime::OnPause(void* stream,
                                 std::int32_t paused) noexcept {
    auto* runtime = active_.load(std::memory_order_acquire);
    if (runtime != nullptr) {
        runtime->Capture(MusicEventKind::Pause, stream, paused);
    }
}

void MusicRepairRuntime::OnPosition(void* stream,
                                    std::int32_t offset) noexcept {
    auto* runtime = active_.load(std::memory_order_acquire);
    if (runtime != nullptr) {
        runtime->Capture(MusicEventKind::Position, stream, offset);
    }
}

void MusicRepairRuntime::DrainEvents() {
    std::vector<MusicEvent> ready;
    ready.reserve(events_.size());
    for (auto& slot : events_) {
        if (slot.state.load(std::memory_order_acquire) != 2u) continue;
        ready.push_back(slot.event);
        slot.state.store(0u, std::memory_order_release);
    }
    std::sort(ready.begin(), ready.end(),
              [](const MusicEvent& left, const MusicEvent& right) {
                  return left.sequence < right.sequence;
              });
    for (const auto& event : ready) LogEvent(event);
    const auto dropped = dropped_.load(std::memory_order_relaxed);
    if (dropped != loggedDropped_) {
        logger_.Write(LogLevel::Warning,
                      "event queue incomplete dropped-events=" +
                          std::to_string(dropped));
        loggedDropped_ = dropped;
    }
}

void MusicRepairRuntime::LogEvent(const MusicEvent& event) {
    std::ostringstream line;
    line << "music-event seq=" << event.sequence
         << " uptime-ms=" << event.timestampMs
         << " kind=" << EventKindName(event.kind) << " stream=0x"
         << std::hex << event.stream << std::dec
         << " thread=" << event.threadId;

    LogLevel level = LogLevel::Info;
    if (event.kind == MusicEventKind::Open) {
        const auto path = EscapePath(event.path.data());
        tracker_.ObserveOpen(event.stream, std::string(event.path.data()));
        line << " stream-memory=" << event.value << " path=\"" << path
             << '"';
        if (event.stream == 0u) level = LogLevel::Warning;
    } else if (event.kind == MusicEventKind::Start) {
        const auto observation =
            tracker_.ObserveStart(event.stream, event.timestampMs);
        line << " path=\"" << EscapePath(observation.path.c_str()) << '"'
             << " stream-starts=" << observation.streamStarts
             << " repeat=" << RepeatKindName(observation.repeat);
        if (observation.intervalMs != 0u) {
            line << " interval-ms=" << observation.intervalMs;
        }
        if (observation.repeat == RepeatKind::SameStream ||
            observation.repeat == RepeatKind::SamePath) {
            level = LogLevel::Warning;
        }
    } else if (event.kind == MusicEventKind::Close) {
        tracker_.ObserveClose(event.stream);
    } else {
        line << " value=" << event.value;
    }
    logger_.Write(level, line.str());
}

void MusicRepairRuntime::RequestStop() noexcept {
    stopRequested_.store(true, std::memory_order_release);
}

void MusicRepairRuntime::RunControlLoop() {
    try {
        while (!stopRequested_.load(std::memory_order_acquire)) {
            DrainEvents();
            std::error_code error;
            if (std::filesystem::exists(stopPath_, error) && !error) {
                std::filesystem::remove(stopPath_, error);
                RequestStop();
                break;
            }
            Sleep(50u);
        }
    } catch (...) {
        try {
            logger_.Write(LogLevel::Error,
                          "diagnostic control failed; requesting stop");
        } catch (...) {
        }
    }
    Stop();
}

void MusicRepairRuntime::Stop() noexcept {
    accepting_.store(false);
    active_.store(nullptr, std::memory_order_release);
    const bool restored = hooks_.Stop();
    while (capturesInFlight_.load() != 0u) Sleep(1u);
    try {
        DrainEvents();
        const auto dropped = dropped_.load(std::memory_order_relaxed);
        std::ostringstream line;
        line << "runtime stopped hooks-restored="
             << (restored ? "true" : "false")
             << " dropped-events=" << dropped;
        logger_.Write(restored && dropped == 0u ? LogLevel::Info
                                                : LogLevel::Warning,
                      line.str());
    } catch (...) {
        // Shutdown must not escape into the host process.
    }
    started_ = false;
    try {
        logger_.Close();
    } catch (...) {
    }
}

DWORD WINAPI MusicRepairBootstrapThread(void* module) {
    MusicRepairRuntime* runtime = nullptr;
    try {
        runtime = &MusicRepairRuntimeInstance();
        if (runtime->Start(static_cast<HMODULE>(module))) {
            runtime->RunControlLoop();
        }
        return 0u;
    } catch (...) {
        // Allocation, filesystem, or logging failures must not terminate S4.
        if (runtime != nullptr) runtime->Stop();
        return 1u;
    }
}

}  // namespace music_loop_repair
