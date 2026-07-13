#include "native/NativeEventAdmission.h"

#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>

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

campaign_completion::NativeEventAdmissionEvidence ApprovedEvidence() {
    campaign_completion::NativeEventAdmissionEvidence evidence{};
    evidence.textSection = {0x00001000u, 0x00B13EF0u, true};
    evidence.registerBytes = {
        0x55u, 0x8Bu, 0xECu, 0x6Au, 0xFFu, 0x68u, 0x5Fu, 0x5Cu,
        0xEAu, 0x00u, 0x64u, 0xA1u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x50u, 0x83u, 0xECu, 0x08u, 0x53u, 0x56u, 0x57u, 0xA1u,
        0x44u, 0x24u, 0x16u, 0x01u, 0x33u, 0xC5u, 0x50u, 0x8Du,
        0x45u, 0xF4u, 0x64u, 0xA3u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x83u, 0x7Du, 0x08u, 0x00u, 0x8Bu, 0x1Du, 0x1Cu, 0xB1u,
        0x46u, 0x01u};
    evidence.unregisterBytes = {
        0x55u, 0x8Bu, 0xECu, 0x51u, 0xA1u, 0x1Cu, 0xB1u,
        0x46u, 0x01u, 0x56u, 0x8Bu, 0x75u, 0x08u};
    evidence.registerMemoryReadable = true;
    evidence.unregisterMemoryReadable = true;
    evidence.slotMemoryReadable = true;
    evidence.engineMemoryReadable = true;
    return evidence;
}

campaign_completion::NativeEventAdmission Admit(
    const campaign_completion::ModuleInfo& module,
    const campaign_completion::NativeEventAdmissionEvidence& evidence) {
    return campaign_completion::NativeEventAdmission::FromEvidenceForTesting(
        module, campaign_completion::kApprovedNativeEventLayout, evidence);
}

}  // namespace

int RunNativeEventAdmissionTests() {
    using campaign_completion::NativeEventAdmissionFailure;
    using campaign_completion::kApprovedNativeEventLayout;

    static_assert(kApprovedNativeEventLayout.engineSlotRva == 0x0106B11Cu);
    static_assert(kApprovedNativeEventLayout.registerHandleRva == 0x0005A8A0u);
    static_assert(kApprovedNativeEventLayout.unregisterHandleRva == 0x0005A990u);
    static_assert(kApprovedNativeEventLayout.expectedRegisterPrefix[0] == 0x55u);
    static_assert(kApprovedNativeEventLayout.expectedUnregisterPrefix[4] == 0xA1u);

    const auto ok = Admit(ApprovedModule(), ApprovedEvidence());
    Require(static_cast<bool>(ok), "approved native interface admitted");
    Require(ok.failure == NativeEventAdmissionFailure::None,
            "approved evidence has no failure");
    Require(ok.engineSlot == 0x0146B11Cu, "engine slot resolved");
    Require(ok.registerHandle == 0x0045A8A0u, "register operation resolved");
    Require(ok.unregisterHandle == 0x0045A990u,
            "unregister operation resolved");

    auto wrongVersion = ApprovedModule();
    wrongVersion.version = "2.50.1515.0";
    Require(Admit(wrongVersion, ApprovedEvidence()).failure ==
                NativeEventAdmissionFailure::VersionMismatch,
            "wrong version rejected");

    auto wrongHash = ApprovedModule();
    wrongHash.sha256.back() = '0';
    Require(Admit(wrongHash, ApprovedEvidence()).failure ==
                NativeEventAdmissionFailure::HashMismatch,
            "wrong hash rejected");

    auto truncated = ApprovedModule();
    truncated.size = kApprovedNativeEventLayout.engineSlotRva;
    Require(Admit(truncated, ApprovedEvidence()).failure ==
                NativeEventAdmissionFailure::OutsideModule,
            "RVA outside module rejected");

    auto overflow = ApprovedModule();
    overflow.baseAddress = (std::numeric_limits<std::uintptr_t>::max)() -
                           kApprovedNativeEventLayout.engineSlotRva + 1u;
    Require(Admit(overflow, ApprovedEvidence()).failure ==
                NativeEventAdmissionFailure::AddressOverflow,
            "module address overflow rejected");

    auto nonExecutable = ApprovedEvidence();
    nonExecutable.textSection.executable = false;
    Require(Admit(ApprovedModule(), nonExecutable).failure ==
                NativeEventAdmissionFailure::WrongSection,
            "non-executable operations rejected");

    auto registerUnreadable = ApprovedEvidence();
    registerUnreadable.registerMemoryReadable = false;
    Require(Admit(ApprovedModule(), registerUnreadable).failure ==
                NativeEventAdmissionFailure::MemoryUnavailable,
            "unreadable register operation rejected");

    auto unregisterUnreadable = ApprovedEvidence();
    unregisterUnreadable.unregisterMemoryReadable = false;
    Require(Admit(ApprovedModule(), unregisterUnreadable).failure ==
                NativeEventAdmissionFailure::MemoryUnavailable,
            "unreadable unregister operation rejected");

    auto slotUnreadable = ApprovedEvidence();
    slotUnreadable.slotMemoryReadable = false;
    Require(Admit(ApprovedModule(), slotUnreadable).failure ==
                NativeEventAdmissionFailure::MemoryUnavailable,
            "unreadable engine slot rejected");

    auto engineUnreadable = ApprovedEvidence();
    engineUnreadable.engineMemoryReadable = false;
    Require(Admit(ApprovedModule(), engineUnreadable).failure ==
                NativeEventAdmissionFailure::EngineUnavailable,
            "unreadable engine object rejected");

    auto badRegister = ApprovedEvidence();
    badRegister.registerBytes[0] = 0x90u;
    Require(Admit(ApprovedModule(), badRegister).failure ==
                NativeEventAdmissionFailure::RegisterBytesMismatch,
            "register opcode mismatch rejected");

    auto badUnregister = ApprovedEvidence();
    badUnregister.unregisterBytes[0] = 0x90u;
    Require(Admit(ApprovedModule(), badUnregister).failure ==
                NativeEventAdmissionFailure::UnregisterBytesMismatch,
            "unregister opcode mismatch rejected");

    auto badRegisterOperand = ApprovedEvidence();
    badRegisterOperand.registerBytes[46] ^= 0x01u;
    Require(Admit(ApprovedModule(), badRegisterOperand).failure ==
                NativeEventAdmissionFailure::EngineSlotOperandMismatch,
            "register global operand mismatch rejected");

    auto badUnregisterOperand = ApprovedEvidence();
    badUnregisterOperand.unregisterBytes[5] ^= 0x01u;
    const auto failed = Admit(ApprovedModule(), badUnregisterOperand);
    Require(failed.failure ==
                NativeEventAdmissionFailure::EngineSlotOperandMismatch,
            "unregister global operand mismatch rejected");
    Require(!static_cast<bool>(failed), "failed admission converts to false");
    Require(failed.engineSlot == 0u && failed.registerHandle == 0u &&
                failed.unregisterHandle == 0u,
            "failed admission exposes no addresses");

    return 0;
}
