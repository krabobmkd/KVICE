# VICE 3.10 for AmigaOS 3.x (68020+)

CMake cross build with bebbo gcc 6.5 + libnix, using the
`amigacommonlibs/cmake/Modules/Platform/m68k-amigaos.cmake` platform.

## Build

    cd amiga && mkdir -p build && cd build
    cmake .. -DCMAKE_TOOLCHAIN_FILE=../../../amigacommonlibs/cmake/Modules/Platform/m68k-amigaos.cmake -DCMAKE_BUILD_TYPE=Release
    cmake --build . -j8
    cmake --install .        # -> amiga/dist/VICE/{x64,C64/,DRIVES/}

Options: `AMIGA_USE_68020/030/040/060` (default 68030), `VICE_USE_HARDFLOAT` (OFF),
`VICE_AMIGA_TRACE` (ON: startup traces on stdout).

## Run

    stack 262144
    cd VICE
    x64

x64 refuses to start with less than 128KB of stack (VICE keeps 4KB path
buffers on the stack). With `VICE_AMIGA_TRACE`, lines like
`[TRACE main.c:412] main:video_init` show how far startup got, and
`vsync heartbeat, frame N` is printed every 50 emulated frames.

## State (milestone 1)

- x64 only, pure C, FastSID only (reSID is C++ and too slow for a 68030).
- `src/arch/amiga` is a copy of `src/arch/headless`: no video, no sound, no input yet.
  The native VICE 3.2 port (`vice-3.2/src/arch/amigaos`, `AMIGA_M68K` parts)
  is the reference for the Intuition/AHI/joystick layer to add next.
- No network (`HAVE_NETWORK` off), no ffmpeg export.
- `amiga/include/config.h` is hand written (no autoconf for this target).
- `amiga/x64_sources.cmake` was harvested from a native autotools headless
  build of x64 (`ar t` of each linked lib, mapped back to sources).

## Changes made to the shared VICE sources

All guarded by `AMIGA_COMPILE`:

- `arch/shared/archdep_*.c`: POSIX branches also used for Amiga where libnix
  provides the calls; Amiga branches for program path (`GetProgramDir()`),
  boot path / program name (`:` volume separator), home (`PROGDIR:`),
  data/doc dirs, `realpath` (`Lock`/`NameFromLock`), relative paths,
  filename sanitizing, and a logger that flushes every line.
- `arch/shared/archdep_defs.h`: path list separator `;` (`:` is a volume separator).
- `util.c` `util_join_paths()`: no `/` after a trailing `:` (`DH0:/x` means parent of `DH0:`).
- `main.c`: `DBG()` checkpoints routed to `AMIGA_TRACE`, more checkpoints.
- `gfxoutputdrv/gfxoutput.c`: ffmpeg.exe driver not registered.

Upstream fixes for building without `HAVE_NETWORK` (not Amiga specific):
`monitor/monitor_binary.c`, `monitor/monitor_network.c` (`UI_JAM_HARD_RESET`
does not exist), `network.c` (`DBG` undefined in the stub part),
`arch/shared/coproc.c` (stub part missing includes), `vsync.c` (`monitor.h`
only included with network).
