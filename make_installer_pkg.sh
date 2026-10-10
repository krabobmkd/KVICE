#!/bin/sh
# make_installer_pkg.sh
#
# Assembles the KVICE installer package directory from the built binaries
# and project sources. Run this on Linux after a successful build of x64,
# xplus4 and xvic (amiga/buildr: all three emulators on).
#
# Usage:
#   ./make_installer_pkg.sh [output_dir]
#
# Default output: ./KVICE_pkg  (deleted and recreated each run)
#
# Other places, from the environment:
#   KVICE_BUILD   emulator build dir        (default: amiga/buildr)
#   PETMATE_DIR   amigapetmate project      (default: ../amigapetmate)
#   VICE_DATA     VICE 3.10 data, PetMe fonts (default: ../vice-3.10/data)
#   RESINST_BUILD KVICE Resource Installer build dir (default: the
#                 ResourceInstaller dir of KVICE_BUILD, where amiga/CMakeLists.txt
#                 builds it, else ResourceInstaller/build)
#
# The resulting directory can be archived as an LHA for distribution:
#   cd KVICE_pkg && lha a ../KVICEr1.lha *
#
# The Commodore system files (*.bin) are never put in the package.
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PKG="${1:-$SCRIPT_DIR/KVICE_pkg}"

BUILD="${KVICE_BUILD:-$SCRIPT_DIR/amiga/buildr}"
DATA="$SCRIPT_DIR/amiga/data"
ICONS="$SCRIPT_DIR/icons"
INSTALLER_SRC="$SCRIPT_DIR/Installer"
PETMATE_DIR="${PETMATE_DIR:-$SCRIPT_DIR/../amigapetmate}"
VICE_DATA="${VICE_DATA:-$SCRIPT_DIR/../vice-3.10/data}"
if [ -z "$RESINST_BUILD" ]; then
    RESINST_BUILD="$BUILD/ResourceInstaller"
    if [ ! -f "$RESINST_BUILD/KVICEResourceInstaller" ]; then
        RESINST_BUILD="$SCRIPT_DIR/ResourceInstaller/build"
    fi
fi

# ---------------------------------------------------------------------------
# Sanity checks
# ---------------------------------------------------------------------------
check_file() {
    if [ ! -f "$1" ]; then
        echo "ERROR: required file not found: $1" >&2
        exit 1
    fi
}

for emu in x64 xplus4 xvic; do
    check_file "$BUILD/$emu"
    check_file "$ICONS/$emu.info"
done
for machine in C64 PLUS4 VIC20; do
    check_file "$DATA/$machine/amiga_positional.vkm"
done
# everything tracked in amiga/data is meant to be installed (not the
# system files, *.bin, that may sit there untracked for local runs)
check_file "$SCRIPT_DIR/LICENSE"
check_file "$INSTALLER_SRC/Install"
check_file "$INSTALLER_SRC/Install.info"
check_file "$INSTALLER_SRC/KVICE.guide"
check_file "$INSTALLER_SRC/KVICE.guide.info"
check_file "$INSTALLER_SRC/KVICE.readme"
check_file "$INSTALLER_SRC/KVICE.readme.info"
for g in BASICv2Quick BASICv3Quick; do
    check_file "$INSTALLER_SRC/$g.guide"
    check_file "$INSTALLER_SRC/$g.guide.info"
done
check_file "$VICE_DATA/common/PetMe64.ttf"
check_file "$VICE_DATA/common/PetMe-FreeLicense.txt"

# PetMate: its cross-compiled build dir has the program, its icon, its
# guide and the guide icon
PETMATE_BUILD="$PETMATE_DIR/build-AmigaGcc-Release"
check_file "$PETMATE_BUILD/PetMate"
check_file "$PETMATE_BUILD/PetMate.info"
check_file "$PETMATE_BUILD/PetMate.guide"
check_file "$PETMATE_BUILD/PetMate.guide.info"
check_file "$PETMATE_DIR/LICENSE"
check_file "$INSTALLER_SRC/KVICEdir.info"

# ---------------------------------------------------------------------------
# (Re)create package directory tree
# ---------------------------------------------------------------------------
echo "Creating package directory: $PKG"
rm -rf "$PKG"
mkdir -p "$PKG/bin" "$PKG/data" "$PKG/PetMate" "$PKG/fonts/PetMe"

# ---------------------------------------------------------------------------
# Installer script and icon
# ---------------------------------------------------------------------------
echo "Copying installer..."
cp "$INSTALLER_SRC/Install"      "$PKG/Install"
cp "$INSTALLER_SRC/Install.info" "$PKG/Install.info"
# the icon of the installed KVICE drawer: in bin/, the Install script
# copies it as <parent>/KVICE.info
cp "$INSTALLER_SRC/KVICEdir.info" "$PKG/bin/KVICEdir.info"

# ---------------------------------------------------------------------------
# Emulators, icons, and the files of their resource drawers (no *.bin)
# ---------------------------------------------------------------------------
echo "Copying emulators..."
for emu in x64 xplus4 xvic; do
    cp "$BUILD/$emu"       "$PKG/bin/$emu"
    cp "$ICONS/$emu.info"  "$PKG/bin/$emu.info"
done
for machine in C64 PLUS4 VIC20; do
    mkdir -p "$PKG/data/$machine"
    for f in "$DATA/$machine"/*; do
        case "$f" in
            *.bin|*.uaem) ;;
            *) [ -f "$f" ] && cp "$f" "$PKG/data/$machine/" ;;
        esac
    done
done

# ---------------------------------------------------------------------------
# PetMate
# ---------------------------------------------------------------------------
# (its MIT license as PetMate.LICENSE: installed next to KVICE's LICENSE)
echo "Copying PetMate..."
cp "$PETMATE_BUILD/PetMate"            "$PKG/PetMate/PetMate"
cp "$PETMATE_BUILD/PetMate.info"       "$PKG/PetMate/PetMate.info"
cp "$PETMATE_BUILD/PetMate.guide"      "$PKG/PetMate/PetMate.guide"
cp "$PETMATE_BUILD/PetMate.guide.info" "$PKG/PetMate/PetMate.guide.info"
cp "$PETMATE_DIR/LICENSE"              "$PKG/PetMate/PetMate.LICENSE"
# the example drawings (installed with PetMate, to <dest>/PetmateExample)
mkdir -p "$PKG/PetMate/PetmateExample"
cp "$INSTALLER_SRC/PetmateExample"/*.petmate "$PKG/PetMate/PetmateExample/"

# ---------------------------------------------------------------------------
# PetMe fonts: the collection as distributed, unmodified, with its license
# verbatim (Kreative Software free use license: no modification, the
# license included with every copy)
# ---------------------------------------------------------------------------
echo "Copying PetMe fonts..."
cp "$VICE_DATA"/common/PetMe*.ttf "$PKG/fonts/PetMe/"
cp "$VICE_DATA/common/PetMe-FreeLicense.txt" "$PKG/fonts/PetMe/"

# ---------------------------------------------------------------------------
# KVICE Resource Installer (optional, when it is built)
# ---------------------------------------------------------------------------
if [ -f "$RESINST_BUILD/KVICEResourceInstaller" ]; then
    echo "Copying KVICE Resource Installer..."
    cp "$RESINST_BUILD/KVICEResourceInstaller" "$PKG/bin/"
    if [ -f "$SCRIPT_DIR/ResourceInstaller/KVICEResourceInstaller.info" ]; then
        cp "$SCRIPT_DIR/ResourceInstaller/KVICEResourceInstaller.info" "$PKG/bin/"
    fi
    cp "$SCRIPT_DIR/ResourceInstaller/resourcelist.txt" "$PKG/resourcelist.txt"
else
    echo "Note: KVICE Resource Installer not built, not in the package."
fi

# ---------------------------------------------------------------------------
# Documentation (guide/readme + their icons side by side at the package
# root: the Install script's (infos) picks up "<name>.info")
# ---------------------------------------------------------------------------
echo "Copying documentation..."
cp "$INSTALLER_SRC/KVICE.guide"       "$PKG/KVICE.guide"
cp "$INSTALLER_SRC/KVICE.guide.info"  "$PKG/KVICE.guide.info"
cp "$INSTALLER_SRC/KVICE.readme"      "$PKG/KVICE.readme"
cp "$INSTALLER_SRC/KVICE.readme.info" "$PKG/KVICE.readme.info"
cp "$SCRIPT_DIR/LICENSE"              "$PKG/LICENSE"
# BASIC quick guides and the .bas examples (the Installer asks for them)
for g in BASICv2Quick BASICv3Quick; do
    cp "$INSTALLER_SRC/$g.guide"      "$PKG/$g.guide"
    cp "$INSTALLER_SRC/$g.guide.info" "$PKG/$g.guide.info"
done
for machine in C64 PLUS4 VIC20; do
    mkdir -p "$PKG/BasicExample/$machine"
    cp "$INSTALLER_SRC/BasicExample/$machine"/*.bas "$PKG/BasicExample/$machine/"
done

# ---------------------------------------------------------------------------
# No Commodore system file may end in the package
# ---------------------------------------------------------------------------
if find "$PKG" -name "*.bin" | grep -q .; then
    echo "ERROR: *.bin files found in the package:" >&2
    find "$PKG" -name "*.bin" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
echo ""
echo "Package ready in: $PKG"
echo ""
echo "Contents:"
find "$PKG" -not -type d | sort | sed "s|$PKG/||"
echo ""
echo "To create an LHA archive:"
echo "  cd \"$PKG\" && lha a ../KVICEr1.lha *"
