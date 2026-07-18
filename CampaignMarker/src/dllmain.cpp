#include "runtime/DiagnosticRuntime.h"

#include <windows.h>

extern "C" __declspec(dllexport) void CampaignCompletionDebugStop() {
    campaign_completion::RuntimeInstance().RequestControlledStop();
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        const HANDLE thread = CreateThread(nullptr, 0,
                                           &campaign_completion::BootstrapThread,
                                           module, 0, nullptr);
        if (thread != nullptr) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
