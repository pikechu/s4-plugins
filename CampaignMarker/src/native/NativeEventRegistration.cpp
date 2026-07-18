#include "native/NativeEventRegistration.h"

#include <windows.h>

namespace campaign_completion {
namespace {

#if defined(_MSC_VER)
using NativeEventOperation = bool(__thiscall*)(void*, void*);
#else
using NativeEventOperation = bool (*)(void*, void*);
#endif

}  // namespace

NativeEventRegistration::NativeEventRegistration(
    NativeEventAdmission admission) noexcept
    : admission_(admission) {}

bool NativeEventRegistration::Register(void* handle) noexcept {
    return Invoke(admission_.registerHandle, handle);
}

bool NativeEventRegistration::Unregister(void* handle) noexcept {
    return Invoke(admission_.unregisterHandle, handle);
}

bool NativeEventRegistration::Invoke(std::uintptr_t operation,
                                     void* handle) noexcept {
    if (!admission_ || operation == 0u || handle == nullptr) {
        return false;
    }

#if defined(_MSC_VER)
    __try {
        void* const engine =
            *reinterpret_cast<void* const*>(admission_.engineSlot);
        if (engine == nullptr) {
            return false;
        }
        const auto nativeOperation =
            reinterpret_cast<NativeEventOperation>(operation);
        return nativeOperation(engine, handle);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    void* const engine =
        *reinterpret_cast<void* const*>(admission_.engineSlot);
    if (engine == nullptr) {
        return false;
    }
    const auto nativeOperation =
        reinterpret_cast<NativeEventOperation>(operation);
    return nativeOperation(engine, handle);
#endif
}

}  // namespace campaign_completion
