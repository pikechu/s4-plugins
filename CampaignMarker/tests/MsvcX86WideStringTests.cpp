#include "hook/MsvcX86WideString.h"

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

#pragma pack(push, 1)
struct TestWideString32 final {
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
static_assert(sizeof(TestWideString32) == 24);

TestWideString32 InlineValue(const wchar_t* value) {
    TestWideString32 object{};
    const auto length = std::wcslen(value);
    Require(length < 8u, "inline fixture must fit SSO storage");
    std::memcpy(object.storage.inlineText, value,
                (length + 1u) * sizeof(wchar_t));
    object.length = static_cast<std::uint32_t>(length);
    object.capacity = 7u;
    return object;
}

TestWideString32 HeapValue(wchar_t* value, std::uint32_t length,
                           std::uint32_t capacity) {
    TestWideString32 object{};
    const auto address = reinterpret_cast<std::uintptr_t>(value);
    Require(address <= 0xFFFFFFFFu, "heap fixture must have a 32-bit address");
    object.storage.pointer32 = static_cast<std::uint32_t>(address);
    object.length = length;
    object.capacity = capacity;
    return object;
}

}  // namespace

int RunMsvcX86WideStringTests() {
    using campaign_completion::CaptureMsvcX86WideString;
    using campaign_completion::WideCaptureFailure;

    auto sso = InlineValue(L"Tiny");
    const auto capturedSso = CaptureMsvcX86WideString(&sso, 512u);
    Require(capturedSso.failure == WideCaptureFailure::None,
            "valid SSO value captured");
    Require(capturedSso.value == L"Tiny", "SSO copy is owned and exact");
    sso.storage.inlineText[0] = L'X';
    Require(capturedSso.value == L"Tiny", "capture does not retain SSO pointer");

    constexpr wchar_t stablePath[] = L"Map\\User\\Stable.map";
    constexpr std::size_t stableLength = std::size(stablePath) - 1u;
    auto* heapText = static_cast<wchar_t*>(
        VirtualAlloc(nullptr, sizeof(stablePath), MEM_RESERVE | MEM_COMMIT,
                     PAGE_READWRITE));
    Require(heapText != nullptr, "heap fixture allocation succeeds");
    std::memcpy(heapText, stablePath, sizeof(stablePath));
    auto heap = HeapValue(heapText, static_cast<std::uint32_t>(stableLength),
                          static_cast<std::uint32_t>(stableLength));
    const auto capturedHeap = CaptureMsvcX86WideString(&heap, 512u);
    Require(capturedHeap.failure == WideCaptureFailure::None,
            "valid heap value captured");
    Require(capturedHeap.value == stablePath, "heap copy is exact");
    heapText[0] = L'X';
    Require(capturedHeap.value == stablePath,
            "capture does not retain heap pointer");
    VirtualFree(heapText, 0u, MEM_RELEASE);

    auto badLength = InlineValue(L"Tiny");
    badLength.length = 7u;
    badLength.capacity = 3u;
    Require(CaptureMsvcX86WideString(&badLength, 512u).failure ==
                WideCaptureFailure::LengthExceedsCapacity,
            "length greater than capacity rejected");

    auto atLimit = InlineValue(L"Tiny");
    atLimit.length = 512u;
    atLimit.capacity = 512u;
    Require(CaptureMsvcX86WideString(&atLimit, 512u).failure ==
                WideCaptureFailure::LengthLimitExceeded,
            "length at the caller limit rejected");

    auto excessiveCapacity = InlineValue(L"");
    excessiveCapacity.capacity = 0x00100000u;
    Require(CaptureMsvcX86WideString(&excessiveCapacity, 512u).failure ==
                WideCaptureFailure::CapacityLimitExceeded,
            "unreviewed capacity rejected");

    Require(CaptureMsvcX86WideString(nullptr, 512u).failure ==
                WideCaptureFailure::NullObject,
            "null object rejected");

    auto* noAccessObject = static_cast<TestWideString32*>(VirtualAlloc(
        nullptr, sizeof(TestWideString32), MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE));
    Require(noAccessObject != nullptr, "header fixture allocation succeeds");
    *noAccessObject = InlineValue(L"Tiny");
    DWORD oldProtection = 0;
    Require(VirtualProtect(noAccessObject, sizeof(TestWideString32),
                           PAGE_NOACCESS, &oldProtection) != FALSE,
            "header fixture protection changes");
    Require(CaptureMsvcX86WideString(noAccessObject, 512u).failure ==
                WideCaptureFailure::HeaderUnreadable,
            "no-access object rejected");
    VirtualFree(noAccessObject, 0u, MEM_RELEASE);

    auto* noAccessText = static_cast<wchar_t*>(VirtualAlloc(
        nullptr, 32u, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
    Require(noAccessText != nullptr, "storage fixture allocation succeeds");
    auto noAccessHeap = HeapValue(noAccessText, 3u, 8u);
    Require(CaptureMsvcX86WideString(&noAccessHeap, 512u).failure ==
                WideCaptureFailure::StorageUnreadable,
            "no-access heap target rejected");
    VirtualFree(noAccessText, 0u, MEM_RELEASE);

    auto nullHeap = InlineValue(L"");
    nullHeap.capacity = 8u;
    nullHeap.storage.pointer32 = 0u;
    Require(CaptureMsvcX86WideString(&nullHeap, 512u).failure ==
                WideCaptureFailure::NullStorage,
            "null heap storage rejected");

    wchar_t unterminatedText[8]{L'M', L'a', L'p', L'X'};
    auto unterminated = HeapValue(unterminatedText, 3u, 8u);
    Require(CaptureMsvcX86WideString(&unterminated, 512u).failure ==
                WideCaptureFailure::MissingTerminator,
            "missing terminator rejected");

    auto overflowing = InlineValue(L"");
    overflowing.storage.pointer32 = 0xFFFFFFF8u;
    overflowing.length = 7u;
    overflowing.capacity = 8u;
    Require(CaptureMsvcX86WideString(&overflowing, 512u).failure ==
                WideCaptureFailure::AddressOverflow,
            "storage range overflow rejected");

    return 0;
}
