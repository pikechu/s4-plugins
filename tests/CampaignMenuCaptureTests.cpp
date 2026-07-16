#include "campaign/CampaignMenuCapture.h"

#include <array>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

campaign_completion::CampaignMenuFeature Feature(WORD id, WORD x = 10u) {
    campaign_completion::CampaignMenuFeature feature{};
    feature.valueLink = id;
    feature.x = x;
    feature.y = 20u;
    feature.width = 100u;
    feature.height = 24u;
    feature.mainTexture = 31u;
    return feature;
}

}  // namespace

int RunCampaignMenuCaptureTests() {
    using namespace campaign_completion;

    constexpr std::array<std::pair<DWORD, WORD>, 27u> admittedControls{{
        {S4_SCREEN_MISSIONCD, 1919u},
        {S4_SCREEN_NEWWORLD2, 1825u},
        {S4_SCREEN_NEWWORLD2, 1844u},
        {S4_SCREEN_NEWWORLD2, 2939u},
        {S4_SCREEN_NEWWORLD2, 2954u},
        {S4_SCREEN_ADDON_TROJAN, 91u},
        {S4_SCREEN_ADDON_TROJAN, 102u},
        {S4_SCREEN_ADDON_ROMAN, 47u},
        {S4_SCREEN_ADDON_ROMAN, 50u},
        {S4_SCREEN_ADDON_MAYAN, 34u},
        {S4_SCREEN_ADDON_MAYAN, 37u},
        {S4_SCREEN_ADDON_VIKING, 112u},
        {S4_SCREEN_ADDON_VIKING, 115u},
        {S4_SCREEN_ADDON_SETTLE, 77u},
        {S4_SCREEN_ADDON_SETTLE, 80u},
        {S4_SCREEN_MISSIONCD_ROMAN, 1903u},
        {S4_SCREEN_MISSIONCD_ROMAN, 1907u},
        {S4_SCREEN_MISSIONCD_VIKING, 1950u},
        {S4_SCREEN_MISSIONCD_VIKING, 1954u},
        {S4_SCREEN_MISSIONCD_MAYAN, 1889u},
        {S4_SCREEN_MISSIONCD_MAYAN, 1893u},
        {S4_SCREEN_MISSIONCD_CONFL, 1941u},
        {S4_SCREEN_MISSIONCD_CONFL, 1946u},
        {S4_SCREEN_SINGLEPLAYER_CAMPAIGN, 2038u},
        {S4_SCREEN_SINGLEPLAYER_CAMPAIGN, 2046u},
        {S4_SCREEN_SINGLEPLAYER_DARKTRIBE, 2081u},
        {S4_SCREEN_SINGLEPLAYER_DARKTRIBE, 2092u},
    }};
    for (const auto& [page, control] : admittedControls) {
        Require(IsPhase6DCampaignMissionControl(page, control),
                "every Phase 6D mission-control boundary is admitted");
    }
    for (const auto rejected :
         {std::pair<DWORD, WORD>{S4_SCREEN_MISSIONCD, 1925u},
          {S4_SCREEN_NEWWORLD2, 2938u},
          {S4_SCREEN_SINGLEPLAYER_CAMPAIGN, 2037u},
          {S4_SCREEN_SINGLEPLAYER_DARKTRIBE, 2080u},
          {S4_SCREEN_ADDON_ROMAN, 46u}}) {
        Require(!IsPhase6DCampaignMissionControl(rejected.first,
                                                  rejected.second),
                "navigation and neighboring controls stay outside the gap catalog");
    }

    std::array<bool, kCampaignCatalogPages.size()> activePages{};
    Require(SelectCampaignCatalogOwner(activePages) == S4_GUI_UNKNOWN,
            "no active campaign page has no catalog owner");
    activePages[0] = true;
    Require(SelectCampaignCatalogOwner(activePages) == S4_SCREEN_ADDON,
            "an Add-on selector owns its composed public layers");
    activePages[4] = true;
    Require(SelectCampaignCatalogOwner(activePages) == S4_SCREEN_ADDON_TROJAN,
            "a child campaign page outranks its selector during transition");
    activePages = {};
    activePages[2] = true;
    activePages[3] = true;
    Require(SelectCampaignCatalogOwner(activePages) == S4_SCREEN_NEWWORLD2,
            "the stable 5/6 composite uses one deterministic owner");

    for (const DWORD page : {3u, 4u, 5u, 6u, 11u, 12u, 13u, 14u,
                             15u, 16u, 17u, 18u, 19u, 20u, 21u}) {
        Require(IsCampaignCatalogPage(page),
                "every approved campaign catalog page is admitted");
    }
    for (const DWORD page :
         {0u, 1u, 2u, 7u, 10u, 22u,
          static_cast<unsigned int>(S4_GUI_ENUM_MAXVALUE)}) {
        Require(!IsCampaignCatalogPage(page),
                "non-campaign and out-of-range pages are rejected");
    }

    CampaignMenuCapture capture;
    Require(!capture.ObserveFeature(Feature(1u)),
            "features outside an admitted epoch are ignored");
    Require(!capture.ObserveFrame(7u, true).has_value() && !capture.Active(),
            "a non-campaign page cannot open capture");
    Require(!capture.ObserveFrame(20u, true).has_value() && capture.Active() &&
                capture.ActivePage() == 20u,
            "an approved original-campaign page opens capture");
    capture.ObserveFrame(20u, false);
    Require(!capture.ObserveFrame(kDarkTribeCampaignPage, true).has_value() &&
                capture.Active() && capture.ActivePage() == 21u,
            "the first admitted Dark Tribe frame opens capture");

    CampaignMenuCapture entryCapture;
    entryCapture.SynchronizePage(S4_SCREEN_ADDON_ROMAN, true);
    Require(entryCapture.Active() &&
                entryCapture.ActivePage() == S4_SCREEN_ADDON_ROMAN,
            "a GUI callback can prime the owner before the first UI-frame callback");
    for (WORD control = 47u; control <= 50u; ++control) {
        Require(entryCapture.ObserveFeature(Feature(control)),
                "the initial full redraw is retained without hover");
    }
    const auto entrySnapshot =
        entryCapture.ObserveFrame(S4_SCREEN_ADDON_ROMAN, true);
    Require(entrySnapshot.has_value() && entrySnapshot->count == 4u &&
                entrySnapshot->features[0].valueLink == 47u &&
                entrySnapshot->features[3].valueLink == 50u,
            "the first UI frame publishes the complete pre-frame redraw");

    char label[] = "Mission One";
    S4GuiElementBltParams raw{};
    raw.surfaceWidth = 1024u;
    raw.surfaceHeight = 768u;
    raw.currentGFXCollection = 7u;
    raw.containerType = 9u;
    raw.x = 12u;
    raw.y = 34u;
    raw.width = 120u;
    raw.height = 30u;
    raw.mainTexture = 42u;
    raw.valueLink = 600u;
    raw.buttonPressedTexture = 43u;
    raw.tooltipLink = 44u;
    raw.tooltipLinkExtra = 45u;
    raw.imageStyle = S4_UI_TYPE::MAP;
    raw.effects = S4_UI_EFFECTS::NONE;
    raw.textStyle = S4_UI_TEXTSTYLE::SMALL_WHITE;
    raw.showTexture = 46u;
    raw.backTexture = 47u;
    raw.text = label;
    CampaignMenuFeature copied{};
    Require(CopyCampaignMenuFeature(&raw, copied),
            "public GUI fields copy into owned storage");
    label[0] = 'X';
    Require(copied.hasText && copied.text.length == 11u &&
                std::string_view(copied.text.bytes.data(), copied.text.length) ==
                    "Mission One",
            "copied text is pointer-free and survives source mutation");
    Require(capture.ObserveFeature(copied) && capture.ObserveFeature(copied),
            "identical duplicate callbacks deduplicate");
    const auto snapshot =
        capture.ObserveFrame(kDarkTribeCampaignPage, true);
    Require(snapshot.has_value() &&
                snapshot->status == CampaignMenuSnapshotStatus::Success &&
                snapshot->page == kDarkTribeCampaignPage &&
                snapshot->count == 1u &&
                EqualCampaignMenuFeature(snapshot->features[0], copied),
            "a complete admitted epoch publishes one deterministic feature");
    for (int frame = 0; frame < 100; ++frame) {
        Require(!capture.ObserveFrame(kDarkTribeCampaignPage, true).has_value(),
                "empty callback intervals retain the page-residency cache");
    }
    auto hovered = copied;
    hovered.effects = static_cast<std::uint16_t>(S4_UI_EFFECTS::HOVER);
    Require(capture.ObserveFeature(hovered),
            "a volatile effects transition updates the cached control");
    const auto hoverSnapshot =
        capture.ObserveFrame(kDarkTribeCampaignPage, true);
    Require(hoverSnapshot.has_value() &&
                hoverSnapshot->status == CampaignMenuSnapshotStatus::Success &&
                hoverSnapshot->count == 1u &&
                EqualCampaignMenuFeature(hoverSnapshot->features[0], hovered),
            "the sparse cache republishes only the updated volatile state");

    capture.ObserveFrame(kDarkTribeCampaignPage, false);
    Require(!capture.Active() && !capture.ObserveFeature(Feature(2u)),
            "leaving page 21 clears and closes capture");
    Require(!capture.ObserveFrame(kDarkTribeCampaignPage, true).has_value(),
            "re-entering starts with an empty cache rather than stale features");

    CampaignMenuCapture ambiguous;
    ambiguous.ObserveFrame(kDarkTribeCampaignPage, true);
    Require(ambiguous.ObserveFeature(Feature(3u)),
            "ambiguous-page fixture admits a feature");
    Require(!ambiguous.ObserveFrame(20u, true).has_value() &&
                ambiguous.Active() && ambiguous.ActivePage() == 20u,
            "switching campaign pages clears the old page cache");
    Require(!ambiguous.ObserveFrame(kDarkTribeCampaignPage, true).has_value(),
            "a later valid page visit cannot recover stale ambiguous data");

    CampaignMenuCapture allCampaigns;
    Require(!allCampaigns.ObserveFrame(S4_SCREEN_ADDON_ROMAN, true).has_value(),
            "an Add-on child page opens an independent sparse cache");
    Require(allCampaigns.ObserveFeature(Feature(1201u)),
            "the Add-on page retains its public feature");
    const auto addOn =
        allCampaigns.ObserveFrame(S4_SCREEN_ADDON_ROMAN, true);
    Require(addOn.has_value() && addOn->page == S4_SCREEN_ADDON_ROMAN &&
                addOn->count == 1u,
            "an Add-on page publishes a page-typed snapshot");
    Require(!allCampaigns.ObserveFrame(S4_SCREEN_MISSIONCD_ROMAN, true)
                 .has_value() &&
                allCampaigns.ActivePage() == S4_SCREEN_MISSIONCD_ROMAN,
            "a Mission CD page begins empty rather than inheriting Add-on data");
    Require(!allCampaigns.ObserveFrame(S4_SCREEN_MISSIONCD_ROMAN, true)
                 .has_value(),
            "an empty new page cannot republish the previous page catalog");

    CampaignMenuCapture conflict;
    conflict.ObserveFrame(kDarkTribeCampaignPage, true);
    auto first = Feature(10u);
    auto second = first;
    second.mainTexture = 99u;
    Require(conflict.ObserveFeature(first) &&
                !conflict.ObserveFeature(second),
            "a control-key collision invalidates the epoch");
    const auto invalid =
        conflict.ObserveFrame(kDarkTribeCampaignPage, true);
    Require(invalid.has_value() &&
                invalid->status == CampaignMenuSnapshotStatus::Invalid &&
                invalid->count == 0u,
            "a conflicted epoch fails closed");

    CampaignMenuCapture overflow;
    overflow.ObserveFrame(kDarkTribeCampaignPage, true);
    for (std::size_t index = 0u; index < kMaximumCampaignMenuFeatures; ++index) {
        Require(overflow.ObserveFeature(
                    Feature(static_cast<WORD>(index + 1u),
                            static_cast<WORD>(index + 1u))),
                "features up to the fixed bound are accepted");
    }
    Require(!overflow.ObserveFeature(Feature(500u, 500u)),
            "capacity overflow invalidates instead of truncating");
    const auto overflowed =
        overflow.ObserveFrame(kDarkTribeCampaignPage, true);
    Require(overflowed.has_value() &&
                overflowed->status == CampaignMenuSnapshotStatus::Invalid,
            "overflow publishes only an invalid diagnostic snapshot");

    return 0;
}
