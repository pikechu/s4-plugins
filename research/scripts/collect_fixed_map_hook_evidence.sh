#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"

export LC_ALL=C

readonly EXE="$GAME_DIR/S4_Main.exe"
readonly PDB="$GAME_DIR/S4_MainR.pdb"
readonly S4MODAPI_DLL="$GAME_DIR/S4ModApi.dll"
readonly S4MODAPI_HEADER="$WORKSPACE_DIR/third_party/S4ModApi/S4ModApi.h"
readonly S4MODAPI_LIB="$WORKSPACE_DIR/third_party/S4ModApi/S4ModApi.lib"
readonly MANIFEST="$EVIDENCE_DIR/manifest.sha256"
readonly OUTPUT="$EVIDENCE_DIR/fixed-map-hook-site.txt"

readonly APPROVED_EXE_SHA256="3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816"
readonly APPROVED_PDB_SHA256="702df42ef4d7e8f6ba39aee96b5c83780c3266a7e9a174f81db13c6934050ae6"
readonly APPROVED_S4MODAPI_DLL_SHA256="ed111491b1a6bd2822ccd21fec31adedfb35b6bf311cd568d9ff0e6e2193126f"
readonly APPROVED_S4MODAPI_HEADER_SHA256="31069630faffea89d21359bdc001261fbf4eef3307b7ee900be4d44934a835d5"
readonly APPROVED_S4MODAPI_LIB_SHA256="96025df625b7b718737b261278459e28932e7b107cc5c90cd2ecf32df1577a62"

readonly CALL_SITE_VMA=$((0x004fefa5))
readonly CALL_SITE_RVA=$((0x000fefa5))
readonly CALL_SITE_FILE_OFFSET=$((0x000fe3a5))
readonly CALL_NEXT_VMA=$((0x004fefaa))
readonly ORIGINAL_TARGET_VMA=$((0x004b1310))
readonly ORIGINAL_TARGET_RVA=$((0x000b1310))
readonly EXPECTED_CALL_BYTES="e8 66 23 fb ff"

for path in "$EXE" "$PDB" "$S4MODAPI_DLL" "$S4MODAPI_HEADER" \
    "$S4MODAPI_LIB" "$MANIFEST"; do
    require_readable_file "$path"
done
for command_name in objdump od sha256sum sed grep awk git; do
    command -v "$command_name" >/dev/null || {
        printf 'Required command missing: %s\n' "$command_name" >&2
        exit 1
    }
done

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
verify_hash "$S4MODAPI_DLL" "$APPROVED_S4MODAPI_DLL_SHA256" S4ModApi.dll
verify_hash "$S4MODAPI_HEADER" "$APPROVED_S4MODAPI_HEADER_SHA256" S4ModApi.h
verify_hash "$S4MODAPI_LIB" "$APPROVED_S4MODAPI_LIB_SHA256" S4ModApi.lib
verify_manifest_entry "$EXE" "$APPROVED_EXE_SHA256"
verify_manifest_entry "$PDB" "$APPROVED_PDB_SHA256"
verify_manifest_entry "$S4MODAPI_DLL" "$APPROVED_S4MODAPI_DLL_SHA256"

actual_call_bytes="$(
    od -An -v -tx1 -j "$CALL_SITE_FILE_OFFSET" -N 5 "$EXE" |
        awk '{$1=$1; print}'
)"
[[ "$actual_call_bytes" == "$EXPECTED_CALL_BYTES" ]] || {
    printf 'Call-site bytes mismatch: %s\n' "$actual_call_bytes" >&2
    exit 1
}

rel32_unsigned=$((0xfffb2366))
rel32_signed="$rel32_unsigned"
if ((rel32_signed & 0x80000000)); then
    rel32_signed=$((rel32_signed - 0x100000000))
fi
decoded_target=$((CALL_NEXT_VMA + rel32_signed))
[[ "$decoded_target" -eq "$ORIGINAL_TARGET_VMA" ]] || {
    printf 'Decoded call target mismatch: 0x%08x\n' "$decoded_target" >&2
    exit 1
}

exports="$(objdump -p "$S4MODAPI_DLL")"
for symbol in \
    '??0CallPatch@hlib@@QAE@_KKPBUBYTE5@Patch@1@I@Z' \
    '?patch@AbstractPatch@hlib@@QAE_NXZ' \
    '?unpatch@AbstractPatch@hlib@@QAE_NXZ' \
    '?update@AbstractPatch@hlib@@QAE_NXZ' \
    '?isPatched@AbstractPatch@hlib@@QBE_NXZ'; do
    grep -F "$symbol" <<<"$exports" >/dev/null || {
        printf 'Required S4ModApi export missing: %s\n' "$symbol" >&2
        exit 1
    }
done

emit_disassembly() {
    local label="$1"
    local start="$2"
    local stop="$3"

    printf '\n## disassembly %s vma=%s..%s\n' "$label" "$start" "$stop"
    objdump -d -Mintel --start-address="$start" --stop-address="$stop" "$EXE"
}

tmp="$(mktemp "$EVIDENCE_DIR/.fixed-map-hook-site.XXXXXX")"
trap 'rm -f "$tmp"' EXIT

{
    write_evidence_header 'Phase 2.3 fixed-map single-call Hook evidence'
    printf 'exe_sha256=%s\n' "$APPROVED_EXE_SHA256"
    printf 'pdb_sha256=%s\n' "$APPROVED_PDB_SHA256"
    printf 's4modapi_dll_sha256=%s\n' "$APPROVED_S4MODAPI_DLL_SHA256"
    printf 's4modapi_header_sha256=%s\n' "$APPROVED_S4MODAPI_HEADER_SHA256"
    printf 's4modapi_lib_sha256=%s\n' "$APPROVED_S4MODAPI_LIB_SHA256"
    printf 'image_base=0x00400000\n'
    printf 'call_site_vma=0x%08x\n' "$CALL_SITE_VMA"
    printf 'call_site_rva=0x%08x\n' "$CALL_SITE_RVA"
    printf 'call_site_file_offset=0x%08x\n' "$CALL_SITE_FILE_OFFSET"
    printf 'call_next_vma=0x%08x\n' "$CALL_NEXT_VMA"
    printf 'call_bytes=%s\n' "$actual_call_bytes"
    printf 'rel32_unsigned=0x%08x\n' "$rel32_unsigned"
    printf 'rel32_signed=%d\n' "$rel32_signed"
    printf 'decoded_target_vma=0x%08x\n' "$decoded_target"
    printf 'decoded_target_rva=0x%08x\n' "$ORIGINAL_TARGET_RVA"
    printf 'callee_stack_cleanup=0x0000000c\n'

    printf '\n## PE sections\n'
    objdump -h "$EXE"

    printf '\n## exact call-site file bytes\n'
    od -An -v -tx1 -j "$CALL_SITE_FILE_OFFSET" -N 5 "$EXE"

    emit_disassembly 'fixed-map caller ABI' 0x004fef8f 0x004fefb0
    emit_disassembly 'CMapFile load ABI and path layout' 0x004b1310 0x004b1460
    emit_disassembly 'x86 MSVC wide-string length capacity and storage' 0x00458150 0x00458250
    emit_disassembly 'converted path construction' 0x004fef21 0x004fefa6

    printf '\n## S4ModApi hlib declarations\n'
    sed -n '1052,1138p' "$S4MODAPI_HEADER"

    printf '\n## required S4ModApi exports\n'
    grep -F -e '??0CallPatch@hlib@@QAE@_KKPBUBYTE5@Patch@1@I@Z' \
        -e '?patch@AbstractPatch@hlib@@QAE_NXZ' \
        -e '?unpatch@AbstractPatch@hlib@@QAE_NXZ' \
        -e '?update@AbstractPatch@hlib@@QAE_NXZ' \
        -e '?isPatched@AbstractPatch@hlib@@QBE_NXZ' <<<"$exports"
} | sed 's/[[:space:]]\+$//' >"$tmp"

mv "$tmp" "$OUTPUT"
trap - EXIT
printf 'Wrote %s\n' "$OUTPUT"
