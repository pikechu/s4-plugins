#include "campaign/CampaignDescriptorCatalog.h"

#include <windows.h>

#include <array>
#include <cstring>
#include <limits>
#include <string_view>

namespace campaign_completion {
namespace {

constexpr std::size_t GroupIndex(CampaignDescriptorGroup group) noexcept {
    return static_cast<std::size_t>(group);
}

constexpr std::array<CampaignDescriptorRecord,
                     kPhase6DCampaignDescriptorCount> kDescriptors{{
#include "campaign/Phase6DDescriptorTable.inc"
}};

bool SameControlId(const CampaignDescriptorRecord& record,
                   const CampaignMenuAssociation& association) noexcept {
    return record.page == association.page &&
           record.control.controlId == association.control.controlId;
}

bool SameGeometry(const CampaignControlIdentity& left,
                  const CampaignControlIdentity& right) noexcept {
    return left.x == right.x && left.y == right.y &&
           left.width == right.width && left.height == right.height;
}

CampaignDescriptorGroup GroupForPage(DWORD page) noexcept {
    switch (page) {
        case S4_SCREEN_ADDON_TROJAN:
        case S4_SCREEN_ADDON_ROMAN:
        case S4_SCREEN_ADDON_MAYAN:
        case S4_SCREEN_ADDON_VIKING:
        case S4_SCREEN_ADDON_SETTLE:
            return CampaignDescriptorGroup::AddOn;
        case S4_SCREEN_MISSIONCD:
        case S4_SCREEN_MISSIONCD_ROMAN:
        case S4_SCREEN_MISSIONCD_VIKING:
        case S4_SCREEN_MISSIONCD_MAYAN:
        case S4_SCREEN_MISSIONCD_CONFL:
            return CampaignDescriptorGroup::MissionCd;
        case S4_SCREEN_SINGLEPLAYER_CAMPAIGN:
            return CampaignDescriptorGroup::Original;
        case S4_SCREEN_SINGLEPLAYER_DARKTRIBE:
            return CampaignDescriptorGroup::DarkTribe;
        case S4_SCREEN_NEWWORLD:
            return CampaignDescriptorGroup::NewWorld;
        case S4_SCREEN_NEWWORLD2:
            return CampaignDescriptorGroup::NewWorld2;
        default:
            return CampaignDescriptorGroup::Count;
    }
}

bool Readable(const void* address, std::size_t size) noexcept {
    if (address == nullptr || size == 0u) return false;
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(address, &memory, sizeof(memory)) != sizeof(memory) ||
        memory.State != MEM_COMMIT ||
        (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0u) return false;
    const auto begin = reinterpret_cast<std::uintptr_t>(address);
    const auto region = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
    if (begin < region || size > memory.RegionSize - (begin - region)) {
        return false;
    }
    return true;
}

template <std::size_t N>
bool Match(const ModuleInfo& executable, std::uint32_t rva,
           const std::array<BYTE, N>& expected) noexcept {
    if (executable.baseAddress == 0u || rva > executable.size ||
        N > executable.size - rva ||
        executable.baseAddress >
            (std::numeric_limits<std::uintptr_t>::max)() - rva) return false;
    const auto* address = reinterpret_cast<const void*>(
        executable.baseAddress + static_cast<std::uintptr_t>(rva));
    if (!Readable(address, N)) return false;
#if defined(_MSC_VER)
    __try {
        return std::memcmp(address, expected.data(), N) == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    return std::memcmp(address, expected.data(), N) == 0;
#endif
}

bool MatchRelocatedPointer(const ModuleInfo& executable, std::uint32_t rva,
                           std::uint32_t targetRva) noexcept {
    if (executable.baseAddress == 0u || rva > executable.size ||
        sizeof(std::uint32_t) > executable.size - rva ||
        executable.baseAddress >
            (std::numeric_limits<std::uint32_t>::max)() - targetRva) {
        return false;
    }
    const auto* address = reinterpret_cast<const void*>(
        executable.baseAddress + static_cast<std::uintptr_t>(rva));
    if (!Readable(address, sizeof(std::uint32_t))) return false;
    std::uint32_t actual = 0u;
#if defined(_MSC_VER)
    __try {
        std::memcpy(&actual, address, sizeof(actual));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    std::memcpy(&actual, address, sizeof(actual));
#endif
    return actual == static_cast<std::uint32_t>(executable.baseAddress) +
                         targetRva;
}

template <std::size_t N>
bool MatchRelocatedRvaTable(
    const ModuleInfo& executable, std::uint32_t rva,
    const std::array<std::uint32_t, N>& targetRvas) noexcept {
    for (std::size_t index = 0u; index < N; ++index) {
        if (!MatchRelocatedPointer(
                executable,
                rva + static_cast<std::uint32_t>(index * sizeof(std::uint32_t)),
                targetRvas[index])) {
            return false;
        }
    }
    return true;
}

bool MatchFileWindow(const ModuleInfo& executable, std::uint64_t offset,
                     std::size_t length,
                     std::string_view expected) noexcept {
    try {
        const auto actual =
            Sha256FileRange(executable.path, offset, length);
        return actual.has_value() && *actual == expected;
    } catch (...) {
        return false;
    }
}

}  // namespace

bool CampaignDescriptorCatalog::GroupAdmitted(
    CampaignDescriptorGroup group) const noexcept {
    const auto index = GroupIndex(group);
    return executableAdmitted && index < groups.size() && groups[index];
}

CampaignDescriptorEvidence InspectCampaignDescriptorEvidence(
    const ModuleInfo& executable) noexcept {
    CampaignDescriptorEvidence result{};
    if (CheckTargetExecutable(executable) != CompatibilityResult::Compatible) {
        return result;
    }
    result.addOn =
        Match(executable, 0x0006B27Bu,
              std::array<BYTE, 12u>{0x0F, 0xB7, 0x45, 0x0C, 0x83, 0xC0,
                                    0xA6, 0x83, 0xF8, 0x0C, 0x0F, 0x87}) &&
        Match(executable, 0x0006A25Bu,
              std::array<BYTE, 12u>{0x0F, 0xB7, 0x45, 0x0C, 0x83, 0xC0,
                                    0xDF, 0x83, 0xF8, 0x04, 0x0F, 0x87});
    result.missionCd =
        Match(executable, 0x000961F6u,
              std::array<BYTE, 8u>{0x85, 0xC9, 0x0F, 0x84, 0x08, 0x01,
                                   0x00, 0x00}) &&
        Match(executable, 0x000963EBu,
              std::array<BYTE, 14u>{0x0F, 0xB7, 0x45, 0x0C, 0x05, 0x91, 0xF8,
                                    0xFF, 0xFF, 0x83, 0xF8, 0x0C, 0x0F, 0x87}) &&
        Match(executable, 0x000965A0u,
              std::array<BYTE, 13u>{0x00, 0x01, 0x02, 0x03, 0x04, 0x06, 0x06,
                                    0x06, 0x06, 0x06, 0x06, 0x06, 0x05}) &&
        MatchRelocatedRvaTable(
            executable, 0x00096584u,
            std::array<std::uint32_t, 7u>{0x0009643Eu, 0x00096471u,
                                          0x000964A4u, 0x000964D4u,
                                          0x00096504u, 0x0009640Bu,
                                          0x0009653Eu}) &&
        Match(executable, 0x00096452u,
              std::array<BYTE, 11u>{0x6A, 0x00, 0x6A, 0x00, 0x6A, 0x00,
                                    0x68, 0x60, 0x1B, 0x00, 0x00}) &&
        Match(executable, 0x00096485u,
              std::array<BYTE, 11u>{0x6A, 0x00, 0x6A, 0x00, 0x6A, 0x01,
                                    0x68, 0x60, 0x1B, 0x00, 0x00}) &&
        Match(executable, 0x00125368u,
              std::array<BYTE, 20u>{0x8B, 0x4D, 0x08, 0x8B, 0x51, 0x04, 0x8B,
                                    0xC2, 0x83, 0xE8, 0x0B, 0x74, 0x30, 0x2D,
                                    0x55, 0x1B, 0x00, 0x00, 0x74, 0x14}) &&
        Match(executable, 0x00125390u,
              std::array<BYTE, 3u>{0x8B, 0x51, 0x08}) &&
        Match(executable, 0x00125398u,
              std::array<BYTE, 6u>{0xC1, 0xE2, 0x10, 0x83, 0xCA, 0x05}) &&
        Match(executable, 0x00124701u,
              std::array<BYTE, 9u>{0x8B, 0x46, 0x04, 0x83, 0xE8, 0x05, 0x8D,
                                   0x04, 0x40}) &&
        MatchRelocatedPointer(executable, 0x0012470Du, 0x0109C32Cu) &&
        MatchRelocatedPointer(executable, 0x00124715u, 0x0109C318u) &&
        Match(executable, 0x0012471Du,
              std::array<BYTE, 6u>{0x8B, 0x46, 0x08, 0x40, 0x50, 0x51}) &&
        Match(executable, 0x00023ADCu,
              std::array<BYTE, 4u>{0x6A, 0x1D, 0x33, 0xC0}) &&
        MatchRelocatedPointer(executable, 0x00023AEBu, 0x00C573E0u) &&
        MatchRelocatedPointer(executable, 0x00023AF0u, 0x0109C318u) &&
        Match(executable, 0x00C573E0u,
              std::array<BYTE, 60u>{
                  0x4D, 0x00, 0x61, 0x00, 0x70, 0x00, 0x5C, 0x00, 0x43, 0x00,
                  0x61, 0x00, 0x6D, 0x00, 0x70, 0x00, 0x61, 0x00, 0x69, 0x00,
                  0x67, 0x00, 0x6E, 0x00, 0x5C, 0x00, 0x6D, 0x00, 0x64, 0x00,
                  0x5F, 0x00, 0x72, 0x00, 0x6F, 0x00, 0x6D, 0x00, 0x61, 0x00,
                  0x6E, 0x00, 0x25, 0x00, 0x30, 0x00, 0x31, 0x00, 0x69, 0x00,
                  0x2E, 0x00, 0x6D, 0x00, 0x61, 0x00, 0x70, 0x00, 0x00, 0x00});
    result.original =
        Match(executable, 0x0007BF96u,
              std::array<BYTE, 8u>{0x85, 0xC9, 0x0F, 0x84, 0xA2, 0x01,
                                   0x00, 0x00}) &&
        Match(executable, 0x0007C22Bu,
              std::array<BYTE, 14u>{0x0F, 0xB7, 0x45, 0x0C, 0x05, 0x0B, 0xF8,
                                    0xFF, 0xFF, 0x83, 0xF8, 0x09, 0x0F, 0x87});
    result.darkTribe =
        Match(executable, 0x0007D7B6u,
              std::array<BYTE, 8u>{0x85, 0xC9, 0x0F, 0x84, 0x22, 0x02,
                                   0x00, 0x00}) &&
        Match(executable, 0x0007DAEBu,
              std::array<BYTE, 4u>{0x0F, 0xB7, 0x45, 0x0C});
    result.addOn =
        result.addOn &&
        MatchFileWindow(executable, 0x695F0u, 5952u,
            "f096f8969a8304498b0cd053adbb6c5a7793e77eb317f9c591b4c41f67dcb36d") &&
        MatchFileWindow(executable, 0x10D1A0u, 8960u,
            "09408e191881efa8ba44705bb66a358e3e83d3e362d288ffed6a3596e4b98971") &&
        MatchFileWindow(executable, 0x22490u, 415u,
            "9459c9d0fc6c027ec736eb9bf9b3c0ddf6642edc0e8957919406bc8f81812799");
    result.missionCd =
        result.missionCd &&
        MatchFileWindow(executable, 0x95230u, 5136u,
            "147f9db65cbdd0b09fba7a0001cb15c96bea8cba4855226c6ebfca98c59b90b3") &&
        MatchFileWindow(executable, 0x123750u, 7104u,
            "c34714420d42d8f680705dd2d88dd158eb92ace78bcbe2b601a7eba8788e141b") &&
        MatchFileWindow(executable, 0x22EB0u, 415u,
            "63278d29a609391708defde9fbbf28fd9c09d9f8a1ac9521fd8beab849aef8b8");
    result.newWorld =
        MatchFileWindow(executable, 0x94670u, 1040u,
            "85f8bbd64886596ca14e3e9f5e624c91b30785331e2e4b243abedf33455d1e47") &&
        MatchFileWindow(executable, 0x1228F0u, 3376u,
            "12d6217ca1e1354ac4ff4b9b2d8dbf521c743bcdf5e3205f20c090b59d38372a") &&
        MatchFileWindow(executable, 0x22D80u, 297u,
            "d37b56be5c1509afac177a56b17f9a9849b1ebf25cc696de848fb6d74a1c5c56");
    result.newWorld2 =
        MatchFileWindow(executable, 0xAF380u, 2752u,
            "e1374d0d00cdd2d2280ea963907cf6f9692198c449862fd59895e80e2e3c4b50") &&
        MatchFileWindow(executable, 0x127EE0u, 3556u,
            "08595043d6f859af73f4f92311023d60b1da1ebaf86f80c2e47b556741945471") &&
        MatchFileWindow(executable, 0x230A0u, 297u,
            "3c561626b14678bf2a94d76097c44b785a91dd520a0e6d4438cecb6e1576ce88");
    result.original =
        result.original &&
        MatchFileWindow(executable, 0x7B550u, 1072u,
            "ded0825cc53c46669bc2b8ba8e2bf19085b81129554609e81d035ce724d769b3") &&
        MatchFileWindow(executable, 0x10FA10u, 3751u,
            "685185042a2f72803083ffc09df07b225812a2a8569704e78e3908b9e3e95722") &&
        MatchFileWindow(executable, 0x227F0u, 384u,
            "96a78fc162b178d98a4733fc758b0cc1fb1e73ba75ce51bd26b5a4308a6bd978");
    result.darkTribe =
        result.darkTribe &&
        MatchFileWindow(executable, 0x7CBB0u, 561u,
            "44b14df6007177e510e7f68311f7827ead6cf7d9f3ae8271f9daa60cfeeae016") &&
        MatchFileWindow(executable, 0x7CEEBu, 677u,
            "e0a84e09d81d061ea4a948ee3a97c86dbfc6d74f14a1ce8f138a6ef17964df34");
    return result;
}

CampaignDescriptorCatalog AdmitCampaignDescriptorCatalog(
    const ModuleInfo& executable) noexcept {
    return AdmitCampaignDescriptorCatalog(
        executable, InspectCampaignDescriptorEvidence(executable));
}

CampaignDescriptorCatalog AdmitCampaignDescriptorCatalog(
    const ModuleInfo& executable,
    const CampaignDescriptorEvidence& evidence) noexcept {
    CampaignDescriptorCatalog result{};
    result.executableAdmitted =
        CheckTargetExecutable(executable) == CompatibilityResult::Compatible;
    if (!result.executableAdmitted) return result;
    result.groups[GroupIndex(CampaignDescriptorGroup::AddOn)] = evidence.addOn;
    result.groups[GroupIndex(CampaignDescriptorGroup::MissionCd)] =
        evidence.missionCd;
    result.groups[GroupIndex(CampaignDescriptorGroup::NewWorld)] =
        evidence.newWorld;
    result.groups[GroupIndex(CampaignDescriptorGroup::NewWorld2)] =
        evidence.newWorld2;
    result.groups[GroupIndex(CampaignDescriptorGroup::Original)] =
        evidence.original;
    result.groups[GroupIndex(CampaignDescriptorGroup::DarkTribe)] =
        evidence.darkTribe;
    return result;
}

CampaignDescriptorValidation ValidateCampaignDescriptor(
    const CampaignDescriptorCatalog& catalog,
    const CampaignMenuAssociation& association) noexcept {
    if (!catalog.executableAdmitted) {
        return {CampaignDescriptorValidationStatus::CatalogNotAdmitted,
                nullptr};
    }
    for (const auto& record : kDescriptors) {
        if (!SameControlId(record, association)) continue;
        if (!catalog.GroupAdmitted(record.group)) {
            return {CampaignDescriptorValidationStatus::GroupNotAdmitted,
                    &record};
        }
        if (!SameGeometry(record.control, association.control)) {
            return {CampaignDescriptorValidationStatus::GeometryMismatch,
                    &record};
        }
        if (record.relative == nullptr ||
            association.relative != std::wstring_view(record.relative)) {
            return {CampaignDescriptorValidationStatus::RelativeMismatch,
                    &record};
        }
        return {CampaignDescriptorValidationStatus::Matched, &record};
    }
    const auto group = GroupForPage(association.page);
    if (group != CampaignDescriptorGroup::Count &&
        !catalog.GroupAdmitted(group)) {
        return {CampaignDescriptorValidationStatus::GroupNotAdmitted, nullptr};
    }
    return {CampaignDescriptorValidationStatus::ControlUnknown, nullptr};
}

const CampaignDescriptorRecord* FindAdmittedCampaignDescriptor(
    const CampaignDescriptorCatalog& catalog, DWORD page,
    const CampaignControlIdentity& control) noexcept {
    if (!catalog.executableAdmitted) return nullptr;
    for (const auto& record : kDescriptors) {
        if (record.page == page && record.control.controlId == control.controlId &&
            SameGeometry(record.control, control) &&
            catalog.GroupAdmitted(record.group)) {
            return &record;
        }
    }
    return nullptr;
}

const CampaignDescriptorRecord* FindAdmittedCampaignDescriptorRelative(
    const CampaignDescriptorCatalog& catalog,
    std::wstring_view relative) noexcept {
    if (!catalog.executableAdmitted || relative.empty()) return nullptr;
    const CampaignDescriptorRecord* match = nullptr;
    for (const auto& record : kDescriptors) {
        if (!catalog.GroupAdmitted(record.group) || record.relative == nullptr ||
            relative != std::wstring_view(record.relative)) {
            continue;
        }
        if (match != nullptr) return nullptr;
        match = &record;
    }
    return match;
}

const std::array<CampaignDescriptorRecord,
                 kPhase6DCampaignDescriptorCount>&
AllCampaignDescriptors() noexcept {
    return kDescriptors;
}

}  // namespace campaign_completion
