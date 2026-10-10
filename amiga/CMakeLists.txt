# CMakeLists.txt for the AmigaOS 3.x m68k port of VICE 3.10.
#
# Cross compile with bebbo gcc 6.5 and the amigacommonlibs cmake platform:
#
#   mkdir build && cd build
#   cmake .. -DCMAKE_TOOLCHAIN_FILE=$(realpath ../../../amigacommonlibs/cmake/Modules/Platform/m68k-amigaos.cmake) -DCMAKE_BUILD_TYPE=Release
#   cmake --build . -j8
#
# (the toolchain file path must be absolute)
#
# Programs of the Amiga package (one option each):
#   x64     C64 (the fast VIC-II emulation)       ON  (ported)
#   xvic    VIC-20                                OFF (being ported)
#   xplus4  Plus/4, C16, C116 (TED machines)      ON  (ported)
#   xpet    PET / CBM                             OFF (not ported yet)
#   c1541   disk image tool                       OFF (not ported yet)
#   KVICEResourceInstaller  ../ResourceInstaller  ON  (system files download)
#
# Pure C, FastSID only (reSID is C++ and too slow for a 68030).
#
# Sources (cmake/*.cmake, generated from a native build of each program):
#   vice_common_sources.cmake  shared by the 4 emulators, compiled once into
#                              the vice_common object library, with the Amiga
#                              arch layer (src/arch/amiga)
#   <program>_sources.cmake    what only that emulator uses
#   c1541_sources.cmake        all of c1541 (a subset, not vice_common)

cmake_minimum_required (VERSION 3.13)

project(ViceAmiga LANGUAGES C VERSION 3.10)

option(VICE_BUILD_X64    "build x64, the C64 emulator" ON)
option(VICE_BUILD_XVIC   "build xvic, the VIC-20 emulator (being ported)" ON)
option(VICE_BUILD_XPLUS4 "build xplus4, the Plus/4 C16 C116 emulator" ON)
option(VICE_BUILD_XPET   "build xpet, the PET CBM emulator (not ported yet)" OFF)
option(VICE_BUILD_C1541  "build c1541, the disk image tool (not ported yet)" OFF)
option(KVICE_BUILD_RESOURCE_INSTALLER "build KVICE Resource Installer (MUI, AmiSSL v5): downloads the system files the emulators need" ON)

option(AMIGA_USE_68020 "compile for 68020" OFF)
option(AMIGA_USE_68030 "compile for 68030" ON)
option(AMIGA_USE_68040 "compile for 68040" OFF)
option(AMIGA_USE_68060 "compile for 68060" OFF)
option(VICE_USE_HARDFLOAT "use 6888x/040/060 FPU instructions" OFF)
option(VICE_AMIGA_TRACE "print startup traces to stdout (arch/amiga/amigatrace.h)" OFF)
option(VICE_AMIGA_SAVE_ASM "keep the generated assembler (.s) and preprocessed (.i) files next to the objects in the build dir" ON)
option(VICE_AMIGA_TIME_STATS "measure: print the host time spent in CPU, VIC-II, screen output, SID, sound and waiting every 5 s" OFF)
option(VICE_AMIGA_DIV64_STATS "measure: print 64 bit divisions per second and their callers every 5 s" OFF)
option(VICE_AMIGA_STACK_WATCH "measure: print the stack really used, at exit (STACK_WATCH in arch/amiga/main.c)" OFF)

set(VICE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../src")
# ROMs and data files of VICE 3.10 (C64/, DRIVES/...): ../data when copied
# there from an official VICE 3.10 archive, else the vice-3.10 source tree
# next to KVICE
set(VICE_DATA_DEFAULT "${CMAKE_CURRENT_SOURCE_DIR}/../data")
if(NOT EXISTS "${VICE_DATA_DEFAULT}" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../vice-3.10/data")
	set(VICE_DATA_DEFAULT "${CMAKE_CURRENT_SOURCE_DIR}/../../vice-3.10/data")
endif()
set(VICE_DATA "${VICE_DATA_DEFAULT}" CACHE PATH "VICE 3.10 data dir: ROMs in C64/, DRIVES/...")
set(COMMONLIBS "${CMAKE_CURRENT_SOURCE_DIR}/../../amigacommonlibs")

# "cmake --install ." or "make install": a runnable folder to copy to the Amiga.
# The data dir is the program dir (PROGDIR:), ROMs are searched in
# PROGDIR:<machine> and PROGDIR:DRIVES, then in PROGDIR://data/<machine> and
# PROGDIR://data/DRIVES (the source tree, for an executable in amiga/build).
set(VICE_DIST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dist/VICE" CACHE PATH "where make install puts the Amiga distribution")

if(AMIGA)
	if(AMIGA_USE_68060)
		list(APPEND AMIGA_COMPILER_OPTIONS -m68060)
	elseif(AMIGA_USE_68040)
		list(APPEND AMIGA_COMPILER_OPTIONS -m68040)
	elseif(AMIGA_USE_68030)
		list(APPEND AMIGA_COMPILER_OPTIONS -m68030)
	else()
		list(APPEND AMIGA_COMPILER_OPTIONS -m68020)
	endif()
	if(VICE_USE_HARDFLOAT)
		list(APPEND AMIGA_COMPILER_OPTIONS -mhard-float)
	else()
		list(APPEND AMIGA_COMPILER_OPTIONS -msoft-float)
	endif()
endif()

# zlib from amigacommonlibs (target: z)
add_subdirectory(${COMMONLIBS}/zlib zlib EXCLUDE_FROM_ALL)
# amigacommonlibs compiles zlib with -O2: added after it, the last -O wins
target_compile_options(z PRIVATE -O3)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/vice_common_sources.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/x64_sources.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/xvic_sources.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/xplus4_sources.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/xpet_sources.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/c1541_sources.cmake)

# ---------------------------------------------------------------------------
# source list adjustments for AmigaOS 3.x

# not usable on AmigaOS 3.x for now
set(VICE_AMIGA_EXCLUDED_SRC
	${VICE_SRC}/arch/shared/socketdrv/socketdrv.c	# no HAVE_NETWORK (bsdsocket.library later)
	${VICE_SRC}/gfxoutputdrv/ffmpegexedrv.c		# needs an external ffmpeg process + sockets
	${VICE_SRC}/vicii/vicii-clock-stretch.c		# x128 only, never linked in x64
	${VICE_SRC}/drive/drive-sound.c			# floppy noise samples: arch/amiga/amiga_nosoundfx.c
	${VICE_SRC}/datasette/datasette-sound.c		# datasette sound: arch/amiga/amiga_nosoundfx.c
)
foreach(list_name VICE_COMMON_SRC VICE_X64_SRC VICE_XVIC_SRC VICE_XPLUS4_SRC VICE_XPET_SRC VICE_C1541_SRC)
	list(REMOVE_ITEM ${list_name} ${VICE_AMIGA_EXCLUDED_SRC})
endforeach()

# needed by hvsc, header-only in the autotools build list
list(APPEND VICE_COMMON_SRC ${VICE_SRC}/lib/md5/md5.c)

# The Amiga arch layer, shared by all the emulators (display, scalers, UI).
# What is machine specific comes from the amiga_machine table
# (amigamachine.h) of the machine UI file in each emulator's own list.
list(APPEND VICE_COMMON_SRC
	${VICE_SRC}/arch/amiga/amigatrace.c
	${VICE_SRC}/arch/amiga/soundahi.c
	${VICE_SRC}/arch/amiga/joy.c
	${VICE_SRC}/arch/amiga/amigawait.c
	${VICE_SRC}/arch/amiga/amigalocale.c
	${VICE_SRC}/arch/amiga/amigaaction.c
	${VICE_SRC}/arch/amiga/amigabasic.c
	${VICE_SRC}/arch/amiga/amigamenu.c
	${VICE_SRC}/arch/amiga/amigamui.c
	${VICE_SRC}/arch/amiga/amigakeys.c
	${VICE_SRC}/arch/amiga/amigafile.c
	${VICE_SRC}/arch/amiga/amiga_cgxscale.c
	${VICE_SRC}/arch/amiga/amiga_planarscale.c
	${VICE_SRC}/arch/amiga/amiga_screenmode.c
	# replaces the slow libgcc (68000 multilib) 64 bit divisions
	${VICE_SRC}/arch/amiga/amiga_div64.c
	${VICE_SRC}/arch/amiga/amiga_propjoy.c
	# empty drive and datasette sound effects
	${VICE_SRC}/arch/amiga/amiga_nosoundfx.c
	# measurement build (VICE_AMIGA_TIME_STATS), empty otherwise
	${VICE_SRC}/arch/amiga/amiga_timestats.c
)

set(VICE_INCLUDE_DIRS
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${VICE_SRC}
	${VICE_SRC}/arch/amiga
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
	${COMMONLIBS}/zlib
	${COMMONLIBS}/extrasdk/CGX/include
	${COMMONLIBS}/extrasdk/AHI/include
	${COMMONLIBS}/extrasdk/MUI38/include
	${CMAKE_CURRENT_BINARY_DIR}/generated
)

# the default positional keymap of each machine built in (its UI file,
# c64ui.c...), used when data/<machine>/amiga_positional.vkm is not found on
# disk: a C string of the file without its comment lines, in
# generated/amiga_positional_vkm_<machine>.h, made again when the file changes
function(vice_amiga_builtin_vkm machine)
	set(vkm_file "${CMAKE_CURRENT_SOURCE_DIR}/data/${machine}/amiga_positional.vkm")
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${vkm_file}")
	file(READ "${vkm_file}" vkm_text)
	# comments out first: they have ';' (CMake list separator) and brackets
	# (a vkm comment runs to the end of its line)
	string(REGEX REPLACE "/\\*[^\n]*" "" vkm_text "${vkm_text}")
	string(REGEX REPLACE "#[^\n]*" "" vkm_text "${vkm_text}")
	string(REGEX REPLACE "[;\r\\\"]" "" vkm_text "${vkm_text}")
	string(REPLACE "\n" ";" vkm_lines "${vkm_text}")
	set(vkm_c "/* made by CMakeLists.txt from data/${machine}/amiga_positional.vkm */\n")
	string(APPEND vkm_c "static const char amiga_positional_vkm[] =\n")
	foreach(vkm_line IN LISTS vkm_lines)
		string(STRIP "${vkm_line}" vkm_line)
		if(NOT vkm_line STREQUAL "")
			string(APPEND vkm_c "    \"${vkm_line}\\n\"\n")
		endif()
	endforeach()
	string(APPEND vkm_c "    ;\n")
	string(TOLOWER "${machine}" machine_lc)
	file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/amiga_positional_vkm_${machine_lc}.h" "${vkm_c}")
endfunction()
vice_amiga_builtin_vkm(C64)
vice_amiga_builtin_vkm(PLUS4)
vice_amiga_builtin_vkm(VIC20)

# same compile settings for every target
function(vice_amiga_setup_target target)
	set_property(TARGET ${target} PROPERTY C_STANDARD 11)
	target_include_directories(${target} PRIVATE ${VICE_INCLUDE_DIRS})
	target_compile_definitions(${target} PRIVATE HAVE_CONFIG_H=1)
	if(VICE_AMIGA_DIV64_STATS)
		target_compile_definitions(${target} PRIVATE VICE_AMIGA_DIV64_STATS=1)
	endif()
	if(VICE_AMIGA_TIME_STATS)
		target_compile_definitions(${target} PRIVATE VICE_AMIGA_TIME_STATS=1)
	endif()
	if(VICE_AMIGA_TRACE)
		target_compile_definitions(${target} PRIVATE VICE_AMIGA_TRACE=1)
	endif()
	if(VICE_AMIGA_STACK_WATCH)
		target_compile_definitions(${target} PRIVATE STACK_WATCH=1)
	endif()
	target_compile_options(${target} PRIVATE
		-O3 -fomit-frame-pointer -Wno-unused-parameter
		${AMIGA_COMPILER_OPTIONS})
	if(VICE_AMIGA_SAVE_ASM)
		# CMakeFiles/<target>.dir/.../<file>.s, with the C variable names
		target_compile_options(${target} PRIVATE -save-temps=obj -fverbose-asm)
	endif()
endfunction()

# an emulator: vice_common objects + its own sources
function(vice_amiga_emulator target)
	add_executable(${target} $<TARGET_OBJECTS:vice_common> ${ARGN})
	vice_amiga_setup_target(${target})
	target_link_options(${target} PRIVATE ${AMIGA_COMPILER_OPTIONS})
	target_link_libraries(${target} PRIVATE z m)
	install(TARGETS ${target} DESTINATION "${VICE_DIST_DIR}")
endfunction()

# ROMs, keymaps... of a machine: next to the executable in the build dir
# (runnable from there) and in the distribution
# Amiga specific data of KVICE (keymaps...), in this repository
set(KVICE_DATA "${CMAKE_CURRENT_SOURCE_DIR}/data")

function(vice_amiga_data)
	foreach(dir ${ARGN})
		# ROMs and data files are not in the KVICE repository: copy them to
		# ../data from an official VICE 3.10 archive to get a runnable build
		if(EXISTS ${VICE_DATA}/${dir})
			file(COPY ${VICE_DATA}/${dir}
				DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
				PATTERN "gtk3_*" EXCLUDE
				PATTERN "sdl_*" EXCLUDE
				PATTERN "Makefile*" EXCLUDE)
			install(DIRECTORY ${VICE_DATA}/${dir}
				DESTINATION "${VICE_DIST_DIR}"
				PATTERN "gtk3_*" EXCLUDE
				PATTERN "sdl_*" EXCLUDE
				PATTERN "Makefile*" EXCLUDE)
		else()
			message(WARNING "${VICE_DATA}/${dir} not found: no ROMs nor data files copied")
		endif()
		# then the KVICE files: they replace VICE ones of the same name
		if(EXISTS ${KVICE_DATA}/${dir})
			file(COPY ${KVICE_DATA}/${dir} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
			install(DIRECTORY ${KVICE_DATA}/${dir} DESTINATION "${VICE_DIST_DIR}")
		endif()
	endforeach()
endfunction()

# ---------------------------------------------------------------------------
# targets

if(VICE_BUILD_X64 OR VICE_BUILD_XVIC OR VICE_BUILD_XPLUS4 OR VICE_BUILD_XPET)
	# compiled once, linked in each emulator
	add_library(vice_common OBJECT ${VICE_COMMON_SRC})
	vice_amiga_setup_target(vice_common)
	vice_amiga_data(DRIVES)
endif()

if(VICE_BUILD_X64)
	vice_amiga_emulator(x64 ${VICE_X64_SRC})
	vice_amiga_data(C64)
endif()

if(VICE_BUILD_XPLUS4)
	vice_amiga_emulator(xplus4 ${VICE_XPLUS4_SRC})
	target_include_directories(xplus4 PRIVATE
		${VICE_SRC}/plus4/cart
		${VICE_SRC}/drive/iec/plus4exp)
	vice_amiga_data(PLUS4)
endif()

if(VICE_BUILD_XVIC)
	vice_amiga_emulator(xvic ${VICE_XVIC_SRC})
	target_include_directories(xvic PRIVATE
		${VICE_SRC}/vic20
		${VICE_SRC}/vic20/cart)
	vice_amiga_data(VIC20)
endif()

foreach(prog XPET C1541)
	if(VICE_BUILD_${prog})
		string(TOLOWER ${prog} prog_name)
		message(WARNING "${prog_name} is not ported to AmigaOS yet: skipped. "
		                "Its sources are listed in cmake/${prog_name}_sources.cmake.")
	endif()
endforeach()

# KVICE Resource Installer: its own project in ../ResourceInstaller, built
# here with the emulators (in <build>/ResourceInstaller)
if(KVICE_BUILD_RESOURCE_INSTALLER)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../ResourceInstaller ResourceInstaller)
	install(TARGETS KVICEResourceInstaller DESTINATION "${VICE_DIST_DIR}")
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../ResourceInstaller/resourcelist.txt
		DESTINATION "${VICE_DIST_DIR}")
endif()
