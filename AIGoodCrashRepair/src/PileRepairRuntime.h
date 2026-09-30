#pragma once

#include "S4ModApi.h"
#include "diagnostics/Logger.h"
#include "PileChainCore.h"

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <vector>

namespace pile_chain_repair {

class PileRepairRuntime final {
public:
    bool Start(HMODULE module);
    void RunControlLoop();
    void RequestStop() noexcept;

private:
    static HRESULT S4HCALL OnMapInit(LPVOID, LPVOID);
    static HRESULT S4HCALL OnTick(DWORD, BOOL, BOOL);

    void ObserveMapInit() noexcept;
    void ObserveTick(DWORD tick) noexcept;
    bool ValidateTables() const noexcept;
    bool Repair(const ChainAnalysis& analysis, DWORD tick) noexcept;
    void Stop() noexcept;

    static std::atomic<PileRepairRuntime*> active_;

    campaign_completion::Logger logger_;
    std::filesystem::path stopPath_;
    S4API api_ = nullptr;
    S4HOOK mapInitHook_ = 0u;
    S4HOOK tickHook_ = 0u;
    ChainTables tables_{};
    std::vector<std::uint32_t> visitMarks_;
    std::uint32_t visitGeneration_ = 0u;
    std::atomic<bool> stopRequested_{false};
    std::mutex callbackMutex_;
    std::atomic<bool> mapPending_{true};
    std::size_t repairs_ = 0u;
    bool started_ = false;
};

PileRepairRuntime& PileRepairRuntimeInstance();
DWORD WINAPI PileRepairBootstrapThread(void* module);

}  // namespace pile_chain_repair
