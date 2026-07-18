#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"

readonly REPOSITORY_URL="https://github.com/nyfrk/S4ModApi.git"
readonly VENDOR_DIR="$CAMPAIGN_MARKER_DIR/research/vendor/S4ModApi"
readonly OUTPUT="$EVIDENCE_DIR/s4modapi-source.txt"

mkdir -p "$(dirname "$VENDOR_DIR")" "$EVIDENCE_DIR"
if [[ -d "$VENDOR_DIR/.git" ]]; then
    git -C "$VENDOR_DIR" fetch --tags --prune origin
    git -C "$VENDOR_DIR" checkout --detach origin/master
else
    git clone --filter=blob:none "$REPOSITORY_URL" "$VENDOR_DIR"
    git -C "$VENDOR_DIR" checkout --detach origin/master
fi

commit="$(git -C "$VENDOR_DIR" rev-parse HEAD)"
release_tag="$(git -C "$VENDOR_DIR" describe --tags --abbrev=0 HEAD 2>/dev/null || printf 'none')"
header="$VENDOR_DIR/S4ModApi/S4ModApi.h"
implementation_header="$VENDOR_DIR/S4ModApi/CSettlers4Api.h"
example="$VENDOR_DIR/Examples/HelloWorld/HelloWorld/dllmain.cpp"
require_readable_file "$header"
require_readable_file "$implementation_header"
require_readable_file "$example"

emit_named_declarations() {
    local pattern
    pattern='S4CreateInterface|S4ApiCreate|AddFrameListener|AddUIFrameListener|AddMapInitListener|AddMouseListener|AddTickListener|AddGuiBltListener|AddGuiElementBltListener|AddGuiClearListener|GetHoveringUiElement|IsCurrentlyOnScreen|CreateCustomUiElement|GetLocalPlayer|HasPlayerLost|GameDefaultGameEndCheck'
    printf '\n## PUBLIC_DECLARATIONS\n'
    local source
    for source in "$header" "$implementation_header"; do
        rg -n -C 2 "$pattern" "$source" | sed "s|$VENDOR_DIR/||g"
    done
}

emit_type_block() {
    local start="$1"
    local end="$2"
    printf '\n## TYPE_BLOCK start=%s end=%s\n' "$start" "$end"
    awk -v start="$start" -v end="$end" '
        index($0, start) { printing=1 }
        printing { print FNR ":" $0 }
        printing && index($0, end) { exit }
    ' "$header"
}

{
    write_evidence_header "Pinned S4ModApi public source audit"
    printf 'repository=%s\n' "$REPOSITORY_URL"
    printf 'commit=%s\n' "$commit"
    printf 'release_tag=%s\n' "$release_tag"

    emit_named_declarations
    emit_type_block 'enum S4_GUI_ENUM' 'S4_GUI_ENUM_MAXVALUE'
    emit_type_block 'typedef struct S4UiElement' '} *LPS4UIELEMENT;'
    emit_type_block 'typedef struct S4GuiElementBltParams' '} *LPS4GUIDRAWBLTPARAMS;'

    printf '\n## MAIN_MENU_UI_EXAMPLE\n'
    rg -n -C 3 'S4_SCREEN_MAINMENU|CreateCustomUiElement' "$example" | sed "s|$VENDOR_DIR/||g"

    printf '\n## IMPLEMENTATION_FILES\n'
    rg -l 'AddUIFrameListener|AddMapInitListener|AddGuiElementBltListener|CreateCustomUiElement|GameDefaultGameEndCheck' \
        "$VENDOR_DIR/S4ModApi" --glob '*.{h,cpp}' | sed "s|$VENDOR_DIR/||g" | LC_ALL=C sort

    printf '\n## ABSENCE_CHECKS\n'
    for api in OnVictory OnGameEnd GetCurrentMapName IsRandomMap IsMultiplayer GetCampaignMenuButtons; do
        if rg -q "\\b${api}\\b" "$header"; then
            printf 'absence_check:%s=declared\n' "$api"
        else
            printf 'absence_check:%s=not_declared\n' "$api"
        fi
    done
} > "$OUTPUT"
