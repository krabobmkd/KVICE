# KVICE Resource Installer: checks and downloads the Commodore system files
# the KVICE emulators need (resourcelist.txt), MUI window, AmiSSL v5.
#
# Cross compile with bebbo gcc 6.5 and the amigacommonlibs cmake platform:
#
#   mkdir build && cd build
#   cmake .. -DCMAKE_TOOLCHAIN_FILE=$(realpath ../../../amigacommonlibs/cmake/Modules/Platform/m68k-amigaos.cmake) -DCMAKE_BUILD_TYPE=Release
#   cmake --build .
#
# Usually built from amiga/CMakeLists.txt with the emulators
# (KVICE_BUILD_RESOURCE_INSTALLER, on by default); this file also builds it alone.
#
# The program reads resourcelist.txt next to it (PROGDIR:): it is copied to
# the build dir. gen_resourcelist.sh writes it.

cmake_minimum_required(VERSION 3.13)

project(KVICEResourceInstaller C)

get_filename_component(COMMONLIBS "${CMAKE_CURRENT_SOURCE_DIR}/../../amigacommonlibs" ABSOLUTE)

set(AMISSL_INCLUDE_DIR "${COMMONLIBS}/extrasdk/AmiSSL/Developer/include"
    CACHE PATH "AmiSSL v5 Developer include directory")
set(AMISSL_LIB_DIR "${COMMONLIBS}/extrasdk/AmiSSL/Developer/lib/AmigaOS3"
    CACHE PATH "AmiSSL v5 Developer lib directory (AmigaOS3)")

add_executable(KVICEResourceInstaller
    rimain.c
    rinet.c
    rilist.c
    risha256.c
    rinet.h
    rilist.h
    risha256.h
)

set_property(TARGET KVICEResourceInstaller PROPERTY C_STANDARD 11)

target_include_directories(KVICEResourceInstaller PRIVATE
    ${COMMONLIBS}/extrasdk/MUI38/include
    ${AMISSL_INCLUDE_DIR}
    ${COMMONLIBS}/extrasdk/SDI/include
)

target_compile_options(KVICEResourceInstaller PRIVATE
    -m68020 -O2 -fomit-frame-pointer -Wall -Wno-unused-parameter)
target_link_options(KVICEResourceInstaller PRIVATE -m68020)

# libnix' errno does not fit libamisslauto.a: the stubs, AmiSSL opened by
# hand (rinet.c), as FriendSh3ep does
target_link_libraries(KVICEResourceInstaller PRIVATE ${AMISSL_LIB_DIR}/libamisslstubs.a)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resourcelist.txt
               ${CMAKE_CURRENT_BINARY_DIR}/resourcelist.txt COPYONLY)
