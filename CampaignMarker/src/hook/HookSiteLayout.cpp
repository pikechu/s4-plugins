#include "hook/HookSiteLayout.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>
#include <limits>

namespace campaign_completion {
namespace {

HookSiteAdmission Failed(HookSiteFailure failure) noexcept {
    HookSiteAdmission result{};
    result.failure = failure;
    return result;
}

HookSiteFailure CheckIdentity(const ModuleInfo& executable) noexcept {
    switch (CheckTargetExecutable(executable)) {
        case CompatibilityResult::Compatible:
            return HookSiteFailure::None;
        case CompatibilityResult::VersionMismatch:
            return HookSiteFailure::VersionMismatch;
        case CompatibilityResult::HashMismatch:
            return HookSiteFailure::HashMismatch;
    }
    return HookSiteFailure::HashMismatch;
}

bool CheckedAdd(std::uintptr_t base, std::uint32_t offset,
                std::uintptr_t& result) noexcept {
    const auto maximum = (std::numeric_limits<std::uintptr_t>::max)();
    if (base > maximum - offset) {
        return false;
    }
    result = base + offset;
    return true;
}

HookSiteAdmission Admit(const ModuleInfo& executable,
                        const HookSectionSpan& textSection,
                        const FixedMapHookSiteSpec& spec,
                        const std::array<std::uint8_t, 5>& liveBytes,
                        bool checkIdentity) noexcept {
    if (checkIdentity) {
        const auto identityFailure = CheckIdentity(executable);
        if (identityFailure != HookSiteFailure::None) {
            return Failed(identityFailure);
        }
    }

    constexpr std::uint32_t callSize = 5u;
    if (spec.callSiteRva > executable.size ||
        callSize > executable.size - spec.callSiteRva ||
        spec.originalTargetRva >= executable.size) {
        return Failed(HookSiteFailure::OutsideModule);
    }

    if (!textSection.executable ||
        spec.callSiteRva < textSection.virtualAddress) {
        return Failed(HookSiteFailure::WrongSection);
    }
    const auto sectionOffset = spec.callSiteRva - textSection.virtualAddress;
    if (sectionOffset > textSection.virtualSize ||
        callSize > textSection.virtualSize - sectionOffset) {
        return Failed(HookSiteFailure::WrongSection);
    }

    std::uintptr_t callSite = 0;
    std::uintptr_t originalTarget = 0;
    if (!CheckedAdd(executable.baseAddress, spec.callSiteRva, callSite) ||
        !CheckedAdd(executable.baseAddress, spec.originalTargetRva,
                    originalTarget)) {
        return Failed(HookSiteFailure::AddressOverflow);
    }

    if (liveBytes != spec.expectedCallBytes) {
        return Failed(HookSiteFailure::BytesMismatch);
    }
    if (liveBytes[0] != 0xE8u) {
        return Failed(HookSiteFailure::TargetMismatch);
    }

    const std::uint32_t relativeBits =
        static_cast<std::uint32_t>(liveBytes[1]) |
        (static_cast<std::uint32_t>(liveBytes[2]) << 8u) |
        (static_cast<std::uint32_t>(liveBytes[3]) << 16u) |
        (static_cast<std::uint32_t>(liveBytes[4]) << 24u);
    const std::int64_t relative =
        relativeBits <= 0x7FFFFFFFu
            ? static_cast<std::int64_t>(relativeBits)
            : static_cast<std::int64_t>(relativeBits) - 0x100000000LL;
    const std::int64_t decoded = static_cast<std::int64_t>(callSite) +
                                 static_cast<std::int64_t>(callSize) + relative;
    if (decoded < 0 ||
        static_cast<std::uint64_t>(decoded) >
            (std::numeric_limits<std::uintptr_t>::max)() ||
        static_cast<std::uintptr_t>(decoded) != originalTarget) {
        return Failed(HookSiteFailure::TargetMismatch);
    }

    return {callSite, originalTarget, HookSiteFailure::None};
}

bool ReadExact(HANDLE file, std::uint64_t offset, void* destination,
               DWORD size) noexcept {
    LARGE_INTEGER position{};
    position.QuadPart = static_cast<LONGLONG>(offset);
    if (SetFilePointerEx(file, position, nullptr, FILE_BEGIN) == FALSE) {
        return false;
    }
    DWORD bytesRead = 0;
    return ReadFile(file, destination, size, &bytesRead, nullptr) != FALSE &&
           bytesRead == size;
}

HookSiteFailure ReadTextSection(const ModuleInfo& executable,
                                HookSectionSpan& result) noexcept {
    const HANDLE file = CreateFileW(
        executable.path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return HookSiteFailure::FileUnavailable;
    }

    IMAGE_DOS_HEADER dos{};
    bool valid = ReadExact(file, 0u, &dos, static_cast<DWORD>(sizeof(dos))) &&
                 dos.e_magic == IMAGE_DOS_SIGNATURE && dos.e_lfanew > 0;

    DWORD signature = 0;
    IMAGE_FILE_HEADER fileHeader{};
    IMAGE_OPTIONAL_HEADER32 optionalHeader{};
    const auto ntOffset = static_cast<std::uint64_t>(dos.e_lfanew);
    if (valid) {
        valid = ReadExact(file, ntOffset, &signature,
                          static_cast<DWORD>(sizeof(signature))) &&
                signature == IMAGE_NT_SIGNATURE &&
                ReadExact(file, ntOffset + sizeof(signature), &fileHeader,
                          static_cast<DWORD>(sizeof(fileHeader))) &&
                fileHeader.Machine == IMAGE_FILE_MACHINE_I386 &&
                fileHeader.NumberOfSections > 0u &&
                fileHeader.NumberOfSections <= 96u &&
                static_cast<std::size_t>(fileHeader.SizeOfOptionalHeader) >=
                    sizeof(optionalHeader) &&
                ReadExact(file,
                          ntOffset + sizeof(signature) + sizeof(fileHeader),
                          &optionalHeader,
                          static_cast<DWORD>(sizeof(optionalHeader))) &&
                optionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    }

    bool found = false;
    if (valid) {
        const std::uint64_t sectionOffset =
            ntOffset + sizeof(signature) + sizeof(fileHeader) +
            fileHeader.SizeOfOptionalHeader;
        constexpr std::array<BYTE, IMAGE_SIZEOF_SHORT_NAME> textName{
            '.', 't', 'e', 'x', 't', 0, 0, 0};
        for (WORD index = 0; index < fileHeader.NumberOfSections; ++index) {
            IMAGE_SECTION_HEADER section{};
            const auto offset = sectionOffset +
                                static_cast<std::uint64_t>(index) *
                                    sizeof(IMAGE_SECTION_HEADER);
            if (!ReadExact(file, offset, &section,
                           static_cast<DWORD>(sizeof(section)))) {
                valid = false;
                break;
            }
            if (std::equal(textName.begin(), textName.end(),
                           std::begin(section.Name))) {
                result.virtualAddress = section.VirtualAddress;
                result.virtualSize = section.Misc.VirtualSize;
                result.executable =
                    (section.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0u &&
                    (section.Characteristics & IMAGE_SCN_CNT_CODE) != 0u;
                found = true;
                break;
            }
        }
    }

    CloseHandle(file);
    if (!valid) {
        return HookSiteFailure::InvalidPeImage;
    }
    if (!found) {
        return HookSiteFailure::TextSectionMissing;
    }
    return HookSiteFailure::None;
}

bool IsExecutableProtection(DWORD protection) noexcept {
    const DWORD basicProtection = protection & 0xFFu;
    return basicProtection == PAGE_EXECUTE ||
           basicProtection == PAGE_EXECUTE_READ ||
           basicProtection == PAGE_EXECUTE_READWRITE ||
           basicProtection == PAGE_EXECUTE_WRITECOPY;
}

bool CanReadLiveCall(std::uintptr_t address) noexcept {
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(reinterpret_cast<const void*>(address), &memory,
                     sizeof(memory)) != sizeof(memory) ||
        memory.State != MEM_COMMIT ||
        (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0u ||
        !IsExecutableProtection(memory.Protect)) {
        return false;
    }

    const auto regionBase = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
    if (address < regionBase) {
        return false;
    }
    const auto offset = address - regionBase;
    return offset <= memory.RegionSize && 5u <= memory.RegionSize - offset;
}

}  // namespace

HookSiteAdmission HookSiteLayout::Create(
    const ModuleInfo& executable) noexcept {
    const auto identityFailure = CheckIdentity(executable);
    if (identityFailure != HookSiteFailure::None) {
        return Failed(identityFailure);
    }

    HookSectionSpan textSection{};
    const auto sectionFailure = ReadTextSection(executable, textSection);
    if (sectionFailure != HookSiteFailure::None) {
        return Failed(sectionFailure);
    }

    const auto metadata =
        Admit(executable, textSection, kApprovedFixedMapHookSite,
              kApprovedFixedMapHookSite.expectedCallBytes, false);
    if (!metadata) {
        return metadata;
    }
    if (!CanReadLiveCall(metadata.callSite)) {
        return Failed(HookSiteFailure::MemoryUnavailable);
    }

    std::array<std::uint8_t, 5> liveBytes{};
#if defined(_MSC_VER)
    __try {
        std::memcpy(liveBytes.data(),
                    reinterpret_cast<const void*>(metadata.callSite),
                    liveBytes.size());
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return Failed(HookSiteFailure::MemoryUnavailable);
    }
#else
    std::memcpy(liveBytes.data(),
                reinterpret_cast<const void*>(metadata.callSite),
                liveBytes.size());
#endif

    return Admit(executable, textSection, kApprovedFixedMapHookSite, liveBytes,
                 false);
}

HookSiteAdmission HookSiteLayout::FromEvidenceForTesting(
    const ModuleInfo& executable, const HookSectionSpan& textSection,
    const FixedMapHookSiteSpec& spec,
    const std::array<std::uint8_t, 5>& liveBytes) noexcept {
    return Admit(executable, textSection, spec, liveBytes, true);
}

}  // namespace campaign_completion
