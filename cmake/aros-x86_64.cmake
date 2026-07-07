# =====================================================================
# CMake toolchain-fil för cross-kompilering mot AROS x86_64 (ABIv11)
#
# Använd med:
#   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$(pwd)/cmake/aros-x86_64.cmake
#
# Sysroot-värdet nedan är samma som i Zqueal-Makefilen; justera vid
# behov via -DAROS_SYSROOT=/annan/väg på cmake-kommandoraden.
# =====================================================================

set(CMAKE_SYSTEM_NAME AROS)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Tillåt override från kommandoraden, annars samma default som Zqueal
if(NOT DEFINED AROS_SYSROOT)
    set(AROS_SYSROOT "$ENV{HOME}/Aros/arosbuilds/core-linux-x86_64-d/bin/linux-x86_64/AROS/Development"
        CACHE PATH "AROS development sysroot")
endif()

# Kompilatorer -- måste finnas i PATH (samma antagande som Zqueal-Makefilen)
set(CMAKE_C_COMPILER   x86_64-aros-gcc)
set(CMAKE_CXX_COMPILER x86_64-aros-g++)
set(CMAKE_AR           x86_64-aros-ar CACHE FILEPATH "Archiver")
set(CMAKE_RANLIB       x86_64-aros-ranlib CACHE FILEPATH "Ranlib")

# --sysroot måste med på både kompilerings- och länksteget
set(CMAKE_C_FLAGS_INIT   "--sysroot=${AROS_SYSROOT}")
set(CMAKE_CXX_FLAGS_INIT "--sysroot=${AROS_SYSROOT}")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "--sysroot=${AROS_SYSROOT}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "--sysroot=${AROS_SYSROOT}")

# CMake ska INTE leta efter körbara testprogram på värden (Linux) --
# try_compile-testerna kan bygga men aldrig KÖRAS här.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Sökordning för headers/bibliotek: leta ALDRIG i värdsystemets
# /usr/include eller /usr/lib -- bara i sysrooten. Annars riskerar vi
# att av misstag länka mot Linux-glibc-headers/bibliotek.
set(CMAKE_FIND_ROOT_PATH ${AROS_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

message(STATUS "AROS x86_64 toolchain: sysroot = ${AROS_SYSROOT}")
