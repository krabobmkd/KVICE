#!/bin/sh
# Turn the "div64:" lines of a VICE_AMIGA_DIV64_STATS run into function names.
#
#   amiga/tools/div64_sites.sh build/x64 log.txt
#
# The Amiga prints caller addresses as offsets from __udivdi3: the executable
# has a single code hunk, so the same offsets work on the symbols of the file.
NM=${NM:-m68k-amigaos-nm}
command -v "$NM" >/dev/null 2>&1 || NM=/opt/amiga/bin/m68k-amigaos-nm
exe=$1
log=$2
if [ -z "$exe" ] || [ -z "$log" ]; then
    echo "usage: $0 <x64 executable> <log with div64: lines>" >&2
    exit 1
fi
"$NM" -n "$exe" | awk -v logfile="$log" '
    $2 ~ /^[tT]$/ {
        addr[n] = strtonum("0x" $1); name[n] = $3; n++
        if ($3 == "___udivdi3") base = strtonum("0x" $1)
    }
    END {
        while ((getline line < logfile) > 0) {
            if (match(line, /from __udivdi3([+-][0-9]+)/, m)) {
                a = base + m[1]
                fn = "?"
                for (i = 0; i < n && addr[i] <= a; i++) fn = name[i]
                sub(/from __udivdi3[+-][0-9]+/, "from " fn, line)
            }
            if (line ~ /^div64:/) print line
        }
    }'
