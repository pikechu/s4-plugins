#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"

mkdir -p "$EVIDENCE_DIR"

files=(
    "$GAME_DIR/S4_Main.exe"
    "$GAME_DIR/S4_MainR.pdb"
    "$GAME_DIR/S4ModApi.dll"
    "$GAME_DIR/ddraw.dll"
    "$GAME_DIR/Plugins/SettlersUnited.asi"
    "$GAME_DIR/Plugins/SettlersUnitedLogger.asi"
    "$GAME_DIR/Plugins/S4HD-Patch.asi"
    "$GAME_DIR/Plugins/S4WarriorsLib.asi"
    "$GAME_DIR/Plugins/ExtraZoom.asi"
    "$SU_DIR/resources/bin/s4u.exe"
    "$SU_DIR/resources/bin/s4.dll"
    "$SU_DIR/resources/bin/settlers-united.dll"
)

for path in "${files[@]}"; do
    require_readable_file "$path"
done

printf '%s\n' "${files[@]}" | LC_ALL=C sort | xargs -d '\n' sha256sum \
    > "$EVIDENCE_DIR/manifest.sha256"

normalize_path() {
    sed -e "s|$GAME_DIR|\${GAME_DIR}|g" -e "s|$SU_DIR|\${SU_DIR}|g"
}

windows_version() {
    local path="$1"
    local win_path
    local escaped_path
    win_path="$(wslpath -w "$path")"
    escaped_path="${win_path//\'/\'\'}"
    powershell.exe -NoProfile -Command \
        "\$path='$escaped_path'; \$v=[System.Diagnostics.FileVersionInfo]::GetVersionInfo(\$path); \"FileVersion=\$(\$v.FileVersion);ProductVersion=\$(\$v.ProductVersion);Company=\$(\$v.CompanyName)\"" \
        </dev/null | tr -d '\r'
}

{
    write_evidence_header "Installed PE and loader metadata"
    printf 'input_count=%d\n' "${#files[@]}"

    while IFS= read -r path; do
        name="$(basename "$path")"
        file_desc="$(file -b "$path")"
        format="$(objdump -f "$path" 2>/dev/null | awk '/file format/{print $NF; exit}' || true)"
        version="$(windows_version "$path" 2>/dev/null || printf 'FileVersion=;ProductVersion=;Company=')"
        printf '\n## FILE name=%s path=%s format=%s description=%s %s\n' \
            "$name" "$path" "$format" "$file_desc" "$version"

        objdump -f "$path" 2>/dev/null || true

        while IFS= read -r dependency; do
            printf 'IMPORT file=%s %s\n' "$name" "$dependency"
        done < <(objdump -p "$path" 2>/dev/null | sed -n 's/^[[:space:]]*DLL Name: /DLL Name: /p')

        if [[ "$name" == "S4ModApi.dll" ]]; then
            objdump -p "$path" 2>/dev/null | sed -n '/Export Table/,$p'
        fi
    done < <(printf '%s\n' "${files[@]}" | LC_ALL=C sort)
} | normalize_path > "$EVIDENCE_DIR/pe-metadata.txt"
