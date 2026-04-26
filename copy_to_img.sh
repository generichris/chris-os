#!/bin/bash
# copy_to_img.sh - Copy a folder's contents into fat32.img

set -e

FOLDER="$1"
FAT32_IMG="build/fat32.img"
PADDED_IMG="build/padded.img"
FINAL_IMG="build/final.img"

if [ -z "$FOLDER" ]; then
    echo "Usage: $0 <folder>"
    echo "Example: $0 myfiles/"
    exit 1
fi

if [ ! -d "$FOLDER" ]; then
    echo "Error: '$FOLDER' is not a directory"
    exit 1
fi

if [ ! -f "$FAT32_IMG" ]; then
    echo "Error: $FAT32_IMG not found. Run 'make mkfs' first."
    exit 1
fi

if [ ! -f "$PADDED_IMG" ]; then
    echo "Error: $PADDED_IMG not found. Run 'make mkfs' first."
    exit 1
fi


if ! command -v mcopy &> /dev/null; then
    echo "mtools not found. Installing..."
    sudo apt install -y mtools
fi


echo "Copying files from '$FOLDER' into $FAT32_IMG..."

find "$FOLDER" -type f | while read -r FILE; do
    # Get path relative to the folder
    REL="${FILE#$FOLDER}"
    REL="${REL#/}"

    # FAT32 filenames: uppercase, max 8.3
    FATNAME=$(echo "$REL" | tr '[:lower:]' '[:upper:]')

    echo "  $FILE -> ::$FATNAME"
    mcopy -i "$FAT32_IMG" -o "$FILE" "::$FATNAME"
done

echo ""
echo "Files in image:"
mdir -i "$FAT32_IMG"


echo ""
echo "Rebuilding $FINAL_IMG..."
cat "$PADDED_IMG" "$FAT32_IMG" > "$FINAL_IMG"
echo "Done! Run 'make run' to boot."