#!/bin/sh
# KVICE Linux benchmark and regression check.
#
# Runs every testprg/*.prg for the same emulated time with a fixed random
# seed, and prints for each one:
# - the hash of all the emulated frames (KVICE_FRAME_CHECKSUM): an
#   optimization must give the same hash, pixel for pixel,
# - the number of host instructions of the emulation, counted by callgrind
#   (deterministic, no special rights needed unlike perf), in a second run
#   without the frames hash.
# Warp mode for speed, but with the sound chips still emulated
# (-soundwarpmode 1), as on the Amiga which never warps, in mono
# (-soundoutput 1: with the dummy device, the default "system decides"
# mode is never resolved and the SID would compute nothing).
#
# usage: linux/kvice_bench.sh <x64 executable> [cycles] [prg...]
#   default: 20000000 cycles (about 20 s of C64 time), all testprg/*.prg

X64="$1"
CYCLES="${2:-20000000}"
if [ -z "$X64" ] || [ ! -x "$X64" ]; then
    echo "usage: $0 <x64 executable> [cycles] [prg...]" >&2
    exit 1
fi
shift
[ $# -gt 0 ] && shift
TOP=$(cd "$(dirname "$0")/.." && pwd)
if [ $# -gt 0 ]; then
    PRGS="$*"
else
    PRGS=$(ls "$TOP"/testprg/*.prg)
fi

# a separate home: no vicerc of the user, same settings each time
BENCH_HOME=$(mktemp -d)
trap 'rm -rf "$BENCH_HOME"' EXIT

printf "%-24s %-18s %16s\n" "program" "frames hash" "instructions"
for prg in $PRGS; do
    # the frames hash: native run
    out=$(HOME="$BENCH_HOME" "$X64" +logcolorize -seed 1 -autostartprgmode 1 -warp \
          -limitcycles "$CYCLES" -soundwarpmode 1 -soundoutput 1 -sounddev dummy -soundrate 22050 \
          -autostart "$prg" 2>&1)
    hash=$(echo "$out" | sed -n 's/.*framecheck: .* hash \([0-9a-f]*\).*/\1/p')
    # the instructions: callgrind run without the hash (not emulation work)
    out=$(HOME="$BENCH_HOME" KVICE_FRAMECHECK=0 valgrind --tool=callgrind \
          --callgrind-out-file=/dev/null \
          "$X64" +logcolorize -seed 1 -autostartprgmode 1 -warp \
          -limitcycles "$CYCLES" -soundwarpmode 1 -soundoutput 1 -sounddev dummy -soundrate 22050 \
          -autostart "$prg" 2>&1)
    instr=$(echo "$out" | sed -n 's/^==[0-9]*== Collected : \([0-9]*\)$/\1/p')
    printf "%-24s %-18s %16s\n" "$(basename "$prg")" "${hash:-?}" "${instr:-?}"
done
