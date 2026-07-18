#include "native/NativeEventAdmission.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>
#include <limits>

namespace campaign_completion {
namespace {

constexpr std::size_t kRegisterOperandOffset = 46u;
constexpr std::size_t kUnregisterOperandOffset = 5u;

NativeEventAdmission Failed(NativeEventAdmissionFailure failure) noexcept {
    NativeEventAdmission result{};
    result.failure = failure;
    return result;
}

NativeEventAdmissionFailure CheckIdentity(
    const ModuleInfo& executable) noexcept {
    switch (CheckTargetExecutable(executable)) {
        case CompatibilityResult::Compatible:
            return NativeEventAdmissionFailure::None;
        case CompatibilityResult::VersionMismatch:
            return NativeEventAdmissionFailure::VersionMismatch;
        case CompatibilityResult::HashMismatch:
            return NativeEventAdmissionFailure::HashMismatch;
    }
    return NativeEventAdmissionFailure::HashMismatch;
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

bool Contains(const NativeEventSectionSpan& section, std::uint32_t rva,
              std::size_t size) noexcept {
    if (!section.executable || rva < section.virtualAddress) {
        return false;
    }
    const auto offset = rva - section.virtualAddress;
    return offset <= section.virtualSize &&
           size <= static_cast<std::size_t>(section.virtualSize - offset);
}

bool FitsModule(const ModuleInfo& executable, std::uint32_t rva,
                std::size_t size) noexcept {
    return rva <= executable.size &&
           size <= static_cast<std::size_t>(executable.size - rva);
}

std::uint32_t DecodeU32(const std::uint8_t* bytes) noexcept {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8u) |
           (static_cast<std::uint32_t>(bytes[2]) << 16u) |
           (static_cast<std::uint32_t>(bytes[3]) << 24u);
}

NativeEventAdmission Admit(const ModuleInfo& executable,
                           const NativeEventLayoutSpec& spec,
                           const NativeEventAdmissionEvidence& evidence,
                           bool checkIdentity) noexcept {
    if (checkIdentity) {
        const auto identityFailure = CheckIdentity(executable);
        if (identityFailure != NativeEventAdmissionFailure::None) {
            return Failed(identityFailure);
        }
    }

    if (!FitsModule(executable, spec.registerHandleRva,
                    evidence.registerBytes.size()) ||
        !FitsModule(executable, spec.unregisterHandleRva,
                    evidence.unregisterBytes.size()) ||
        !FitsModule(executable, spec.engineSlotRva, sizeof(std::uint32_t))) {
        return Failed(NativeEventAdmissionFailure::OutsideModule);
    }
    if (!Contains(evidence.textSection, spec.registerHandleRva,
                  evidence.registerBytes.size()) ||
        !Contains(evidence.textSection, spec.unregisterHandleRva,
                  evidence.unregisterBytes.size())) {
        return Failed(NativeEventAdmissionFailure::WrongSection);
    }

    std::uintptr_t engineSlot = 0u;
    std::uintptr_t registerHandle = 0u;
    std::uintptr_t unregisterHandle = 0u;
    if (!CheckedAdd(executable.baseAddress, spec.engineSlotRva, engineSlot) ||
        !CheckedAdd(executable.baseAddress, spec.registerHandleRva,
                    registerHandle) ||
        !CheckedAdd(executable.baseAddress, spec.unregisterHandleRva,
                    unregisterHandle)) {
        return Failed(NativeEventAdmissionFailure::AddressOverflow);
    }

    if (!evidence.registerMemoryReadable ||
        !evidence.unregisterMemoryReadable || !evidence.slotMemoryReadable) {
        return Failed(NativeEventAdmissionFailure::MemoryUnavailable);
    }
    if (!std::equal(spec.expectedRegisterPrefix.begin(),
                    spec.expectedRegisterPrefix.end(),
                    evidence.registerBytes.begin()) ||
        evidence.registerBytes[40] != 0x83u ||
        evidence.registerBytes[41] != 0x7Du ||
        evidence.registerBytes[42] != 0x08u ||
        evidence.registerBytes[43] != 0x00u ||
        evidence.registerBytes[44] != 0x8Bu ||
        evidence.registerBytes[45] != 0x1Du) {
        return Failed(NativeEventAdmissionFailure::RegisterBytesMismatch);
    }
    if (!std::equal(spec.expectedUnregisterPrefix.begin(),
                    spec.expectedUnregisterPrefix.end(),
                    evidence.unregisterBytes.begin()) ||
        evidence.unregisterBytes[9] != 0x56u ||
        evidence.unregisterBytes[10] != 0x8Bu ||
        evidence.unregisterBytes[11] != 0x75u ||
        evidence.unregisterBytes[12] != 0x08u) {
        return Failed(NativeEventAdmissionFailure::UnregisterBytesMismatch);
    }

    const auto expectedSlot = static_cast<std::uint32_t>(engineSlot);
    if (DecodeU32(evidence.registerBytes.data() + kRegisterOperandOffset) !=
            expectedSlot ||
        DecodeU32(evidence.unregisterBytes.data() +
                  kUnregisterOperandOffset) != expectedSlot) {
        return Failed(NativeEventAdmissionFailure::EngineSlotOperandMismatch);
    }
    if (!evidence.engineMemoryReadable) {
        return Failed(NativeEventAdmissionFailure::EngineUnavailable);
    }

    return {engineSlot, registerHandle, unregisterHandle,
            NativeEventAdmissionFailure::None};
}

bool ReadExact(HANDLE file, std::uint64_t offset, void* destination,
               DWORD size) noexcept {
    LARGE_INTEGER position{};
    position.QuadPart = static_cast<LONGLONG>(offset);
    if (SetFilePointerEx(file, position, nullptr, FILE_BEGIN) == FALSE) {
        return false;
    }
    DWORD bytesRead = 0u;
    return ReadFile(file, destination, size, &bytesRead, nullptr) != FALSE &&
           bytesRead == size;
}

NativeEventAdmissionFailure ReadTextSection(
    const ModuleInfo& executable, NativeEventSectionSpan& result) noexcept {
    const HANDLE file = CreateFileW(
        executable.path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return NativeEventAdmissionFailure::FileUnavailable;
    }

    IMAGE_DOS_HEADER dos{};
    bool valid = ReadExact(file, 0u, &dos, static_cast<DWORD>(sizeof(dos))) &&
                 dos.e_magic == IMAGE_DOS_SIGNATURE && dos.e_lfanew > 0;
    DWORD signature = 0u;
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
        for (WORD index = 0u; index < fileHeader.NumberOfSections; ++index) {
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
        return NativeEventAdmissionFailure::InvalidPeImage;
    }
    if (!found) {
        return NativeEventAdmissionFailure::TextSectionMissing;
    }
    return NativeEventAdmissionFailure::None;
}

bool IsExecutableProtection(DWORD protection) noexcept {
    const DWORD basic = protection & 0xFFu;
    return basic == PAGE_EXECUTE || basic == PAGE_EXECUTE_READ ||
           basic == PAGE_EXECUTE_READWRITE ||
           basic == PAGE_EXECUTE_WRITECOPY;
}

bool IsReadable(std::uintptr_t address, std::size_t size,
                bool requireExecutable) noexcept {
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(reinterpret_cast<const void*>(address), &memory,
                     sizeof(memory)) != sizeof(memory) ||
        memory.State != MEM_COMMIT ||
        (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0u ||
        (requireExecutable && !IsExecutableProtection(memory.Protect))) {
        return false;
    }
    const auto base = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
    if (address < base) {
        return false;
    }
    const auto offset = address - base;
    return offset <= memory.RegionSize && size <= memory.RegionSize - offset;
}

template <typename Value>
bool ReadLive(std::uintptr_t address, Value& value) noexcept {
#if defined(_MSC_VER)
    __try {
        std::memcpy(&value, reinterpret_cast<const void*>(address),
                    sizeof(value));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
#endif
    return true;
}

}  // namespace

NativeEventAdmission NativeEventAdmission::Create(
    const ModuleInfo& executable) noexcept {
    const auto identityFailure = CheckIdentity(executable);
    if (identityFailure != NativeEventAdmissionFailure::None) {
        return Failed(identityFailure);
    }

    NativeEventAdmissionEvidence evidence{};
    const auto sectionFailure = ReadTextSection(executable, evidence.textSection);
    if (sectionFailure != NativeEventAdmissionFailure::None) {
        return Failed(sectionFailure);
    }

    const auto metadata =
        Admit(executable, kApprovedNativeEventLayout, evidence, false);
    if (metadata.failure != NativeEventAdmissionFailure::MemoryUnavailable) {
        return metadata;
    }

    std::uintptr_t engineSlot = 0u;
    std::uintptr_t registerHandle = 0u;
    std::uintptr_t unregisterHandle = 0u;
    if (!CheckedAdd(executable.baseAddress,
                    kApprovedNativeEventLayout.engineSlotRva, engineSlot) ||
        !CheckedAdd(executable.baseAddress,
                    kApprovedNativeEventLayout.registerHandleRva,
                    registerHandle) ||
        !CheckedAdd(executable.baseAddress,
                    kApprovedNativeEventLayout.unregisterHandleRva,
                    unregisterHandle)) {
        return Failed(NativeEventAdmissionFailure::AddressOverflow);
    }

    evidence.registerMemoryReadable =
        IsReadable(registerHandle, evidence.registerBytes.size(), true);
    evidence.unregisterMemoryReadable =
        IsReadable(unregisterHandle, evidence.unregisterBytes.size(), true);
    evidence.slotMemoryReadable =
        IsReadable(engineSlot, sizeof(std::uint32_t), false);
    if (!evidence.registerMemoryReadable ||
        !evidence.unregisterMemoryReadable || !evidence.slotMemoryReadable ||
        !ReadLive(registerHandle, evidence.registerBytes) ||
        !ReadLive(unregisterHandle, evidence.unregisterBytes)) {
        return Failed(NativeEventAdmissionFailure::MemoryUnavailable);
    }

    std::uint32_t engineObject = 0u;
    if (!ReadLive(engineSlot, engineObject) || engineObject == 0u) {
        return Failed(NativeEventAdmissionFailure::EngineUnavailable);
    }
    evidence.engineMemoryReadable =
        IsReadable(static_cast<std::uintptr_t>(engineObject),
                   sizeof(std::uint32_t), false);
    return Admit(executable, kApprovedNativeEventLayout, evidence, false);
}

NativeEventAdmission NativeEventAdmission::FromEvidenceForTesting(
    const ModuleInfo& executable, const NativeEventLayoutSpec& spec,
    const NativeEventAdmissionEvidence& evidence) noexcept {
    return Admit(executable, spec, evidence, true);
}

}  // namespace campaign_completion
