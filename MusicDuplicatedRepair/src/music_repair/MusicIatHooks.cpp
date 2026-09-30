#include "music_repair/MusicIatHooks.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>

namespace music_loop_repair {
namespace {

std::atomic<AilOpenStream> gOpen{nullptr};
std::atomic<AilStreamCall> gStart{nullptr};
std::atomic<AilStreamCall> gClose{nullptr};
std::atomic<AilPauseStream> gPause{nullptr};
std::atomic<AilSetStreamPosition> gPosition{nullptr};
MusicHookCallbacks gCallbacks{};

void* __stdcall OpenHook(void* driver, const char* path,
                         std::int32_t streamMemory) {
    const auto original = gOpen.load(std::memory_order_acquire);
    void* const stream =
        original != nullptr ? original(driver, path, streamMemory) : nullptr;
    if (gCallbacks.open != nullptr) {
        gCallbacks.open(stream, path, streamMemory);
    }
    return stream;
}

void __stdcall StartHook(void* stream) {
    if (gCallbacks.start != nullptr) gCallbacks.start(stream);
    const auto original = gStart.load(std::memory_order_acquire);
    if (original != nullptr) original(stream);
}

void __stdcall CloseHook(void* stream) {
    if (gCallbacks.close != nullptr) gCallbacks.close(stream);
    const auto original = gClose.load(std::memory_order_acquire);
    if (original != nullptr) original(stream);
}

void __stdcall PauseHook(void* stream, std::int32_t paused) {
    if (gCallbacks.pause != nullptr) gCallbacks.pause(stream, paused);
    const auto original = gPause.load(std::memory_order_acquire);
    if (original != nullptr) original(stream, paused);
}

void __stdcall PositionHook(void* stream, std::int32_t offset) {
    if (gCallbacks.position != nullptr) gCallbacks.position(stream, offset);
    const auto original = gPosition.load(std::memory_order_acquire);
    if (original != nullptr) original(stream, offset);
}

bool EqualInsensitiveAscii(const char* left, const char* right) noexcept {
    if (left == nullptr || right == nullptr) return false;
    for (;;) {
        char a = *left++;
        char b = *right++;
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
        if (a != b) return false;
        if (a == '\0') return true;
    }
}

template <typename T>
T* ImagePointer(std::uintptr_t base, std::uint32_t imageSize,
                std::uint32_t rva, std::size_t count = 1u) noexcept {
    if (rva >= imageSize || count > (std::numeric_limits<std::size_t>::max)() /
                                      sizeof(T)) {
        return nullptr;
    }
    const auto bytes = count * sizeof(T);
    if (bytes > imageSize - rva) return nullptr;
    return reinterpret_cast<T*>(base + rva);
}

const char* ImageString(std::uintptr_t base, std::uint32_t imageSize,
                        std::uint32_t rva) noexcept {
    const auto* value = ImagePointer<char>(base, imageSize, rva);
    if (value == nullptr ||
        std::memchr(value, '\0', imageSize - rva) == nullptr) {
        return nullptr;
    }
    return value;
}

void PublishOriginal(const char* name, void* value) noexcept {
    if (std::strcmp(name, "_AIL_open_stream@12") == 0) {
        gOpen.store(reinterpret_cast<AilOpenStream>(value),
                    std::memory_order_release);
    } else if (std::strcmp(name, "_AIL_start_stream@4") == 0) {
        gStart.store(reinterpret_cast<AilStreamCall>(value),
                     std::memory_order_release);
    } else if (std::strcmp(name, "_AIL_close_stream@4") == 0) {
        gClose.store(reinterpret_cast<AilStreamCall>(value),
                     std::memory_order_release);
    } else if (std::strcmp(name, "_AIL_pause_stream@8") == 0) {
        gPause.store(reinterpret_cast<AilPauseStream>(value),
                     std::memory_order_release);
    } else if (std::strcmp(name, "_AIL_set_stream_position@8") == 0) {
        gPosition.store(reinterpret_cast<AilSetStreamPosition>(value),
                        std::memory_order_release);
    }
}

}  // namespace

bool MusicIatHooks::Start(HMODULE executable, std::uint32_t mappedSize,
                          MusicHookCallbacks callbacks) noexcept {
    if (executable == nullptr || mappedSize < sizeof(IMAGE_DOS_HEADER) ||
        installed_ != 0u) {
        failure_ = MusicHookFailure::InvalidModule;
        return false;
    }
    const auto base = reinterpret_cast<std::uintptr_t>(executable);
    const auto* dos =
        ImagePointer<IMAGE_DOS_HEADER>(base, mappedSize, 0u);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
        failure_ = MusicHookFailure::InvalidPeImage;
        return false;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
        base + static_cast<std::uint32_t>(dos->e_lfanew));
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        failure_ = MusicHookFailure::InvalidPeImage;
        return false;
    }
    const auto imageSize = nt->OptionalHeader.SizeOfImage;
    if (imageSize == 0u || imageSize > mappedSize) {
        failure_ = MusicHookFailure::InvalidPeImage;
        return false;
    }
    const auto& directory =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (nt->OptionalHeader.NumberOfRvaAndSizes <=
            IMAGE_DIRECTORY_ENTRY_IMPORT ||
        directory.VirtualAddress == 0u ||
        directory.Size < sizeof(IMAGE_IMPORT_DESCRIPTOR)) {
        failure_ = MusicHookFailure::ImportDirectoryMissing;
        return false;
    }

    bindings_ = {{
        {"_AIL_open_stream@12", reinterpret_cast<void*>(&OpenHook), nullptr,
         nullptr, true},
        {"_AIL_start_stream@4", reinterpret_cast<void*>(&StartHook), nullptr,
         nullptr, true},
        {"_AIL_close_stream@4", reinterpret_cast<void*>(&CloseHook), nullptr,
         nullptr, true},
        {"_AIL_pause_stream@8", reinterpret_cast<void*>(&PauseHook), nullptr,
         nullptr, false},
        {"_AIL_set_stream_position@8",
         reinterpret_cast<void*>(&PositionHook), nullptr, nullptr, false},
    }};

    const auto descriptorCount =
        directory.Size / sizeof(IMAGE_IMPORT_DESCRIPTOR);
    auto* imports = ImagePointer<IMAGE_IMPORT_DESCRIPTOR>(
        base, imageSize, directory.VirtualAddress, descriptorCount);
    if (imports == nullptr) {
        failure_ = MusicHookFailure::InvalidPeImage;
        return false;
    }
    bool milesFound = false;
    for (std::size_t descriptorIndex = 0u;
         descriptorIndex < descriptorCount; ++descriptorIndex) {
        auto* descriptor = imports + descriptorIndex;
        if (descriptor->Name == 0u) break;
        const auto* library =
            ImageString(base, imageSize, descriptor->Name);
        if (library == nullptr) {
            failure_ = MusicHookFailure::InvalidPeImage;
            return false;
        }
        if (!EqualInsensitiveAscii(library, "mss32.dll")) continue;
        milesFound = true;
        if (descriptor->OriginalFirstThunk == 0u ||
            descriptor->FirstThunk == 0u) {
            failure_ = MusicHookFailure::InvalidPeImage;
            return false;
        }
        auto* names = ImagePointer<IMAGE_THUNK_DATA32>(
            base, imageSize, descriptor->OriginalFirstThunk);
        auto* slots = ImagePointer<IMAGE_THUNK_DATA32>(
            base, imageSize, descriptor->FirstThunk);
        if (names == nullptr || slots == nullptr) {
            failure_ = MusicHookFailure::InvalidPeImage;
            return false;
        }
        const auto nameCount = (imageSize - descriptor->OriginalFirstThunk) /
                               sizeof(IMAGE_THUNK_DATA32);
        const auto slotCount = (imageSize - descriptor->FirstThunk) /
                               sizeof(IMAGE_THUNK_DATA32);
        const auto thunkCount = (std::min)(nameCount, slotCount);
        bool terminated = false;
        for (std::size_t index = 0u; index < thunkCount; ++index) {
            if (names[index].u1.AddressOfData == 0u) {
                terminated = true;
                break;
            }
            if (IMAGE_SNAP_BY_ORDINAL32(names[index].u1.Ordinal)) continue;
            const auto* import = ImagePointer<IMAGE_IMPORT_BY_NAME>(
                base, imageSize, names[index].u1.AddressOfData);
            if (import == nullptr) {
                failure_ = MusicHookFailure::InvalidPeImage;
                return false;
            }
            const auto nameRva = names[index].u1.AddressOfData +
                                 offsetof(IMAGE_IMPORT_BY_NAME, Name);
            const auto* name = ImageString(base, imageSize, nameRva);
            if (name == nullptr) {
                failure_ = MusicHookFailure::InvalidPeImage;
                return false;
            }
            for (auto& binding : bindings_) {
                if (std::strcmp(name, binding.name) == 0) {
                    if (binding.slot != nullptr) {
                        failure_ = MusicHookFailure::InvalidPeImage;
                        return false;
                    }
                    binding.slot =
                        reinterpret_cast<void**>(&slots[index].u1.Function);
                }
            }
        }
        if (!terminated) {
            failure_ = MusicHookFailure::InvalidPeImage;
            return false;
        }
        break;
    }
    if (!milesFound) {
        failure_ = MusicHookFailure::MilesImportMissing;
        return false;
    }
    for (const auto& binding : bindings_) {
        if (binding.required && binding.slot == nullptr) {
            failure_ = MusicHookFailure::RequiredFunctionMissing;
            return false;
        }
    }

    gCallbacks = callbacks;
    for (auto& binding : bindings_) {
        if (binding.slot != nullptr && !Patch(binding)) {
            Rollback();
            return false;
        }
    }
    failure_ = MusicHookFailure::None;
    return true;
}

bool MusicIatHooks::Patch(Binding& binding) noexcept {
    DWORD oldProtection = 0u;
    if (VirtualProtect(binding.slot, sizeof(void*), PAGE_READWRITE,
                       &oldProtection) == FALSE) {
        failure_ = MusicHookFailure::ProtectFailed;
        return false;
    }
    binding.originalProtection = oldProtection;
    auto* const slot = reinterpret_cast<void* volatile*>(binding.slot);
    binding.original = InterlockedCompareExchangePointer(slot, nullptr,
                                                         nullptr);
    bool patched = false;
    if (binding.original == nullptr ||
        binding.original == binding.replacement) {
        failure_ = MusicHookFailure::InvalidOriginal;
    } else {
        PublishOriginal(binding.name, binding.original);
        patched = InterlockedCompareExchangePointer(
                      slot, binding.replacement, binding.original) ==
                  binding.original;
        if (!patched) failure_ = MusicHookFailure::SlotChanged;
    }
    if (patched) {
        binding.patched = true;
        ++installed_;
    }
    DWORD ignored = 0u;
    if (VirtualProtect(binding.slot, sizeof(void*), oldProtection,
                       &ignored) == FALSE) {
        failure_ = MusicHookFailure::ProtectFailed;
        return false;
    }
    return patched;
}

bool MusicIatHooks::Restore(Binding& binding) noexcept {
    if (!binding.patched) return true;
    DWORD oldProtection = 0u;
    if (VirtualProtect(binding.slot, sizeof(void*), PAGE_READWRITE,
                       &oldProtection) == FALSE) {
        failure_ = MusicHookFailure::ProtectFailed;
        return false;
    }
    const bool restored = InterlockedCompareExchangePointer(
                              reinterpret_cast<void* volatile*>(binding.slot),
                              binding.original, binding.replacement) ==
                          binding.replacement;
    if (restored) {
        binding.patched = false;
        if (installed_ != 0u) --installed_;
    } else {
        failure_ = MusicHookFailure::SlotChanged;
    }
    DWORD ignored = 0u;
    const auto protection = restored ? binding.originalProtection
                                    : oldProtection;
    if (VirtualProtect(binding.slot, sizeof(void*), protection,
                       &ignored) == FALSE) {
        failure_ = MusicHookFailure::ProtectFailed;
        return false;
    }
    return restored;
}

void MusicIatHooks::Rollback() noexcept {
    for (auto it = bindings_.rbegin(); it != bindings_.rend(); ++it) {
        Restore(*it);
    }
}

bool MusicIatHooks::Stop() noexcept {
    bool success = true;
    for (auto it = bindings_.rbegin(); it != bindings_.rend(); ++it) {
        if (!Restore(*it)) success = false;
    }
    if (success) failure_ = MusicHookFailure::None;
    return success;
}

const char* MusicHookFailureName(MusicHookFailure failure) noexcept {
    switch (failure) {
        case MusicHookFailure::None: return "none";
        case MusicHookFailure::InvalidModule: return "invalid-module";
        case MusicHookFailure::InvalidPeImage: return "invalid-pe-image";
        case MusicHookFailure::ImportDirectoryMissing:
            return "import-directory-missing";
        case MusicHookFailure::MilesImportMissing: return "miles-import-missing";
        case MusicHookFailure::RequiredFunctionMissing:
            return "required-function-missing";
        case MusicHookFailure::InvalidOriginal: return "invalid-original";
        case MusicHookFailure::ProtectFailed: return "protect-failed";
        case MusicHookFailure::SlotChanged: return "slot-changed";
    }
    return "unknown";
}

}  // namespace music_loop_repair
