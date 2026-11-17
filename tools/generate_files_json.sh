#!/usr/bin/env bash
# ==============================================================================
# Script: generate_files_json.sh
# Purpose: Scan game assets directory and generate files.json for Cloudflare R2
# ==============================================================================

set -e

TARGET_DIR="${1:-.}"
OUTPUT_FILE="${2:-files.json}"

if [ ! -d "$TARGET_DIR" ]; then
    echo "Error: Directory '$TARGET_DIR' does not exist."
    echo "Usage: ./generate_files_json.sh [directory_path] [output_files.json]"
    exit 1
fi

# Convert OUTPUT_FILE to absolute path
case "$OUTPUT_FILE" in
    /*) ABS_OUTPUT="$OUTPUT_FILE" ;;
    *)  ABS_OUTPUT="$(pwd)/$OUTPUT_FILE" ;;
esac

echo "Scanning files in: $TARGET_DIR"
echo "Generating manifest: $ABS_OUTPUT"

TEMP_JSON=$(mktemp)
echo '{"files": [' > "$TEMP_JSON"

FIRST=1
COUNT=0
TOTAL_BYTES=0

# Exclude temporary, cache, config or git files
EXCLUDE_REGEX='(\.git|\.tmp$|\.bak$|\.log$|files\.json$|samp_log\.txt|svlog\.txt|startup\.log|\.asset_sync_v1|gtasatelem\.set|gta_sa\.set|settings\.ini|GTASAMP10\.b|GTASAsf10\.b|\.DS_Store|Thumbs\.db|^dexopt|/dexopt)'

cd "$TARGET_DIR"

while IFS= read -r -d '' file; do
    # Remove leading ./
    rel_path="${file#./}"

    # Filter out excluded files
    if echo "$rel_path" | grep -qE "$EXCLUDE_REGEX"; then
        continue
    fi

    # Check if it is a regular file
    if [ -f "$rel_path" ]; then
        file_name=$(basename "$rel_path")
        file_size=$(stat -c %s "$rel_path" 2>/dev/null || wc -c < "$rel_path" | tr -d ' ')
        file_md5=$(md5sum "$rel_path" 2>/dev/null | awk '{print $1}' || echo "")

        TOTAL_BYTES=$((TOTAL_BYTES + file_size))
        COUNT=$((COUNT + 1))

        if [ $FIRST -eq 0 ]; then
            echo "," >> "$TEMP_JSON"
        fi
        FIRST=0

        # Write file entry
        printf '    {"name": "%s", "size": %s, "path": "%s", "url": "%s", "md5": "%s"}' \
            "$file_name" "$file_size" "$rel_path" "$rel_path" "$file_md5" >> "$TEMP_JSON"
    fi
done < <(find . -type f -print0 | sort -z)

echo "" >> "$TEMP_JSON"
echo ']}' >> "$TEMP_JSON"

mv "$TEMP_JSON" "$ABS_OUTPUT"

echo "=========================================="
echo " Manifest created successfully!"
echo " Total files: $COUNT"
echo " Total size : $((TOTAL_BYTES / 1048576)) MB ($TOTAL_BYTES bytes)"
echo " Output path: $ABS_OUTPUT"
echo "=========================================="
