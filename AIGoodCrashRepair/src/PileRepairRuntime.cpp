#include "PileRepairRuntime.h"

#include "diagnostics/ModuleInventory.h"

#include <algorithm>
#include <cwctype>
#include <sstream>
#include <string>
#include <system_error>

namespace pile_chain_repair {
namespace {

using campaign_completion::CheckTargetExecutable;
using campaign_completion::CompatibilityResult;
using campaign_completion::EnumerateLoadedModules;
using campaign_completion::LogLevel;
using campaign_completion::ModuleInfo;

constexpr std::uintptr_t kEntityTableRva = 0x00E9BC38u;
constexpr std::uintptr_t kEcoSectorTableRva = 0x0106C8A4u;
constexpr std::size_t kEntitySlots = 0x10000u;
constexpr std::size_t kEcoSectorSlots = 0x4000u;
constexpr GUID kSettlers4Api2Guid{
    0x05104b9fu,
    0x52d3u,
    0x4904u,
    {0x8du, 0x6fu, 0xa7u, 0xc3u, 0x01u, 0x2eu, 0xabu, 0xddu}};

S4API CreateSettlers4Api() noexcept {
    S4API api = nullptr;
    if (FAILED(S4CreateInterface(&kSettlers4Api2Guid, &api))) return nullptr;
    return api;
}

bool EqualInsensitive(std::wstring value, const wchar_t* expected) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value == expected;
}

bool ProtectionAllowsRead(DWORD protection) noexcept {
    protection &= 0xffu;
    return protection == PAGE_READONLY || protection == PAGE_READWRITE ||
           protection == PAGE_WRITECOPY ||
           protection == PAGE_EXECUTE_READ ||
           protection == PAGE_EXECUTE_READWRITE ||
           protection == PAGE_EXECUTE_WRITECOPY;
}

bool ProtectionAllowsWrite(DWORD protection) noexcept {
    protection &= 0xffu;
    return protection == PAGE_READWRITE || protection == PAGE_WRITECOPY ||
           protection == PAGE_EXECUTE_READWRITE ||
           protection == PAGE_EXECUTE_WRITECOPY;
}

bool AccessibleRange(const void* address, std::size_t bytes,
                     bool requireWrite) noexcept {
    if (address == nullptr || bytes == 0u) return false;
    auto current = reinterpret_cast<std::uintptr_t>(address);
    const auto end = current + bytes;
    if (end < current) return false;
    while (current < end) {
        MEMORY_BASIC_INFORMATION memory{};
        if (VirtualQuery(reinterpret_cast<const void*>(current), &memory,
                         sizeof(memory)) != sizeof(memory) ||
            memory.State != MEM_COMMIT ||
            (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0u ||
            !ProtectionAllowsRead(memory.Protect) ||
            (requireWrite && !ProtectionAllowsWrite(memory.Protect))) {
            return false;
        }
        const auto base =
            reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
        const auto next = base + memory.RegionSize;
        if (next <= current) return false;
        current = next;
    }
    return true;
}

std::filesystem::path ModulePath(HMODULE module) {
    std::wstring path(32768u, L'\0');
    const DWORD length = GetModuleFileNameW(
        module, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0u || length == path.size()) return {};
    path.resize(length);
    return std::filesystem::path(path);
}

const char* CompatibilityName(CompatibilityResult result) noexcept {
    switch (result) {
        case CompatibilityResult::Compatible: return "compatible";
        case CompatibilityResult::VersionMismatch: return "version-mismatch";
        case CompatibilityResult::HashMismatch: return "hash-mismatch";
    }
    return "unknown";
}

}  // namespace

std::atomic<PileRepairRuntime*> PileRepairRuntime::active_{nullptr};

PileRepairRuntime& PileRepairRuntimeInstance() {
    static auto* const runtime = new PileRepairRuntime();
    return *runtime;
}

bool PileRepairRuntime::Start(HMODULE module) {
    static_assert(sizeof(void*) == 4u,
                  "PileChainRepair must be built as 32-bit");
    if (module == nullptr || started_) return false;

    const auto modulePath = ModulePath(module);
    if (modulePath.empty()) return false;
    const auto dataDirectory = modulePath.parent_path() / L"PileChainRepair";
    stopPath_ = dataDirectory / L"PileChainRepair.stop";
    if (!logger_.Open(dataDirectory / L"PileChainRepair.log")) return false;
    logger_.Write(LogLevel::Info,
                  "PileChainRepair bootstrap version=0.3.0 mode=local-cut");

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
    logger_.Write(compatibility == CompatibilityResult::Compatible
                      ? LogLevel::Info
                      : LogLevel::Error,
                  std::string("executable compatibility=") +
                      CompatibilityName(compatibility));
    if (compatibility != CompatibilityResult::Compatible) {
        logger_.Close();
        return false;
    }

    tables_.entities = reinterpret_cast<void* const*>(
        executable->baseAddress + kEntityTableRva);
    tables_.entitySlots = kEntitySlots;
    tables_.ecoSectors = reinterpret_cast<void* const*>(
        executable->baseAddress + kEcoSectorTableRva);
    tables_.ecoSectorSlots = kEcoSectorSlots;
    visitMarks_.assign(tables_.entitySlots, 0u);
    tables_.visitMarks = visitMarks_.data();
    tables_.visitMarkCount = visitMarks_.size();
    tables_.visitGeneration = &visitGeneration_;
    tables_.accessProbe = &AccessibleRange;
    if (!ValidateTables()) {
        logger_.Write(LogLevel::Error,
                      "internal tables failed memory admission");
        logger_.Close();
        return false;
    }

    constexpr DWORD kWaitStepMs = 100u;
    constexpr DWORD kWaitLimitMs = 30'000u;
    DWORD waited = 0u;
    while (GetModuleHandleW(L"S4ModApi.dll") == nullptr &&
           waited < kWaitLimitMs) {
        Sleep(kWaitStepMs);
        waited += kWaitStepMs;
    }
    if (GetModuleHandleW(L"S4ModApi.dll") == nullptr) {
        logger_.Write(LogLevel::Error,
                      "S4ModApi.dll was not loaded within 30 seconds");
        logger_.Close();
        return false;
    }
    api_ = CreateSettlers4Api();
    if (api_ == nullptr) {
        logger_.Write(LogLevel::Error, "S4ApiCreate failed");
        logger_.Close();
        return false;
    }

    active_.store(this, std::memory_order_release);
    mapInitHook_ = api_->AddMapInitListener(&OnMapInit);
    tickHook_ = api_->AddTickListener(&OnTick);
    if (mapInitHook_ == 0u || tickHook_ == 0u) {
        logger_.Write(LogLevel::Error, "listener registration failed");
        Stop();
        return false;
    }
    started_ = true;
    logger_.Write(LogLevel::Info,
                  "runtime started; pile chains will be validated each tick");
    return true;
}

bool PileRepairRuntime::ValidateTables() const noexcept {
    return AccessibleRange(tables_.entities,
                           tables_.entitySlots * sizeof(void*), false) &&
           AccessibleRange(tables_.ecoSectors,
                           tables_.ecoSectorSlots * sizeof(void*), false);
}

bool PileRepairRuntime::RefreshEcoSectorList() noexcept {
    try {
        std::vector<std::uint16_t> candidate;
        candidate.push_back(0u);
        for (std::size_t sector = 1u; sector < tables_.ecoSectorSlots;
             ++sector) {
            const auto* manager = tables_.ecoSectors[sector];
            if (manager == nullptr) continue;
            if (!AccessibleRange(manager, kEcoSectorBytesRequired, true)) {
                return false;
            }
            candidate.push_back(static_cast<std::uint16_t>(sector));
        }
        activeEcoSectors_.swap(candidate);
        tables_.activeEcoSectors = activeEcoSectors_.data();
        tables_.activeEcoSectorCount = activeEcoSectors_.size();
        return true;
    } catch (...) {
        return false;
    }
}

bool PileRepairRuntime::Repair(const ChainAnalysis& analysis,
                               DWORD tick) noexcept {
    std::ostringstream detected;
    detected << "corruption detected tick=" << tick
             << " kind=" << ChainIssueName(analysis.firstIssue.kind)
             << " sector=" << analysis.firstIssue.ecoSector
             << " good=" << analysis.firstIssue.goodType
             << " entity=" << analysis.firstIssue.entity
             << " expected-prev=" << analysis.firstIssue.expectedPrevious
             << " actual-prev=" << analysis.firstIssue.actualPrevious;
    logger_.Write(LogLevel::Warning, detected.str());

    const bool cut = CutFatalPileChainIssue(tables_, analysis.firstIssue);
    logger_.Write(cut ? LogLevel::Info : LogLevel::Error,
                  std::string("local cut success=") +
                      (cut ? "true" : "false"));
    if (cut) ++repairs_;
    return cut;
}

void PileRepairRuntime::ObserveMapInit() noexcept {
    mapPending_.store(true, std::memory_order_release);
    logger_.Write(LogLevel::Info,
                  "map initialized; validation armed for first game tick");
}

void PileRepairRuntime::ObserveTick(DWORD tick) noexcept {
    bool expected = false;
    if (!inCallback_.compare_exchange_strong(expected, true,
                                              std::memory_order_acq_rel)) {
        return;
    }
    struct CallbackExit {
        std::atomic<bool>& flag;
        ~CallbackExit() { flag.store(false, std::memory_order_release); }
    } exit{inCallback_};

    if (!ValidateTables()) {
        if (mapPending_.load(std::memory_order_acquire)) {
            logger_.Write(LogLevel::Warning,
                          "validation postponed: map tables are not ready");
        }
        return;
    }
    if (mapPending_.exchange(false, std::memory_order_acq_rel) &&
        !RefreshEcoSectorList()) {
        mapPending_.store(true, std::memory_order_release);
        logger_.Write(LogLevel::Error,
                      "validation postponed: economy-sector list is inaccessible");
        return;
    }
    constexpr std::size_t kMaxCutsPerTick = 64u;
    for (std::size_t cut = 0u; cut < kMaxCutsPerTick; ++cut) {
        const auto analysis = AnalyzeFatalPileChainIssues(tables_);
        if (analysis.Clean()) return;
        if (!Repair(analysis, tick)) return;
    }
    logger_.Write(LogLevel::Error,
                  "local cut limit reached; remaining issues deferred");
}

HRESULT S4HCALL PileRepairRuntime::OnMapInit(LPVOID, LPVOID) {
    auto* runtime = active_.load(std::memory_order_acquire);
    if (runtime != nullptr) runtime->ObserveMapInit();
    return S_OK;
}

HRESULT S4HCALL PileRepairRuntime::OnTick(DWORD tick, BOOL, BOOL) {
    auto* runtime = active_.load(std::memory_order_acquire);
    if (runtime != nullptr) runtime->ObserveTick(tick);
    return S_OK;
}

void PileRepairRuntime::RequestStop() noexcept {
    stopRequested_.store(true, std::memory_order_release);
}

void PileRepairRuntime::RunControlLoop() {
    while (!stopRequested_.load(std::memory_order_acquire)) {
        std::error_code error;
        if (std::filesystem::exists(stopPath_, error) && !error) {
            std::filesystem::remove(stopPath_, error);
            RequestStop();
            break;
        }
        Sleep(100u);
    }
    Stop();
}

void PileRepairRuntime::Stop() noexcept {
    active_.store(nullptr, std::memory_order_release);
    while (inCallback_.load(std::memory_order_acquire)) Sleep(1u);
    if (api_ != nullptr) {
        if (tickHook_ != 0u) api_->RemoveListener(tickHook_);
        if (mapInitHook_ != 0u) api_->RemoveListener(mapInitHook_);
        tickHook_ = 0u;
        mapInitHook_ = 0u;
        api_->Release();
        api_ = nullptr;
    }
    if (started_) {
        logger_.Write(LogLevel::Info,
                      "runtime stopped repairs=" + std::to_string(repairs_));
    }
    started_ = false;
    logger_.Close();
}

DWORD WINAPI PileRepairBootstrapThread(void* module) {
    auto& runtime = PileRepairRuntimeInstance();
    if (runtime.Start(static_cast<HMODULE>(module))) {
        runtime.RunControlLoop();
    }
    return 0u;
}

}  // namespace pile_chain_repair
