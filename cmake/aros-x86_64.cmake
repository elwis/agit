# =====================================================================
# CMake toolchain file for cross-compiling against AROS x86_64 (ABIv11)
#
# Use with:
#   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$(pwd)/cmake/aros-x86_64.cmake
#
# The sysroot value below is the same as in the Zqueal Makefile; adjust
# if needed via -DAROS_SYSROOT=/other/path on the cmake command line.
# =====================================================================

set(CMAKE_SYSTEM_NAME AROS)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Allow override from the command line, otherwise the same default as Zqueal
if(NOT DEFINED AROS_SYSROOT)
    set(AROS_SYSROOT "$ENV{HOME}/Aros/arosbuilds/core-linux-x86_64-d/bin/linux-x86_64/AROS/Development"
        CACHE PATH "AROS development sysroot")
endif()

# Compilers -- must be in PATH (same assumption as the Zqueal Makefile)
set(CMAKE_C_COMPILER   x86_64-aros-gcc)
set(CMAKE_CXX_COMPILER x86_64-aros-g++)
set(CMAKE_AR           x86_64-aros-ar CACHE FILEPATH "Archiver")
set(CMAKE_RANLIB       x86_64-aros-ranlib CACHE FILEPATH "Ranlib")

# -DMBEDTLS_NO_PLATFORM_ENTROPY and -DMBEDTLS_PLATFORM_MS_TIME_ALT are
# added HERE (not via -DCMAKE_C_FLAGS on the command line!) because
# AROS isn't recognized as "unix" by mbedTLS's platform detection --
# neither the entropy source nor mbedtls_ms_time() get a built-in
# implementation, and that applies to every project built against this
# toolchain, not just mbedTLS. We provide our own implementations in
# src/aros_port.c the day something actually LINKS against these
# symbols (the mbedTLS library itself compiles fine without them,
# since static libraries don't require all symbols to be defined until
# final linking).
#
# NOTE: NEVER overwrite CMAKE_C_FLAGS directly via -D on the cmake
# command line -- that wipes out --sysroot from here, since the _INIT
# variant is only used if CMAKE_C_FLAGS is unset. If you want to add
# your OWN flags, do it in CMakeLists.txt or via
# target_compile_definitions().
# AROS-specific mbedTLS deviations (see cmake/mbedtls-user-config.h):
# some modules enabled by default (e.g. MBEDTLS_TIMING_C) can't be
# turned off via a simple -U on the command line, since
# mbedtls_config.h defines them unconditionally -- mbedTLS's own
# solution for that is a "user config" header that's included AFTER
# the default configuration and is allowed to #undef things.
# CMAKE_CURRENT_LIST_DIR points to the same directory this toolchain
# file lives in, regardless of where the project was checked out from.
set(AROS_MBEDTLS_USER_CONFIG_DIR "${CMAKE_CURRENT_LIST_DIR}")

set(CMAKE_C_FLAGS_INIT   "--sysroot=${AROS_SYSROOT} -DMBEDTLS_NO_PLATFORM_ENTROPY -DMBEDTLS_PLATFORM_MS_TIME_ALT -DMBEDTLS_USER_CONFIG_FILE=\\\"mbedtls-user-config.h\\\" -I${AROS_MBEDTLS_USER_CONFIG_DIR}")
set(CMAKE_CXX_FLAGS_INIT "--sysroot=${AROS_SYSROOT} -DMBEDTLS_NO_PLATFORM_ENTROPY -DMBEDTLS_PLATFORM_MS_TIME_ALT -DMBEDTLS_USER_CONFIG_FILE=\\\"mbedtls-user-config.h\\\" -I${AROS_MBEDTLS_USER_CONFIG_DIR}")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "--sysroot=${AROS_SYSROOT}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "--sysroot=${AROS_SYSROOT}")

# CMake should NOT look for runnable test programs on the host (Linux)
# -- try_compile tests can build but must never be RUN here.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Header/library search order: NEVER look in the host system's
# /usr/include or /usr/lib -- only in the sysroot. Otherwise we risk
# accidentally linking against Linux glibc headers/libraries.
set(CMAKE_FIND_ROOT_PATH ${AROS_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

message(STATUS "AROS x86_64 toolchain: sysroot = ${AROS_SYSROOT}")
