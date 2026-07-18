#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"

export LC_ALL=C

readonly EXE="$GAME_DIR/S4_Main.exe"
readonly PDB="$GAME_DIR/S4_MainR.pdb"
readonly SYMBOLS="$EVIDENCE_DIR/pdb-symbols.txt"
readonly MANIFEST="$EVIDENCE_DIR/manifest.sha256"
readonly OUTPUT="$EVIDENCE_DIR/map-identity-candidates.txt"
readonly APPROVED_EXE_SHA256="3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816"
readonly APPROVED_PDB_SHA256="702df42ef4d7e8f6ba39aee96b5c83780c3266a7e9a174f81db13c6934050ae6"
readonly APPROVED_SYMBOLS_SHA256="dafc72d237209e3c332f44dca44e9fcd24eb49e8b667172ff2eda695c2bc247f"

require_readable_file "$EXE"
require_readable_file "$PDB"
require_readable_file "$SYMBOLS"
require_readable_file "$MANIFEST"
command -v objdump >/dev/null || {
    printf 'Required command missing: objdump\n' >&2
    exit 1
}

verify_hash() {
    local path="$1"
    local approved="$2"
    local label="$3"
    local actual

    actual="$(sha256sum "$path" | awk '{print $1}')"
    [[ "$actual" == "$approved" ]] || {
        printf '%s hash mismatch: %s\n' "$label" "$actual" >&2
        exit 1
    }
}

verify_manifest_entry() {
    local path="$1"
    local approved="$2"

    grep -F "$approved  $path" "$MANIFEST" >/dev/null || {
        printf 'Approved manifest entry missing: %s\n' "$path" >&2
        exit 1
    }
}

verify_hash "$EXE" "$APPROVED_EXE_SHA256" EXE
verify_hash "$PDB" "$APPROVED_PDB_SHA256" PDB
verify_hash "$SYMBOLS" "$APPROVED_SYMBOLS_SHA256" symbols
verify_manifest_entry "$EXE" "$APPROVED_EXE_SHA256"
verify_manifest_entry "$PDB" "$APPROVED_PDB_SHA256"

tmp="$(mktemp "$EVIDENCE_DIR/.map-identity-candidates.XXXXXX")"
trap 'rm -f "$tmp"' EXIT

emit_pdb_record() {
    local pattern="$1"

    grep -F -B 2 -A 2 -- "$pattern" "$SYMBOLS" || {
        printf 'Required PDB record missing: %s\n' "$pattern" >&2
        exit 1
    }
}

emit_disassembly() {
    local label="$1"
    local start="$2"
    local stop="$3"

    printf '\n## disassembly %s vma=%s..%s\n' "$label" "$start" "$stop"
    objdump -d -Mintel --start-address="$start" --stop-address="$stop" "$EXE"
}

{
    write_evidence_header 'Phase 2.2 map identity static candidates'
    printf 'exe_sha256=%s\n' "$APPROVED_EXE_SHA256"
    printf 'pdb_sha256=%s\n' "$APPROVED_PDB_SHA256"
    printf 'pdb_symbols_sha256=%s\n' "$APPROVED_SYMBOLS_SHA256"
    printf 'image_base=0x00400000\n'
    printf 'conversion=RVA=section.VirtualAddress+CodeViewOffset; VMA=image_base+RVA\n'

    printf '\n## PE sections\n'
    objdump -h "$EXE"
    printf '\n## raw PE section headers (name, VirtualSize, VirtualAddress, raw size, raw offset)\n'
    od -An -tx4 -j 648 -N 320 "$EXE"
    printf 'text_virtual_rva=0x00001000..0x00B14EF0\n'
    printf 'rdata_virtual_rva=0x00B15000..0x00D4503A\n'
    printf 'data_virtual_rva=0x00D46000..0x01208F40\n'

    printf '\n## exact PDB public records\n'
    emit_pdb_record 'S4::CMapFile::sub_B11B0'
    emit_pdb_record '??_7CMapFile@S4@@6B@'
    emit_pdb_record 'SetupSingleplayerLobbyMenu'
    emit_pdb_record 'CStateLobbyMapSettings::sub_120120'
    emit_pdb_record 'CStateLobbyMapSettings::sub_120C10'
    emit_pdb_record 'CStateLobbyMapSettings::Callbacks`'

    printf '\n## reviewed address conversions\n'
    printf 'CMapFile_sub_B11B0 cv=0001:0x000B01B0 rva=0x000B11B0 vma=0x004B11B0 section=.text\n'
    printf 'CMapFile_vtable cv=0002:0x00118A98 rva=0x00C2DA98 vma=0x0102DA98 section=.rdata\n'
    printf 'SetupSingleplayerLobbyMenu cv=0001:0x0011B1A0 rva=0x0011C1A0 vma=0x0051C1A0 section=.text\n'
    printf 'CStateLobbyMapSettings_sub_120120 cv=0001:0x0011F120 rva=0x00120120 vma=0x00520120 section=.text\n'
    printf 'CStateLobbyMapSettings_sub_120C10 cv=0001:0x0011FC10 rva=0x00120C10 vma=0x00520C10 section=.text\n'
    printf 'CStateLobbyMapSettings_Callbacks cv=0001:0x0011FC90 rva=0x00120C90 vma=0x00520C90 section=.text\n'
    printf 'game_settings_root_pointer rva=0x00E968A0 vma=0x012968A0 section=.data\n'
    printf 'map_list_array rva=0x00E97848 vma=0x01297848 section=.data\n'
    printf 'selected_list_index rva=0x0109C1EC vma=0x0149C1EC section=.data\n'
    printf 'menu_map_wstring rva=0x0109C21C vma=0x0149C21C section=.data\n'
    printf 'random_maps_object rva=0x00DCDA70 vma=0x011CDA70 section=.data\n'

    printf '\n## CMapFile vtable bytes\n'
    objdump -s --start-address=0x0102da90 --stop-address=0x0102dab0 "$EXE"
    printf '\n## CRandomMaps RTTI complete-object locator and type descriptor\n'
    objdump -s --start-address=0x01055970 --stop-address=0x010559a0 "$EXE"
    objdump -s --start-address=0x010a00d0 --stop-address=0x010a00f0 "$EXE"
    objdump -s --start-address=0x012244f8 --stop-address=0x01224540 "$EXE"

    emit_disassembly 'CMapFile constructor, destructor, and load' 0x004b10d0 0x004b1460
    emit_disassembly 'persistent game-settings constructor' 0x00506020 0x00506600
    emit_disassembly 'SetupSingleplayerLobbyMenu' 0x0051c1a0 0x0051c6d0
    emit_disassembly 'fixed-map list construction' 0x005201e0 0x005207e0
    emit_disassembly 'fixed-map list destruction' 0x00520bc0 0x00520c10
    emit_disassembly 'fixed-map list selection' 0x00521540 0x005218f0
    emit_disassembly 'map-settings public callback' 0x00520c90 0x00521420
    emit_disassembly 'single-player lobby start path' 0x00526000 0x00526c20
    emit_disassembly 'CMapFile fixed-map load wrapper' 0x004feef0 0x004ff440
    emit_disassembly 'CRandomMaps CMapFile owner (out of scope)' 0x0050b100 0x0050b2b0
} | sed 's/[[:space:]]\+$//' >"$tmp"

mv "$tmp" "$OUTPUT"
trap - EXIT
printf 'Wrote %s\n' "$OUTPUT"
