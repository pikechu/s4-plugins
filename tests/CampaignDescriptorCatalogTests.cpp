#include "campaign/CampaignDescriptorCatalog.h"

#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

campaign_completion::ModuleInfo ApprovedModule() {
    campaign_completion::ModuleInfo module{};
    module.name = L"S4_Main.exe";
    module.version = "2.50.1516.0";
    module.sha256 =
        "3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816";
    return module;
}

}  // namespace

int RunCampaignDescriptorCatalogTests() {
    using namespace campaign_completion;

    CampaignDescriptorEvidence evidence{};
    evidence.addOn = true;
    evidence.missionCd = true;
    evidence.original = true;
    evidence.darkTribe = true;
    const auto catalog = AdmitCampaignDescriptorCatalog(ApprovedModule(), evidence);
    Require(catalog.executableAdmitted, "exact executable admits the catalog");
    Require(catalog.GroupAdmitted(CampaignDescriptorGroup::AddOn) &&
                catalog.GroupAdmitted(CampaignDescriptorGroup::MissionCd) &&
                catalog.GroupAdmitted(CampaignDescriptorGroup::Original) &&
                catalog.GroupAdmitted(CampaignDescriptorGroup::DarkTribe),
            "independent frozen groups are admitted");
    Require(!catalog.GroupAdmitted(CampaignDescriptorGroup::NewWorld) &&
                !catalog.GroupAdmitted(CampaignDescriptorGroup::NewWorld2),
            "unproven New World groups remain disabled");

    CampaignMenuAssociation association{};
    association.page = S4_SCREEN_ADDON_TROJAN;
    association.control = {91u, 320u, 200u, 175u, 30u};
    association.sessionId = 41u;
    association.relative = L"Map\\Campaign\\ao_trojan01.map";
    auto validation = ValidateCampaignDescriptor(catalog, association);
    Require(validation.status == CampaignDescriptorValidationStatus::Matched &&
                validation.record != nullptr &&
                validation.record->ordinal == 0u,
            "same-session exact Add-on relative matches immutable descriptor");

    association.relative = L"Map\\Campaign\\ao_trojan02.map";
    Require(ValidateCampaignDescriptor(catalog, association).status ==
                CampaignDescriptorValidationStatus::RelativeMismatch,
            "wrong relative fails closed without a name fallback");
    association.relative = L"Map\\Campaign\\ao_trojan01.map";
    association.control.x = 321u;
    Require(ValidateCampaignDescriptor(catalog, association).status ==
                CampaignDescriptorValidationStatus::GeometryMismatch,
            "public rectangle is part of descriptor identity");

    association.page = S4_SCREEN_MISSIONCD_ROMAN;
    association.control = {1904u, 237u, 195u, 175u, 30u};
    association.relative = L"Map\\Campaign\\mcd2_roman2.map";
    Require(ValidateCampaignDescriptor(catalog, association).status ==
                CampaignDescriptorValidationStatus::Matched,
            "Mission CD second Roman mission is independently fixed");

    association.page = S4_SCREEN_SINGLEPLAYER_CAMPAIGN;
    association.control = {2039u, 420u, 150u, 175u, 30u};
    association.relative = L"Map\\Campaign\\viking02.map";
    Require(ValidateCampaignDescriptor(catalog, association).status ==
                CampaignDescriptorValidationStatus::Matched,
            "original Viking descriptor uses the exact static ordinal");

    association.page = S4_SCREEN_SINGLEPLAYER_DARKTRIBE;
    association.control = {2083u, 500u, 168u, 175u, 30u};
    association.relative = L"Map\\Campaign\\dark03.map";
    Require(ValidateCampaignDescriptor(catalog, association).status ==
                CampaignDescriptorValidationStatus::Matched,
            "accepted Dark Tribe same-session anchor remains reusable");

    association.page = S4_SCREEN_NEWWORLD;
    association.control = {1u, 1u, 1u, 1u, 1u};
    association.relative = L"Map\\Campaign\\md_roman1.map";
    Require(ValidateCampaignDescriptor(catalog, association).status ==
                CampaignDescriptorValidationStatus::GroupNotAdmitted,
            "an evidence gap cannot be classified from a plausible path");

    auto wrongModule = ApprovedModule();
    wrongModule.sha256.back() = '0';
    const auto rejected = AdmitCampaignDescriptorCatalog(wrongModule, evidence);
    Require(!rejected.executableAdmitted &&
                ValidateCampaignDescriptor(rejected, association).status ==
                    CampaignDescriptorValidationStatus::CatalogNotAdmitted,
            "executable mismatch rejects every descriptor");

    evidence.missionCd = false;
    const auto localFailure =
        AdmitCampaignDescriptorCatalog(ApprovedModule(), evidence);
    association.page = S4_SCREEN_MISSIONCD_ROMAN;
    Require(ValidateCampaignDescriptor(localFailure, association).status ==
                CampaignDescriptorValidationStatus::GroupNotAdmitted &&
                localFailure.GroupAdmitted(CampaignDescriptorGroup::AddOn),
            "one frozen-window failure disables only its group");

    return 0;
}
