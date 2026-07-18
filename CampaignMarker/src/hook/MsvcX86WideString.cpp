#include "hook/MsvcX86WideString.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>

namespace campaign_completion {
namespace {

#pragma pack(push, 1)
struct MsvcX86WideStringLayout final {
    union Storage {
        wchar_t inlineText[8];
        std::uint32_t pointer32;

        Storage() : inlineText{} {}
    } storage;
    std::uint32_t length = 0;
    std::uint32_t capacity = 0;
};
#pragma pack(pop)

static_assert(sizeof(wchar_t) == 2);
static_assert(sizeof(MsvcX86WideStringLayout) == 24);

CapturedWidePath Failed(WideCaptureFailure failure) noexcept {
    CapturedWidePath result{};
    result.failure = failure;
    return result;
}

bool IsReadableProtection(DWORD protection) noexcept {
    const DWORD basicProtection = protection & 0xFFu;
    return basicProtection == PAGE_READONLY ||
           basicProtection == PAGE_READWRITE ||
           basicProtection == PAGE_WRITECOPY ||
           basicProtection == PAGE_EXECUTE_READ ||
           basicProtection == PAGE_EXECUTE_READWRITE ||
           basicProtection == PAGE_EXECUTE_WRITECOPY;
}

bool RangeOverflows(std::uintptr_t address, std::size_t bytes) noexcept {
    if (bytes == 0u) {
        return false;
    }
    const auto maximum = (std::numeric_limits<std::uintptr_t>::max)();
    return bytes - 1u > maximum || address > maximum - (bytes - 1u);
}

bool IsReadableRange(std::uintptr_t address, std::size_t bytes) noexcept {
    if (bytes == 0u || RangeOverflows(address, bytes)) {
        return false;
    }

    const auto last = address + bytes - 1u;
    auto cursor = address;
    for (;;) {
        MEMORY_BASIC_INFORMATION memory{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &memory,
                         sizeof(memory)) != sizeof(memory) ||
            memory.State != MEM_COMMIT ||
            (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0u ||
            !IsReadableProtection(memory.Protect) || memory.RegionSize == 0u) {
            return false;
        }

        const auto regionBase =
            reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
        if (cursor < regionBase || RangeOverflows(regionBase, memory.RegionSize)) {
            return false;
        }
        const auto regionLast = regionBase + memory.RegionSize - 1u;
        if (cursor > regionLast) {
            return false;
        }
        if (last <= regionLast) {
            return true;
        }
        if (regionLast == (std::numeric_limits<std::uintptr_t>::max)()) {
            return false;
        }
        cursor = regionLast + 1u;
    }
}

bool CopyProtected(void* destination, const void* source,
                   std::size_t bytes) noexcept {
#if defined(_MSC_VER)
    __try {
        std::memcpy(destination, source, bytes);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    std::memcpy(destination, source, bytes);
    return true;
#endif
}

}  // namespace

CapturedWidePath CaptureMsvcX86WideString(const void* object,
                                         std::size_t maxChars) noexcept {
    if (object == nullptr) {
        return Failed(WideCaptureFailure::NullObject);
    }

    const auto objectAddress = reinterpret_cast<std::uintptr_t>(object);
    if (RangeOverflows(objectAddress, sizeof(MsvcX86WideStringLayout))) {
        return Failed(WideCaptureFailure::AddressOverflow);
    }
    if (!IsReadableRange(objectAddress, sizeof(MsvcX86WideStringLayout))) {
        return Failed(WideCaptureFailure::HeaderUnreadable);
    }

    MsvcX86WideStringLayout header{};
    if (!CopyProtected(&header, object, sizeof(header))) {
        return Failed(WideCaptureFailure::HeaderUnreadable);
    }
    if (header.length > header.capacity) {
        return Failed(WideCaptureFailure::LengthExceedsCapacity);
    }
    if (static_cast<std::size_t>(header.length) >= maxChars) {
        return Failed(WideCaptureFailure::LengthLimitExceeded);
    }
    if (header.capacity >= 0x00100000u) {
        return Failed(WideCaptureFailure::CapacityLimitExceeded);
    }

    std::uintptr_t storageAddress = objectAddress;
    if (header.capacity >= 8u) {
        if (header.storage.pointer32 == 0u) {
            return Failed(WideCaptureFailure::NullStorage);
        }
        storageAddress = header.storage.pointer32;
    }

    const auto characterCount = static_cast<std::size_t>(header.length) + 1u;
    const auto byteCount = characterCount * sizeof(wchar_t);
    if (RangeOverflows(storageAddress, byteCount)) {
        return Failed(WideCaptureFailure::AddressOverflow);
    }
    if (!IsReadableRange(storageAddress, byteCount)) {
        return Failed(WideCaptureFailure::StorageUnreadable);
    }

    std::wstring owned;
    try {
        owned.resize(characterCount);
    } catch (...) {
        return Failed(WideCaptureFailure::AllocationFailure);
    }
    if (!CopyProtected(owned.data(),
                       reinterpret_cast<const void*>(storageAddress),
                       byteCount)) {
        return Failed(WideCaptureFailure::StorageUnreadable);
    }
    if (owned[header.length] != L'\0') {
        return Failed(WideCaptureFailure::MissingTerminator);
    }
    owned.resize(header.length);
    return {std::move(owned), WideCaptureFailure::None};
}

}  // namespace campaign_completion
