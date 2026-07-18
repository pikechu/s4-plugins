#include "campaign/CampaignLaunchAssociation.h"
#include "campaign/CampaignDescriptorCatalog.h"

#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

campaign_completion::CampaignMenuSnapshot Snapshot(
    DWORD page = campaign_completion::kDarkTribeCampaignPage) {
    campaign_completion::CampaignMenuSnapshot snapshot{};
    snapshot.status = campaign_completion::CampaignMenuSnapshotStatus::Success;
    snapshot.page = page;
    snapshot.count = 1u;
    snapshot.features[0].valueLink = 2081u;
    snapshot.features[0].x = 500u;
    snapshot.features[0].y = 104u;
    snapshot.features[0].width = 175u;
    snapshot.features[0].height = 30u;
    snapshot.features[0].hasText = true;
    snapshot.features[0].text.bytes[0] = 'M';
    snapshot.features[0].text.bytes[1] = '\0';
    snapshot.features[0].text.length = 1u;
    return snapshot;
}

S4UiElement Element() {
    S4UiElement element{};
    element.id = 2081u;
    element.x = 500u;
    element.y = 104u;
    element.w = 175u;
    element.h = 30u;
    return element;
}

campaign_completion::CampaignDescriptorCatalog Catalog() {
    campaign_completion::ModuleInfo module{};
    module.name = L"S4_Main.exe";
    module.version = "2.50.1516.0";
    module.sha256 =
        "3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816";
    campaign_completion::CampaignDescriptorEvidence evidence{};
    evidence.addOn = true;
    evidence.missionCd = true;
    evidence.original = true;
    evidence.darkTribe = true;
    return campaign_completion::AdmitCampaignDescriptorCatalog(module,
                                                                 evidence);
}

}  // namespace

int RunCampaignLaunchAssociationTests() {
    using namespace campaign_completion;

    CampaignLaunchAssociation association;
    const auto catalog = Catalog();
    auto element = Element();
    association.ObservePage(kDarkTribeCampaignPage);
    association.ObserveSnapshot(Snapshot());
    Require(!association.ObserveClick(WM_LBUTTONDOWN, &element, catalog, 100u),
            "only left-button release can arm association");
    ++element.x;
    Require(!association.ObserveClick(WM_LBUTTONUP, &element, catalog, 100u),
            "rectangle mismatch fails closed");
    element = Element();
    Require(association.ObserveClick(WM_LBUTTONUP, &element, catalog, 100u),
            "one exact control match arms association");
    Require(association.BeginSession(
                1u,
                {LaunchSource::SinglePlayerMap,
                 SessionEligibility::Eligible},
                110u),
            "a known descriptor click binds independently of presentation origin");
    Require(association.PendingDescriptorKey() == "dark-01",
            "the exact admitted descriptor key remains available for session logging");
    Require(!association.ObserveIdentity(
                 {2u, L"display", L"Map\\Campaign\\dark01.map"}, 111u)
                 .has_value(),
            "a different session clears the bound lease");

    association.ObserveSnapshot(Snapshot());
    Require(association.ObserveClick(WM_LBUTTONUP, &element, catalog, 200u),
            "a fresh unique click can arm again");
    association.ObservePage(S4_GUI_UNKNOWN);
    Require(association.BeginSession(
                9u,
                {LaunchSource::Campaign, SessionEligibility::Eligible},
                210u),
            "an armed click survives the briefing-page exit until MapInit");
    Require(!association.ObserveIdentity(
                 {8u, L"RD_PlayerSave", L"Map\\Campaign\\Wrong.map"},
                 220u)
                 .has_value(),
            "session mismatch clears association even with an RD save name");
    Require(!association.ObserveIdentity(
        {9u, L"RD_PlayerSave", L"Map\\Campaign\\Dark\\Mission01.map"},
        230u).has_value(),
            "a cleared session mismatch cannot later be promoted");

    CampaignLaunchAssociation successful;
    successful.ObservePage(kDarkTribeCampaignPage);
    successful.ObserveSnapshot(Snapshot());
    Require(successful.ObserveClick(WM_LBUTTONUP, &element, catalog, 200u) &&
                successful.BeginSession(
                    9u,
                    {LaunchSource::Unknown, SessionEligibility::Unknown},
                    210u),
            "unknown independent origin can bind an exact known descriptor");
    const auto result = successful.ObserveIdentity(
        {9u, L"RD_PlayerSave", L"Map\\Campaign\\Dark\\Mission01.map"}, 230u);
    Require(result.has_value() && result->page == kDarkTribeCampaignPage &&
                result->control.controlId == 2081u && result->sessionId == 9u &&
                result->relative == L"Map\\Campaign\\Dark\\Mission01.map",
            "only the same-session confirmed relative identity is emitted");
    Require(!successful.ObserveIdentity(
                 {9u, L"other", L"Map\\Campaign\\Dark\\Other.map"}, 240u)
                 .has_value(),
            "association is single-use");

    CampaignLaunchAssociation expired;
    expired.ObservePage(kDarkTribeCampaignPage);
    expired.ObserveSnapshot(Snapshot());
    Require(expired.ObserveClick(WM_LBUTTONUP, &element, catalog, 1u),
            "expiry fixture arms");
    Require(!expired.BeginSession(
                10u,
                {LaunchSource::Campaign, SessionEligibility::Eligible},
                1u + kCampaignLaunchAssociationLeaseMs + 1u),
            "expired click cannot bind to a later session");

    CampaignLaunchAssociation left;
    left.ObservePage(kDarkTribeCampaignPage);
    left.ObserveSnapshot(Snapshot());
    Require(left.ObserveClick(WM_LBUTTONUP, &element, catalog, 1u),
            "page-exit fixture arms a candidate");
    left.ObservePage(S4_GUI_UNKNOWN);
    left.ObservePage(kDarkTribeCampaignPage);
    Require(!left.BeginSession(
                11u,
                {LaunchSource::Campaign, SessionEligibility::Eligible}, 2u),
            "returning to page 21 before MapInit clears an abandoned click");

    CampaignLaunchAssociation nonMission;
    auto backSnapshot = Snapshot();
    backSnapshot.features[0].hasText = false;
    backSnapshot.features[0].text = {};
    nonMission.ObservePage(kDarkTribeCampaignPage);
    nonMission.ObserveSnapshot(backSnapshot);
    Require(!nonMission.ObserveClick(WM_LBUTTONUP, &element, catalog, 3u),
            "a textless navigation control cannot arm a mission association");

    CampaignLaunchAssociation replaced;
    replaced.ObservePage(kDarkTribeCampaignPage);
    replaced.ObserveSnapshot(Snapshot());
    Require(replaced.ObserveClick(WM_LBUTTONUP, &element, catalog, 4u),
            "replacement fixture arms a mission click");
    CampaignMenuSnapshot invalidClick{};
    invalidClick.status = CampaignMenuSnapshotStatus::Invalid;
    replaced.ObserveSnapshot(invalidClick);
    Require(!replaced.BeginSession(
                    12u,
                    {LaunchSource::Campaign, SessionEligibility::Eligible},
                    6u),
            "a malformed snapshot clears the pending lease");

    CampaignLaunchAssociation emptyRelative;
    emptyRelative.ObservePage(kDarkTribeCampaignPage);
    emptyRelative.ObserveSnapshot(Snapshot());
    Require(emptyRelative.ObserveClick(WM_LBUTTONUP, &element, catalog, 7u) &&
                emptyRelative.BeginSession(
                    13u,
                    {LaunchSource::Campaign, SessionEligibility::Eligible},
                    8u),
            "empty-relative fixture binds a session");
    Require(!emptyRelative.ObserveIdentity({13u, L"display", L""}, 9u)
                 .has_value() &&
                !emptyRelative.ObserveIdentity(
                     {13u, L"display", L"Map\\Campaign\\late.map"}, 10u)
                     .has_value(),
            "an empty session-bound relative identity clears the lease");

    CampaignLaunchAssociation crossFamily;
    const auto newWorldPage = static_cast<DWORD>(S4_SCREEN_NEWWORLD);
    crossFamily.ObservePage(newWorldPage);
    crossFamily.ObserveSnapshot(Snapshot(newWorldPage));
    Require(!crossFamily.ObserveClick(WM_LBUTTONUP, &element, catalog, 20u),
            "a New World evidence gap cannot arm the common association path");
    crossFamily.ObservePage(S4_SCREEN_MISSIONCD_ROMAN);
    Require(!crossFamily.BeginSession(
                14u,
                {LaunchSource::Campaign, SessionEligibility::Eligible}, 21u),
            "entering another campaign page clears a cross-family candidate");

    CampaignLaunchAssociation newWorld;
    newWorld.ObservePage(newWorldPage);
    newWorld.ObserveSnapshot(Snapshot(newWorldPage));
    Require(!newWorld.ObserveClick(WM_LBUTTONUP, &element, catalog, 30u) &&
                !newWorld.BeginSession(
                    15u, {LaunchSource::Unknown, SessionEligibility::Unknown},
                    31u),
            "a same-page New World evidence gap remains fail closed");

    CampaignLaunchAssociation online;
    online.ObservePage(kDarkTribeCampaignPage);
    online.ObserveSnapshot(Snapshot());
    Require(online.ObserveClick(WM_LBUTTONUP, &element, catalog, 40u) &&
                !online.BeginSession(
                    16u,
                    {LaunchSource::OnlineMultiplayer,
                     SessionEligibility::ExcludedOnlineMultiplayer},
                    41u),
            "an explicitly online origin rejects and clears a known descriptor lease");

    CampaignLaunchAssociation secondClick;
    secondClick.ObservePage(kDarkTribeCampaignPage);
    secondClick.ObserveSnapshot(Snapshot());
    Require(secondClick.ObserveClick(WM_LBUTTONUP, &element, catalog, 50u) &&
                !secondClick.ObserveClick(WM_LBUTTONUP, &element, catalog, 51u) &&
                !secondClick.BeginSession(
                    17u, {LaunchSource::Unknown, SessionEligibility::Unknown},
                    52u),
            "a second click clears instead of replacing the pending lease");

    auto addonSnapshot = Snapshot(S4_SCREEN_ADDON_TROJAN);
    addonSnapshot.features[0].valueLink = 91u;
    addonSnapshot.features[0].x = 320u;
    addonSnapshot.features[0].y = 200u;
    auto addonElement = Element();
    addonElement.id = 91u;
    addonElement.x = 320u;
    addonElement.y = 200u;
    CampaignLaunchAssociation addon;
    addon.ObservePage(S4_SCREEN_ADDON_TROJAN);
    addon.ObserveSnapshot(addonSnapshot);
    Require(addon.ObserveClick(WM_LBUTTONUP, &addonElement, catalog, 60u) &&
                addon.BeginSession(
                    18u, {LaunchSource::Unknown, SessionEligibility::Unknown},
                    61u),
            "the accepted Add-on control binds even when origin is unknown");
    const auto addonResult = addon.ObserveIdentity(
        {18u, L"RD_display_is_not_authority",
         L"Map\\Campaign\\ao_trojan01.map"},
        62u);
    Require(addonResult.has_value() && addonResult->descriptorKey != nullptr &&
                std::string_view(addonResult->descriptorKey) ==
                    "addon-trojan-01" &&
                ValidateCampaignDescriptor(catalog, *addonResult).status ==
                    CampaignDescriptorValidationStatus::Matched,
            "Add-on 91 reaches exact same-session relative validation");

    auto mdSnapshot = Snapshot(S4_SCREEN_MISSIONCD_ROMAN);
    mdSnapshot.features[0].valueLink = 1903u;
    mdSnapshot.features[0].x = 237u;
    mdSnapshot.features[0].y = 148u;
    auto mdElement = Element();
    mdElement.id = 1903u;
    mdElement.x = 237u;
    mdElement.y = 148u;
    CampaignLaunchAssociation mdRoman;
    mdRoman.ObservePage(S4_SCREEN_MISSIONCD_ROMAN);
    mdRoman.ObserveSnapshot(mdSnapshot);
    Require(mdRoman.ObserveClick(WM_LBUTTONUP, &mdElement, catalog, 70u) &&
                mdRoman.BeginSession(
                    19u, {LaunchSource::Unknown, SessionEligibility::Unknown},
                    71u),
            "the closed md Roman descriptor arms the next fresh session");
    const auto mdResult = mdRoman.ObserveIdentity(
        {19u, L"RD_display_is_not_authority",
         L"Map\\Campaign\\md_roman1.map"},
        72u);
    Require(mdResult.has_value() &&
                ValidateCampaignDescriptor(catalog, *mdResult).status ==
                    CampaignDescriptorValidationStatus::Matched,
            "page-16 control 1903 accepts the exact md_roman1 relative");

    auto wrongGeometrySnapshot = mdSnapshot;
    ++wrongGeometrySnapshot.features[0].x;
    ++mdElement.x;
    CampaignLaunchAssociation wrongGeometry;
    wrongGeometry.ObservePage(S4_SCREEN_MISSIONCD_ROMAN);
    wrongGeometry.ObserveSnapshot(wrongGeometrySnapshot);
    Require(!wrongGeometry.ObserveClick(WM_LBUTTONUP, &mdElement, catalog, 80u),
            "matching public snapshot geometry still cannot bypass immutable descriptor geometry");

    return 0;
}
