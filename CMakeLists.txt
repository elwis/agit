cmake_minimum_required(VERSION 3.22)
project(agit C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS ON)

# libgit2.a has unresolved pthread symbols even with THREADSAFE=OFF
# (detected at configure time). The AROS sysroot provides libpthread.a.
find_package(Threads REQUIRED)
if(CMAKE_THREAD_LIBS_INIT)
    set(AGIT_THREAD_LIBS ${CMAKE_THREAD_LIBS_INIT})
else()
    set(AGIT_THREAD_LIBS -lpthread)
endif()

set(AGIT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(LIBGIT2_BUILD_DIR ${AGIT_SOURCE_DIR}/build-libgit2)
set(MBEDTLS_BUILD_DIR ${AGIT_SOURCE_DIR}/build-mbedtls)

add_executable(agit
    src/agit.c
    src/aros_cred.c
    src/aros_dns.c
    src/aros_entropy.c
    src/aros_mman.c
    src/aros_net.c
    src/aros_path_shims.c
    src/aros_posix_shims.c
    src/aros_realpath.c
    src/aros_time.c
)

target_include_directories(agit PRIVATE
    ${AGIT_SOURCE_DIR}/src
    ${AGIT_SOURCE_DIR}/src/aros-shims
    ${AGIT_SOURCE_DIR}/cmake
    ${AGIT_SOURCE_DIR}/deps/libgit2/include
    ${AGIT_SOURCE_DIR}/deps/mbedtls/include
)

target_link_libraries(agit
    ${LIBGIT2_BUILD_DIR}/libgit2.a
    ${MBEDTLS_BUILD_DIR}/library/libmbedtls.a
    ${MBEDTLS_BUILD_DIR}/library/libmbedx509.a
    ${MBEDTLS_BUILD_DIR}/library/libmbedcrypto.a
    ${AGIT_THREAD_LIBS}
)
