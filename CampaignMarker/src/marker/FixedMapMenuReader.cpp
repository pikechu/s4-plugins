#include "marker/FixedMapMenuReader.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

namespace campaign_completion {
namespace {

constexpr std::uintptr_t kScrollBaseRva = 0x00E938D8u;
constexpr std::uintptr_t kEntryArrayRva = 0x00E97848u;
constexpr std::uintptr_t kEntryCountRva = 0x0109C1D8u;
constexpr std::uintptr_t kArrayAliasRva = 0x0109C1E0u;
constexpr std::uintptr_t kRenderWindowRva = 0x0008D660u;
constexpr std::uintptr_t kScrollWindowRva = 0x0008E700u;
constexpr std::uintptr_t kConstructionWindowRva = 0x001202AEu;
constexpr std::uint32_t kMinimumApprovedImageSize = 0x012BF000u;
constexpr std::uint32_t kMaximumEntries = 1000u;
constexpr std::uintptr_t kRelativeIdentifierOffset = 0x18u;

#pragma pack(push, 1)
struct MsvcX86WideStringLayout final {
    union Storage {
        wchar_t inlineText[8];
        std::uint32_t pointer32;

        Storage() : inlineText{} {}
    } storage;
    std::uint32_t length = 0u;
    std::uint32_t capacity = 0u;
};
#pragma pack(pop)

static_assert(sizeof(wchar_t) == 2u);
static_assert(sizeof(MsvcX86WideStringLayout) == 24u);

bool IsReadableProtection(DWORD protection) noexcept {
    const DWORD basic = protection & 0xFFu;
    return basic == PAGE_READONLY || basic == PAGE_READWRITE ||
           basic == PAGE_WRITECOPY || basic == PAGE_EXECUTE_READ ||
           basic == PAGE_EXECUTE_READWRITE ||
           basic == PAGE_EXECUTE_WRITECOPY;
}

bool AddOverflows(std::uintptr_t address, std::size_t bytes) noexcept {
    if (bytes == 0u) return false;
    const auto maximum = (std::numeric_limits<std::uintptr_t>::max)();
    return address > maximum - (bytes - 1u);
}

bool IsReadableRange(const void* source, std::size_t bytes) noexcept {
    if (source == nullptr || bytes == 0u) return false;
    auto cursor = reinterpret_cast<std::uintptr_t>(source);
    if (AddOverflows(cursor, bytes)) return false;
    const auto last = cursor + bytes - 1u;
    for (;;) {
        MEMORY_BASIC_INFORMATION memory{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &memory,
                         sizeof(memory)) != sizeof(memory) ||
            memory.State != MEM_COMMIT || memory.RegionSize == 0u ||
            (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0u ||
            !IsReadableProtection(memory.Protect)) {
            return false;
        }
        const auto base = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
        if (cursor < base || AddOverflows(base, memory.RegionSize)) return false;
        const auto regionLast = base + memory.RegionSize - 1u;
        if (last <= regionLast) return true;
        if (regionLast ==
            (std::numeric_limits<std::uintptr_t>::max)()) return false;
        cursor = regionLast + 1u;
    }
}

bool CopyProtected(void* destination, const void* source,
                   std::size_t bytes) noexcept {
    if (!IsReadableRange(source, bytes)) return false;
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

template <typename T>
bool ReadScalar(const T* source, T& value) noexcept {
    return CopyProtected(&value, source, sizeof(value));
}

bool AddRva(std::uintptr_t base, std::uint32_t imageSize,
            std::uintptr_t rva, std::size_t bytes,
            const void*& output) noexcept {
    if (rva > imageSize || bytes > imageSize - rva ||
        AddOverflows(base, static_cast<std::size_t>(rva) + bytes)) {
        return false;
    }
    output = reinterpret_cast<const void*>(base + rva);
    return true;
}

bool MatchWindow(const void* address, const BYTE* expected,
                 const BYTE* mask, std::size_t size) noexcept {
    std::array<BYTE, 24u> actual{};
    if (size > actual.size() ||
        !CopyProtected(actual.data(), address, size)) return false;
    for (std::size_t index = 0u; index < size; ++index) {
        if (mask[index] != 0u && actual[index] != expected[index]) return false;
    }
    return true;
}

bool MatchRelocatedOperand(const void* window, std::size_t offset,
                           std::uint32_t expected) noexcept {
    const auto address = reinterpret_cast<const BYTE*>(window) + offset;
    std::uint32_t actual = 0u;
    return ReadScalar(reinterpret_cast<const std::uint32_t*>(address), actual) &&
           actual == expected;
}

bool IsControl(wchar_t unit) noexcept {
    const auto value = static_cast<unsigned>(unit);
    return value <= 0x1Fu || (value >= 0x7Fu && value <= 0x9Fu);
}

bool CaptureRelativeIdentifier(
    std::uint32_t entryAddress,
    BoundedRelativeIdentifier& output) noexcept {
    if (entryAddress == 0u ||
        entryAddress > (std::numeric_limits<std::uint32_t>::max)() -
                           kRelativeIdentifierOffset) {
        return false;
    }
    const auto* object = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(entryAddress) +
        kRelativeIdentifierOffset);
    MsvcX86WideStringLayout header{};
    if (!CopyProtected(&header, object, sizeof(header)) ||
        header.length == 0u ||
        header.length > kMaximumRelativeIdentityUnits ||
        header.length > header.capacity || header.capacity >= 0x00100000u) {
        return false;
    }

    std::uintptr_t storage = reinterpret_cast<std::uintptr_t>(object);
    if (header.capacity >= 8u) {
        if (header.storage.pointer32 == 0u) return false;
        storage = header.storage.pointer32;
    }
    const auto count = static_cast<std::size_t>(header.length) + 1u;
    BoundedRelativeIdentifier candidate{};
    if (!CopyProtected(candidate.units.data(),
                       reinterpret_cast<const void*>(storage),
                       count * sizeof(wchar_t)) ||
        candidate.units[header.length] != L'\0') {
        return false;
    }
    for (std::size_t index = 0u; index < header.length; ++index) {
        if (IsControl(candidate.units[index])) return false;
    }
    candidate.length = static_cast<std::uint16_t>(header.length);
    output = candidate;
    return true;
}

struct Header final {
    std::uint32_t alias = 0u;
    std::uint32_t count = 0u;
    std::uint32_t scroll = 0u;
};

bool ReadHeader(const FixedMapMenuMemoryView& memory,
                Header& output) noexcept {
    return ReadScalar(memory.arrayAlias, output.alias) &&
           ReadScalar(memory.entryCount, output.count) &&
           ReadScalar(memory.scrollBase, output.scroll);
}

void RejectSnapshot(FixedMapMenuSnapshot& snapshot,
                    FixedMapMenuReadStatus status) noexcept {
    snapshot = {};
    snapshot.status = status;
}

}  // namespace

FixedMapMenuMemoryView AdmitFixedMapMenuMemory(
    const ModuleInfo& executable) noexcept {
    FixedMapMenuMemoryView result{};
    if (CheckTargetExecutable(executable) !=
            CompatibilityResult::Compatible ||
        executable.baseAddress == 0u ||
        executable.size < kMinimumApprovedImageSize) return result;

    const void* scroll = nullptr;
    const void* entries = nullptr;
    const void* count = nullptr;
    const void* alias = nullptr;
    const void* renderWindow = nullptr;
    const void* scrollWindow = nullptr;
    const void* constructionWindow = nullptr;
    if (!AddRva(executable.baseAddress, executable.size, kScrollBaseRva,
                sizeof(std::uint32_t), scroll) ||
        !AddRva(executable.baseAddress, executable.size, kEntryArrayRva,
                kMaximumEntries * sizeof(std::uint32_t), entries) ||
        !AddRva(executable.baseAddress, executable.size, kEntryCountRva,
                sizeof(std::uint32_t), count) ||
        !AddRva(executable.baseAddress, executable.size, kArrayAliasRva,
                sizeof(std::uint32_t), alias) ||
        !AddRva(executable.baseAddress, executable.size, kRenderWindowRva,
                20u, renderWindow) ||
        !AddRva(executable.baseAddress, executable.size, kScrollWindowRva,
                22u, scrollWindow) ||
        !AddRva(executable.baseAddress, executable.size,
                kConstructionWindowRva, 18u, constructionWindow)) {
        return result;
    }

    constexpr BYTE renderExpected[20]{
        0x8B, 0x35, 0, 0, 0, 0, 0x03, 0xF1, 0x89, 0xB5,
        0xD0, 0xFE, 0xFF, 0xFF, 0x3B, 0x0D, 0, 0, 0, 0};
    constexpr BYTE renderMask[20]{
        1, 1, 0, 0, 0, 0, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 0, 0, 0, 0};
    constexpr BYTE scrollExpected[22]{
        0x8B, 0x0D, 0, 0, 0, 0, 0x8D, 0x41, 0x07, 0x3B, 0x05,
        0, 0, 0, 0, 0x0F, 0x8D, 0, 0, 0, 0, 0x41};
    constexpr BYTE scrollMask[22]{
        1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1};
    constexpr BYTE constructionExpected[18]{
        0xA1, 0, 0, 0, 0, 0x8D, 0x8D, 0xD0, 0xFD,
        0xFF, 0xFF, 0x89, 0x34, 0x85, 0, 0, 0, 0};
    constexpr BYTE constructionMask[18]{
        1, 0, 0, 0, 0, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 0, 0, 0, 0};
    if (!MatchWindow(renderWindow, renderExpected, renderMask, 20u) ||
        !MatchWindow(scrollWindow, scrollExpected, scrollMask, 22u) ||
        !MatchWindow(constructionWindow, constructionExpected,
                     constructionMask, 18u)) {
        return result;
    }

    const auto expected = executable.baseAddress + kEntryArrayRva;
    const auto expectedScroll = executable.baseAddress + kScrollBaseRva;
    const auto expectedCount = executable.baseAddress + kEntryCountRva;
    if (expected > (std::numeric_limits<std::uint32_t>::max)() ||
        expectedScroll > (std::numeric_limits<std::uint32_t>::max)() ||
        expectedCount > (std::numeric_limits<std::uint32_t>::max)()) {
        return result;
    }
    if (!MatchRelocatedOperand(renderWindow, 2u,
                               static_cast<std::uint32_t>(expectedScroll)) ||
        !MatchRelocatedOperand(renderWindow, 16u,
                               static_cast<std::uint32_t>(expectedCount)) ||
        !MatchRelocatedOperand(scrollWindow, 2u,
                               static_cast<std::uint32_t>(expectedScroll)) ||
        !MatchRelocatedOperand(scrollWindow, 11u,
                               static_cast<std::uint32_t>(expectedCount)) ||
        !MatchRelocatedOperand(constructionWindow, 1u,
                               static_cast<std::uint32_t>(expectedCount)) ||
        !MatchRelocatedOperand(constructionWindow, 14u,
                               static_cast<std::uint32_t>(expected))) {
        return result;
    }
    result.scrollBase = static_cast<const std::uint32_t*>(scroll);
    result.entryCount = static_cast<const std::uint32_t*>(count);
    result.arrayAlias = static_cast<const std::uint32_t*>(alias);
    result.entries = static_cast<const std::uint32_t*>(entries);
    result.expectedArrayAddress = static_cast<std::uint32_t>(expected);
    result.admitted = true;
    return result;
}

FixedMapMenuSnapshot ReadFixedMapMenuSnapshot(
    const FixedMapMenuMemoryView& memory) noexcept {
    FixedMapMenuSnapshot result{};
    if (!memory.admitted || memory.scrollBase == nullptr ||
        memory.entryCount == nullptr || memory.arrayAlias == nullptr ||
        memory.entries == nullptr || memory.expectedArrayAddress == 0u) {
        return result;
    }

    Header before{};
    if (!ReadHeader(memory, before)) {
        RejectSnapshot(result, FixedMapMenuReadStatus::HeaderUnreadable);
        return result;
    }
    if (before.alias != memory.expectedArrayAddress) {
        RejectSnapshot(result, FixedMapMenuReadStatus::AliasMismatch);
        return result;
    }
    if (before.count > kMaximumEntries) {
        RejectSnapshot(result, FixedMapMenuReadStatus::CountOutOfRange);
        return result;
    }
    if (before.scroll > before.count ||
        (before.count == 0u && before.scroll != 0u)) {
        RejectSnapshot(result, FixedMapMenuReadStatus::ScrollOutOfRange);
        return result;
    }

    result.entryCount = before.count;
    result.scrollBase = before.scroll;
    const auto remaining = before.count - before.scroll;
    result.rowCount = (std::min)(
        static_cast<std::size_t>(remaining), kMaximumInternalVisibleRows);
    for (std::size_t slot = 0u; slot < result.rowCount; ++slot) {
        const auto index = static_cast<std::size_t>(before.scroll) + slot;
        std::uint32_t entry = 0u;
        if (!ReadScalar(memory.entries + index, entry)) {
            RejectSnapshot(result, FixedMapMenuReadStatus::EntryUnreadable);
            return result;
        }
        if (!CaptureRelativeIdentifier(
                entry, result.relativeIdentifiers[slot])) {
            RejectSnapshot(
                result,
                FixedMapMenuReadStatus::RelativeIdentifierInvalid);
            return result;
        }
    }

    Header after{};
    if (!ReadHeader(memory, after) ||
        before.alias != after.alias || before.count != after.count ||
        before.scroll != after.scroll) {
        RejectSnapshot(result, FixedMapMenuReadStatus::ConcurrentMutation);
        return result;
    }
    result.status = FixedMapMenuReadStatus::Success;
    return result;
}

bool EqualFixedMapMenuSnapshot(const FixedMapMenuSnapshot& left,
                               const FixedMapMenuSnapshot& right) noexcept {
    if (left.status != right.status || left.entryCount != right.entryCount ||
        left.scrollBase != right.scrollBase ||
        left.rowCount != right.rowCount) return false;
    for (std::size_t slot = 0u; slot < left.rowCount; ++slot) {
        if (left.relativeIdentifiers[slot].view() !=
            right.relativeIdentifiers[slot].view()) return false;
    }
    return true;
}

}  // namespace campaign_completion
