#!/usr/bin/env bash
set -euo pipefail

readonly WORKSPACE_DIR="/mnt/f/claude projects/thesettler4plugin"
readonly CAMPAIGN_MARKER_DIR="$WORKSPACE_DIR/CampaignMarker"
readonly GAME_DIR="/mnt/f/Program Files (x86)/Ubisoft/Ubisoft Game Launcher/games/thesettlers4"
readonly SU_DIR="/mnt/c/Program Files/Settlers United"
readonly EVIDENCE_DIR="$CAMPAIGN_MARKER_DIR/research/evidence"

require_readable_file() {
    local path="$1"
    [[ -f "$path" && -r "$path" ]] || {
        printf 'Required readable file missing: %s\n' "$path" >&2
        return 1
    }
}

write_evidence_header() {
    local title="$1"
    printf '# %s\n' "$title"
    printf 'generated_utc=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    printf 'workspace_commit=%s\n' "$(git -C "$WORKSPACE_DIR" rev-parse HEAD)"
}
