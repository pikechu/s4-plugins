#include "PileRepairRuntime.h"

#include <windows.h>

extern "C" __declspec(dllexport) void PileChainRepairStop() {
    pile_chain_repair::PileRepairRuntimeInstance().RequestStop();
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        const HANDLE thread = CreateThread(
            nullptr, 0, &pile_chain_repair::PileRepairBootstrapThread, module,
            0, nullptr);
        if (thread != nullptr) CloseHandle(thread);
    }
    return TRUE;
}
