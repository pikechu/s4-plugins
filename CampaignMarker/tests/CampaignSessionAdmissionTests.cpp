#include "campaign/CampaignSessionAdmission.h"

#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

campaign_completion::CampaignMenuAssociation Association(
    const campaign_completion::CampaignDescriptorRecord& descriptor,
    std::uint64_t sessionId) {
    campaign_completion::CampaignMenuAssociation association{};
    association.page = descriptor.page;
    association.control = descriptor.control;
    association.sessionId = sessionId;
    association.descriptorKey = descriptor.key;
    association.relative = descriptor.relative;
    return association;
}

campaign_completion::ConfirmedMapIdentity Identity(
    const campaign_completion::CampaignDescriptorRecord& descriptor,
    std::uint64_t sessionId) {
    return {sessionId, L"RD_PlayerSave", descriptor.relative};
}

campaign_completion::CampaignDescriptorValidation Matched(
    const campaign_completion::CampaignDescriptorRecord& descriptor) {
    return {
        campaign_completion::CampaignDescriptorValidationStatus::Matched,
        &descriptor};
}

}  // namespace

int RunCampaignSessionAdmissionTests() {
    using namespace campaign_completion;

    const auto& trojan = AllCampaignDescriptors()[16u];
    Require(std::string(trojan.key) == "addon-trojan-01",
            "fixture uses the frozen Trojan regression descriptor");
    const LaunchOriginSnapshot misleading{
        LaunchSource::RandomMap, SessionEligibility::Eligible};

    CampaignSessionAdmission admitted;
    admitted.BeginSession(41u, misleading);
    const auto promoted = admitted.Observe(
        Identity(trojan, 41u), Association(trojan, 41u), Matched(trojan));
    Require(promoted.has_value() &&
                promoted->source == LaunchSource::Campaign &&
                promoted->eligibility == SessionEligibility::Eligible,
            "exact same-session descriptor overrides misleading presentation origin");
    Require(!admitted.Observe(
                 Identity(trojan, 41u), Association(trojan, 41u),
                 Matched(trojan)).has_value(),
            "the campaign promotion is one-shot");

    for (const auto index : {0u, 28u, 56u, 70u, 86u, 95u}) {
        const auto& descriptor = AllCampaignDescriptors()[index];
        CampaignSessionAdmission family;
        family.BeginSession(50u + index, misleading);
        Require(family.Observe(
                    Identity(descriptor, 50u + index),
                    Association(descriptor, 50u + index),
                    Matched(descriptor)).has_value(),
                "every descriptor family shares the exact bounded gate");
    }

    CampaignSessionAdmission wrongSession;
    wrongSession.BeginSession(70u, misleading);
    Require(!wrongSession.Observe(
                 Identity(trojan, 71u), Association(trojan, 70u),
                 Matched(trojan)).has_value(),
            "identity session mismatch fails closed");

    auto wrongKey = Association(trojan, 72u);
    wrongKey.descriptorKey = "dark-01";
    CampaignSessionAdmission keyGate;
    keyGate.BeginSession(72u, misleading);
    Require(!keyGate.Observe(
                 Identity(trojan, 72u), wrongKey, Matched(trojan)).has_value(),
            "descriptor key mismatch fails closed");

    auto wrongCase = Association(trojan, 73u);
    wrongCase.relative[0] = L'm';
    CampaignSessionAdmission relativeGate;
    relativeGate.BeginSession(73u, misleading);
    Require(!relativeGate.Observe(
                 Identity(trojan, 73u), wrongCase, Matched(trojan)).has_value(),
            "case variation fails closed");

    CampaignSessionAdmission geometryGate;
    geometryGate.BeginSession(74u, misleading);
    Require(!geometryGate.Observe(
                 Identity(trojan, 74u), Association(trojan, 74u),
                 {CampaignDescriptorValidationStatus::GeometryMismatch,
                  &trojan}).has_value(),
            "non-matched descriptor validation fails closed");

    CampaignSessionAdmission online;
    online.BeginSession(
        75u, {LaunchSource::OnlineMultiplayer,
              SessionEligibility::ExcludedOnlineMultiplayer});
    Require(!online.Observe(
                 Identity(trojan, 75u), Association(trojan, 75u),
                 Matched(trojan)).has_value(),
            "explicit online origin can never promote");

    auto randomIdentity = Identity(trojan, 76u);
    randomIdentity.relative = L"RD_LCGSDR30";
    CampaignSessionAdmission random;
    random.BeginSession(76u, misleading);
    Require(!random.Observe(
                 randomIdentity, Association(trojan, 76u),
                 Matched(trojan)).has_value(),
            "a true RD relative never enters the campaign gate");

    CampaignSessionAdmission disabled;
    disabled.BeginSession(77u, misleading);
    disabled.Disable();
    Require(!disabled.Observe(
                 Identity(trojan, 77u), Association(trojan, 77u),
                 Matched(trojan)).has_value(),
            "shutdown permanently disables promotion");

    return 0;
}
