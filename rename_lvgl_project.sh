#!/usr/bin/env bash
set -euo pipefail

# ---------------------------------------------------------
# Generic LVGL Project Renamer (macOS-compatible)
#
# This script renames all occurrences of an old project name
# to a new project name in:
#   - file contents
#   - file and directory names
#
# It respects different naming styles:
#   - lowercase:   filamentsilicatdryer_480x480
#   - UPPERCASE:   FILAMENTSILICATDRYER_480X480
#   - CamelCase:   Filamentsilicatdryer480x480
#
# Usage:
#   ./rename_lvgl_project.sh OLD_NAME NEW_NAME [TARGET_DIR]
#
# Example:
#   ./rename_lvgl_project.sh filamentsilicatdryer_480x480 filamentsilicatdryer_480x480 .
# ---------------------------------------------------------

show_help() {
    echo
    echo "Generic LVGL Project Renamer"
    echo "-----------------------------------------"
    echo "Usage:  $0 OLD_NAME NEW_NAME [TARGET_DIR]"
    echo
    echo "Arguments:"
    echo "  OLD_NAME     Existing project name token to search for."
    echo "               Example: filamentsilicatdryer_480x480"
    echo
    echo "  NEW_NAME     New project name token to replace with."
    echo "               Example: filamentsilicatdryer_480x480"
    echo
    echo "  TARGET_DIR   Optional. Project root directory (default: current folder '.')"
    echo
    echo "Examples:"
    echo "  $0 filamentsilicatdryer_480x480 filamentsilicatdryer_480x480"
    echo "  $0 filamentsilicatdryer_480x480 filamentsilicatdryer_480x480 ."
    echo "  $0 filamentsilicatdryer_480x480 filamentsilicatdryer_480x480 /path/to/project"
    echo
    echo "Help:"
    echo "  $0 --help"
    echo "  $0 -h"
    echo
    exit 0
}

# --- help / usage checks ---
if [[ $# -eq 0 || $# -gt 3 ]]; then
    show_help
fi

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    show_help
fi

if [[ $# -lt 2 ]]; then
    show_help
fi

OLD_RAW="$1"
NEW_RAW="$2"
TARGET_DIR="${3:-.}"

# --- helper: build CamelCase from name with _ or - separators ---
to_camel() {
    local s="$1"
    local out=""
    local IFS='_'
    local part

    # normalize: lowercase and treat '-' as '_'
    s="$(echo "$s" | tr '[:upper:]' '[:lower:]')"
    s="${s//-/_}"

    # split into parts by underscore
    set -f  # disable globbing
    set -- $s
    for part in "$@"; do
        [ -z "$part" ] && continue

        # first character upper, rest unchanged
        local first rest
        first=$(printf '%s' "$part" | cut -c1 | tr '[:lower:]' '[:upper:]')
        rest=$(printf '%s' "$part" | cut -c2-)
        out="${out}${first}${rest}"
    done
    set +f

    echo "$out"
}

# --- derive style variants ---
old_lower="$(echo "$OLD_RAW" | tr '[:upper:]' '[:lower:]')"
old_upper="$(echo "$OLD_RAW" | tr '[:lower:]' '[:upper:]')"
old_camel="$(to_camel "$OLD_RAW")"

new_lower="$(echo "$NEW_RAW" | tr '[:upper:]' '[:lower:]')"
new_upper="$(echo "$NEW_RAW" | tr '[:lower:]' '[:upper:]')"
new_camel="$(to_camel "$NEW_RAW")"

echo "Old name (raw)     : $OLD_RAW"
echo "  lower            : $old_lower"
echo "  UPPER            : $old_upper"
echo "  CamelCase        : $old_camel"
echo
echo "New name (raw)     : $NEW_RAW"
echo "  lower            : $new_lower"
echo "  UPPER            : $new_upper"
echo "  CamelCase        : $new_camel"
echo
echo "Target directory   : $TARGET_DIR"
echo

read -r -p "Proceed with renaming? [y/N] " answer
case "$answer" in
    [Yy]* ) echo "Starting rename...";;
    * ) echo "Aborted."; exit 1;;
esac

# ---------------------------------------------------------
# Step 1: Replace occurrences inside files
# ---------------------------------------------------------
echo
echo "Step 1/2: Replacing occurrences inside files..."

find "$TARGET_DIR" \
    -type f \
    ! -path "*/.git/*" \
    ! -path "*/build/*" \
    ! -path "*/cmake-build-*/*" \
    -print0 \
  | while IFS= read -r -d '' file; do
        # only touch if any of the variants is present
        if grep -q "$old_lower" "$file" 2>/dev/null \
        || grep -q "$old_upper" "$file" 2>/dev/null \
        || grep -q "$old_camel" "$file" 2>/dev/null; then
            sed -i.bak \
                -e "s/$old_upper/$new_upper/g" \
                -e "s/$old_camel/$new_camel/g" \
                -e "s/$old_lower/$new_lower/g" \
                "$file"
            rm -f "${file}.bak"
            echo "  patched: $file"
        fi
    done

echo "Done replacing inside files."

# ---------------------------------------------------------
# Step 2: Rename files and directories
# ---------------------------------------------------------
echo
echo "Step 2/2: Renaming files and directories..."

find "$TARGET_DIR" \
    -depth \
    ! -path "*/.git/*" \
    ! -path "*/build/*" \
    ! -path "*/cmake-build-*/*" \
    -print0 \
  | while IFS= read -r -d '' path; do
        new_path="$path"
        # apply all three style variants on the path
        new_path="${new_path//$old_upper/$new_upper}"
        new_path="${new_path//$old_camel/$new_camel}"
        new_path="${new_path//$old_lower/$new_lower}"

        if [[ "$path" != "$new_path" ]]; then
            echo "  mv: $path -> $new_path"
            mv "$path" "$new_path"
        fi
    done

echo
echo "Renaming finished."
echo "Please verify that your project builds and opens correctly in lvgl_editor."
