# CMakeLists.txt for a Linux build of KVICE x64, to profile the emulation on a
# PC (perf, valgrind --tool=callgrind, gprof) with the same sources as the
# Amiga port: the VICE "headless" UI (no window, no GUI library), and the
# emulation code paths of the Amiga build (FastSID with fixed point, no CPU
# memory history, no drive/datasette sound effects...), see include/config.h.
#
#   mkdir build && cd build
#   cmake .. -DCMAKE_BUILD_TYPE=Release
#   cmake --build . -j8
#
# A fixed amount of emulated time, as fast as possible (the ROMs are searched
# in KVICE_DATADIR, by default amiga/data):
#
#   ./x64 +logcolorize -seed 1 -autostartprgmode 1 \
#         -warp -soundwarpmode 1 -limitcycles 100000000 \
#         -soundoutput 1 -sounddev dummy -soundrate 22050 -autostart program.prg
#
# (-soundwarpmode 1: the sound chips are still emulated in warp mode, as on
# the Amiga; -soundoutput 1: mono, else the dummy device leaves the output
# mode unresolved and the SID computes nothing; +logcolorize avoids a crash when stdout is not a terminal;
# -autostartprgmode 1 injects into RAM as the Amiga default; linux/
# kvice_bench.sh runs all the test programs this way)
#
# then for example:
#   valgrind --tool=callgrind ./x64 ...    (exact instruction counts)
#   perf record -g ./x64 ...               (PC time)

cmake_minimum_required (VERSION 3.13)

project(KViceLinux LANGUAGES C)

# closer to a 68k: 32 bit registers, 64 bit CLOCK math in two halves
# (needs the gcc-multilib package)
option(KVICE_LINUX_M32 "32 bit build (-m32)" OFF)
# timestats.h report every 5 s, as the Amiga VICE_AMIGA_TIME_STATS
# (not available yet: the measuring code is in arch/amiga)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

set(VICE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../src")
set(KVICE_AMIGA "${CMAKE_CURRENT_SOURCE_DIR}/../amiga")

# the source lists of the Amiga build: generated from a headless build,
# with arch/headless replaced by arch/amiga, put back here
include(${KVICE_AMIGA}/cmake/vice_common_sources.cmake)
include(${KVICE_AMIGA}/cmake/x64_sources.cmake)
include(${KVICE_AMIGA}/cmake/xplus4_sources.cmake)
include(${KVICE_AMIGA}/cmake/xvic_sources.cmake)
foreach(list_name VICE_COMMON_SRC VICE_X64_SRC VICE_XPLUS4_SRC VICE_XVIC_SRC)
	list(TRANSFORM ${list_name} REPLACE "/arch/amiga/" "/arch/headless/")
endforeach()

# the same exclusions as the Amiga build (kvice_linux_stubs.c: what is then
# missing on Linux)
set(KVICE_EXCLUDED_SRC
	${VICE_SRC}/arch/shared/socketdrv/socketdrv.c
	${VICE_SRC}/gfxoutputdrv/ffmpegexedrv.c
	${VICE_SRC}/vicii/vicii-clock-stretch.c
	${VICE_SRC}/drive/drive-sound.c
	${VICE_SRC}/datasette/datasette-sound.c
)
list(REMOVE_ITEM VICE_COMMON_SRC ${KVICE_EXCLUDED_SRC})
list(REMOVE_ITEM VICE_X64_SRC ${KVICE_EXCLUDED_SRC})
list(REMOVE_ITEM VICE_XPLUS4_SRC ${KVICE_EXCLUDED_SRC})
list(REMOVE_ITEM VICE_XVIC_SRC ${KVICE_EXCLUDED_SRC})
list(APPEND VICE_COMMON_SRC
	${VICE_SRC}/lib/md5/md5.c
	# plain C, not Amiga specific: empty drive and datasette sound effects
	${VICE_SRC}/arch/amiga/amiga_nosoundfx.c
	# no network, no linenoise
	${CMAKE_CURRENT_SOURCE_DIR}/kvice_linux_stubs.c
	# hash of every frame, printed at exit (KVICE_FRAME_CHECKSUM)
	${CMAKE_CURRENT_SOURCE_DIR}/kvice_framecheck.c
)

set(VICE_INCLUDE_DIRS
	# config.h of this build first
	${CMAKE_CURRENT_SOURCE_DIR}/include
	# version.h, infocontrib.h, debug.h: the same on every platform
	${KVICE_AMIGA}/include
	${VICE_SRC}
	${VICE_SRC}/arch/headless
	${VICE_SRC}/arch/shared
	${VICE_SRC}/arch
	${VICE_SRC}/arch/shared/hotkeys
	${VICE_SRC}/arch/shared/socketdrv
	${VICE_SRC}/arch/shared/sounddrv
	${VICE_SRC}/c64
	${VICE_SRC}/c64/cart
	${VICE_SRC}/c64dtv
	${VICE_SRC}/plus4
	${VICE_SRC}/core
	${VICE_SRC}/core/rtc
	${VICE_SRC}/datasette
	${VICE_SRC}/diag
	${VICE_SRC}/diskimage
	${VICE_SRC}/drive
	${VICE_SRC}/drive/iec
	${VICE_SRC}/drive/iec/c64exp
	${VICE_SRC}/drive/ieee
	${VICE_SRC}/drive/tcbm
	${VICE_SRC}/fileio
	${VICE_SRC}/fsdevice
	${VICE_SRC}/gfxoutputdrv
	${VICE_SRC}/hvsc
	${VICE_SRC}/imagecontents
	${VICE_SRC}/joyport
	${VICE_SRC}/lib
	${VICE_SRC}/lib/p64
	${VICE_SRC}/lib/libzmbv
	${VICE_SRC}/lib/md5
	${VICE_SRC}/monitor
	${VICE_SRC}/printerdrv
	${VICE_SRC}/raster
	${VICE_SRC}/rs232drv
	${VICE_SRC}/samplerdrv
	${VICE_SRC}/serial
	${VICE_SRC}/sid
	${VICE_SRC}/tape
	${VICE_SRC}/tapeport
	${VICE_SRC}/userport
	${VICE_SRC}/vdrive
	${VICE_SRC}/vicii
	${VICE_SRC}/video
	# kvice_framecheck.h
	${CMAKE_CURRENT_SOURCE_DIR}
)

set(KVICE_OPTIONS
	# optimized like the Amiga build, but with debug info and frame
	# pointers for the profilers
	-O3 -g -fno-omit-frame-pointer -Wno-unused-parameter)
if(KVICE_LINUX_M32)
	list(APPEND KVICE_OPTIONS -m32)
endif()

add_executable(x64 ${VICE_COMMON_SRC} ${VICE_X64_SRC})
set_property(TARGET x64 PROPERTY C_STANDARD 11)
target_include_directories(x64 PRIVATE ${VICE_INCLUDE_DIRS})
# the ROMs (C64/, DRIVES/...): by default the Amiga data dir of KVICE
set(KVICE_DATADIR "${KVICE_AMIGA}/data" CACHE PATH "VICE data dir: ROMs in C64/, DRIVES/...")
# a hash of every emulated frame printed at exit: the drawing optimizations
# must not change it (same program, -seed and -limitcycles)
option(KVICE_FRAME_CHECKSUM "print a hash of all the emulated frames at exit" ON)
if(KVICE_FRAME_CHECKSUM)
	target_compile_definitions(x64 PRIVATE KVICE_FRAME_CHECKSUM=1)
endif()
# the KVICE Amiga choices that are not Amiga specific (vsync.c: sound flush
# and sync every 16 lines), so the profiles show what the Amiga does
target_compile_definitions(x64 PRIVATE KVICE_LINUX=1)
target_compile_definitions(x64 PRIVATE HAVE_CONFIG_H=1
	VICE_DATADIR="${KVICE_DATADIR}"
	VICE_DOCDIR="${CMAKE_CURRENT_SOURCE_DIR}/../doc")
target_compile_options(x64 PRIVATE ${KVICE_OPTIONS})
if(KVICE_LINUX_M32)
	target_link_options(x64 PRIVATE -m32)
endif()
target_link_libraries(x64 PRIVATE z m pthread dl)

# xplus4 and xvic: same settings, to compare them with the Amiga build
function(kvice_linux_emulator target)
	add_executable(${target} ${VICE_COMMON_SRC} ${ARGN})
	set_property(TARGET ${target} PROPERTY C_STANDARD 11)
	target_include_directories(${target} PRIVATE ${VICE_INCLUDE_DIRS})
	if(KVICE_FRAME_CHECKSUM)
		target_compile_definitions(${target} PRIVATE KVICE_FRAME_CHECKSUM=1)
	endif()
	target_compile_definitions(${target} PRIVATE KVICE_LINUX=1 HAVE_CONFIG_H=1
		VICE_DATADIR="${KVICE_DATADIR}"
		VICE_DOCDIR="${CMAKE_CURRENT_SOURCE_DIR}/../doc")
	target_compile_options(${target} PRIVATE ${KVICE_OPTIONS})
	if(KVICE_LINUX_M32)
		target_link_options(${target} PRIVATE -m32)
	endif()
	target_link_libraries(${target} PRIVATE z m pthread dl)
endfunction()

option(KVICE_LINUX_XPLUS4 "also build xplus4" OFF)
if(KVICE_LINUX_XPLUS4)
	kvice_linux_emulator(xplus4 ${VICE_XPLUS4_SRC})
	target_include_directories(xplus4 PRIVATE
		${VICE_SRC}/plus4/cart
		${VICE_SRC}/drive/iec/plus4exp)
endif()

option(KVICE_LINUX_XVIC "also build xvic" OFF)
if(KVICE_LINUX_XVIC)
	kvice_linux_emulator(xvic ${VICE_XVIC_SRC})
	target_include_directories(xvic PRIVATE
		${VICE_SRC}/vic20
		${VICE_SRC}/vic20/cart)
endif()
