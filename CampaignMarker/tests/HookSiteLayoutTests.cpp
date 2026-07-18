#include "hook/HookSiteLayout.h"

#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

campaign_completion::ModuleInfo ApprovedModule() {
    campaign_completion::ModuleInfo module{};
    module.name = L"S4_Main.exe";
    module.baseAddress = 0x00400000u;
    module.size = 0x012BF000u;
    module.version = "2.50.1516.0";
    module.sha256 =
        "3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816";
    return module;
}

campaign_completion::HookSectionSpan TextSection() {
    return {0x00001000u, 0x00B13EF0u, true};
}

campaign_completion::HookSiteAdmission Admit(
    const campaign_completion::ModuleInfo& module,
    const campaign_completion::HookSectionSpan& section,
    const campaign_completion::FixedMapHookSiteSpec& spec,
    const std::array<std::uint8_t, 5>& liveBytes) {
    return campaign_completion::HookSiteLayout::FromEvidenceForTesting(
        module, section, spec, liveBytes);
}

}  // namespace

int RunHookSiteLayoutTests() {
    using campaign_completion::FixedMapHookSiteSpec;
    using campaign_completion::HookSiteFailure;
    using campaign_completion::kApprovedFixedMapHookSite;

    constexpr FixedMapHookSiteSpec approved = kApprovedFixedMapHookSite;
    static_assert(approved.callSiteRva == 0x000FEFA5u);
    static_assert(approved.originalTargetRva == 0x000B1310u);
    static_assert(approved.expectedCallBytes[0] == 0xE8u);
    static_assert(approved.calleeStackCleanup == 0x0000000Cu);

    const auto ok = Admit(ApprovedModule(), TextSection(), approved,
                          approved.expectedCallBytes);
    Require(ok.failure == HookSiteFailure::None, "approved site admitted");
    Require(static_cast<bool>(ok), "successful admission converts to true");
    Require(ok.callSite == 0x004FEFA5u, "call site resolved");
    Require(ok.originalTarget == 0x004B1310u, "target decoded");

    auto wrongVersion = ApprovedModule();
    wrongVersion.version = "2.50.1515.0";
    Require(Admit(wrongVersion, TextSection(), approved,
                  approved.expectedCallBytes).failure ==
                HookSiteFailure::VersionMismatch,
            "wrong version rejected");

    auto wrongHash = ApprovedModule();
    wrongHash.sha256 = "wrong";
    Require(Admit(wrongHash, TextSection(), approved,
                  approved.expectedCallBytes).failure ==
                HookSiteFailure::HashMismatch,
            "wrong hash rejected");

    auto truncatedModule = ApprovedModule();
    truncatedModule.size = approved.callSiteRva + 4u;
    Require(Admit(truncatedModule, TextSection(), approved,
                  approved.expectedCallBytes).failure ==
                HookSiteFailure::OutsideModule,
            "truncated module rejected");

    const campaign_completion::HookSectionSpan readOnlyData{
        0x00F15000u, 0x0023003Au, false};
    Require(Admit(ApprovedModule(), readOnlyData, approved,
                  approved.expectedCallBytes).failure ==
                HookSiteFailure::WrongSection,
            "site outside executable text rejected");

    auto overflowingBase = ApprovedModule();
    overflowingBase.baseAddress =
        (std::numeric_limits<std::uintptr_t>::max)() -
        approved.callSiteRva + 1u;
    Require(Admit(overflowingBase, TextSection(), approved,
                  approved.expectedCallBytes).failure ==
                HookSiteFailure::AddressOverflow,
            "base plus call-site RVA overflow rejected");

    constexpr std::array<std::uint8_t, 5> prepatched{
        0x90u, 0x90u, 0x90u, 0x90u, 0x90u};
    const auto bytesMismatch =
        Admit(ApprovedModule(), TextSection(), approved, prepatched);
    Require(bytesMismatch.failure == HookSiteFailure::BytesMismatch,
            "prepatched site rejected");
    Require(!static_cast<bool>(bytesMismatch),
            "failed admission converts to false");
    Require(bytesMismatch.callSite == 0u,
            "failure does not expose a call-site address");
    Require(bytesMismatch.originalTarget == 0u,
            "failure does not expose a target address");

    auto wrongTarget = approved;
    wrongTarget.expectedCallBytes = {0xE8u, 0u, 0u, 0u, 0u};
    Require(Admit(ApprovedModule(), TextSection(), wrongTarget,
                  wrongTarget.expectedCallBytes).failure ==
                HookSiteFailure::TargetMismatch,
            "wrong rel32 target rejected");

    return 0;
}
