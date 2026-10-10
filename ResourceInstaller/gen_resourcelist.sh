#!/bin/sh
# gen_resourcelist.sh
#
# Writes resourcelist.txt, the list of the Commodore system files the KVICE
# emulators can use: their path in the KVICE drawer, their SHA-256, and the
# server they are downloaded from (the VICE source mirror on GitHub):
# each file is at the server base + its path.
#
# The SHA-256 come from the VICE 3.10 data files.
#
# Usage:
#   ./gen_resourcelist.sh [--check-urls]
#
#   --check-urls  download each file from the server and compare its SHA-256
#
# Environment:
#   VICE_DATA  VICE 3.10 data dir (default: ../../vice-3.10/data)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VICE_DATA="${VICE_DATA:-$SCRIPT_DIR/../../vice-3.10/data}"
OUT="$SCRIPT_DIR/resourcelist.txt"
BASE="https://raw.githubusercontent.com/VICE-Team/svn-mirror/main/vice/data/"

# every file the x64, xplus4 and xvic models and drive types can use
FILES="
C64/basic-901226-01.bin
C64/chargen-901225-01.bin
C64/chargen-906143-02.bin
C64/kernal-901227-01.bin
C64/kernal-901227-02.bin
C64/kernal-901227-03.bin
C64/kernal-251104-04.bin
C64/kernal-390852-01.bin
C64/kernal-901246-01.bin
C64/kernal-906145-02.bin
PLUS4/basic-318006-01.bin
PLUS4/kernal-318004-01.bin
PLUS4/kernal-318004-05.bin
PLUS4/kernal-318005-05.bin
PLUS4/kernal-364.bin
PLUS4/3plus1-317053-01.bin
PLUS4/3plus1-317054-01.bin
PLUS4/c2lo-364.bin
VIC20/basic-901486-01.bin
VIC20/chargen-901460-02.bin
VIC20/chargen-901460-03.bin
VIC20/kernal.901486-02.bin
VIC20/kernal.901486-06.bin
VIC20/kernal.901486-07.bin
DRIVES/dos1540-325302+3-01.bin
DRIVES/dos1541-325302-01+901229-05.bin
DRIVES/dos1541ii-251968-03.bin
DRIVES/dos1551-318008-01.bin
DRIVES/dos1570-315090-01.bin
DRIVES/dos1571-310654-05.bin
DRIVES/dos1581-318045-02.bin
"

CHECK=0
if [ "$1" = "--check-urls" ]; then
    CHECK=1
    TMP="$(mktemp -d)"
    trap 'rm -rf "$TMP"' EXIT
fi

{
    echo "# KVICE Resource Installer: the files the emulators can use."
    echo "# \"base\": the server, each file is at base + path (several base"
    echo "# lines: tried in order). Then one file per line: path in the KVICE"
    echo "# drawer, SHA-256. Lines starting with # are comments."
    echo "# Written by gen_resourcelist.sh from the VICE 3.10 data files."
    echo "base $BASE"
    for f in $FILES; do
        if [ ! -f "$VICE_DATA/$f" ]; then
            echo "ERROR: $VICE_DATA/$f not found" >&2
            exit 1
        fi
        hash="$(sha256sum "$VICE_DATA/$f" | cut -d' ' -f1)"
        if [ $CHECK = 1 ]; then
            url="$BASE$f"
            if curl -s -f -m 30 -o "$TMP/dl" "$url" \
                    && [ "$(sha256sum "$TMP/dl" | cut -d' ' -f1)" = "$hash" ]; then
                echo "ok    $url" >&2
            else
                echo "BAD   $url" >&2
            fi
        fi
        echo "$f $hash"
    done
} > "$OUT"

echo "$(grep -vc '^#\|^base ' "$OUT") resources written to $OUT"
