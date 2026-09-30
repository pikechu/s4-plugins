#pragma once

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace music_loop_repair {

using AilOpenStream = void*(__stdcall*)(void*, const char*, std::int32_t);
using AilStreamCall = void(__stdcall*)(void*);
using AilPauseStream = void(__stdcall*)(void*, std::int32_t);
using AilSetStreamPosition = void(__stdcall*)(void*, std::int32_t);

struct MusicHookCallbacks final {
    void (*open)(void* stream, const char* path, std::int32_t streamMemory)
        noexcept = nullptr;
    void (*start)(void* stream) noexcept = nullptr;
    void (*close)(void* stream) noexcept = nullptr;
    void (*pause)(void* stream, std::int32_t paused) noexcept = nullptr;
    void (*position)(void* stream, std::int32_t offset) noexcept = nullptr;
};

enum class MusicHookFailure {
    None,
    InvalidModule,
    InvalidPeImage,
    ImportDirectoryMissing,
    MilesImportMissing,
    RequiredFunctionMissing,
    InvalidOriginal,
    ProtectFailed,
    SlotChanged,
};

class MusicIatHooks final {
public:
    bool Start(HMODULE executable, std::uint32_t mappedSize,
               MusicHookCallbacks callbacks) noexcept;
    bool Stop() noexcept;
    MusicHookFailure failure() const noexcept { return failure_; }
    std::size_t installed() const noexcept { return installed_; }

private:
    struct Binding final {
        const char* name = nullptr;
        void* replacement = nullptr;
        void* original = nullptr;
        void** slot = nullptr;
        bool required = false;
        bool patched = false;
        DWORD originalProtection = 0u;
    };

    bool Patch(Binding& binding) noexcept;
    bool Restore(Binding& binding) noexcept;
    void Rollback() noexcept;

    std::array<Binding, 5> bindings_{};
    MusicHookFailure failure_ = MusicHookFailure::None;
    std::size_t installed_ = 0u;
};

const char* MusicHookFailureName(MusicHookFailure failure) noexcept;

}  // namespace music_loop_repair
