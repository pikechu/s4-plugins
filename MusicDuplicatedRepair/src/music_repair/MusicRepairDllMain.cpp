#include "music_repair/MusicRepairRuntime.h"

#include <windows.h>

extern "C" __declspec(dllexport) void MusicLoopRepairStop() {
    try {
        music_loop_repair::MusicRepairRuntimeInstance().RequestStop();
    } catch (...) {
        // Do not propagate bootstrap allocation failure through the export.
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        const HANDLE thread = CreateThread(
            nullptr, 0, &music_loop_repair::MusicRepairBootstrapThread, module,
            0, nullptr);
        if (thread != nullptr) CloseHandle(thread);
    }
    return TRUE;
}
