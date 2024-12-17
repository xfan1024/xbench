cmake_minimum_required(VERSION 3.10)
project(xbench-bin)

# release compile
set(CMAKE_BUILD_TYPE Release)
# add global compile flags
add_compile_options(-Wall -Wextra -Werror)
add_compile_options(-fno-tree-loop-distribute-patterns)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(NUMA IMPORTED_TARGET numa)

add_library(multitask STATIC multitask.c)
target_link_libraries(multitask PUBLIC Threads::Threads)
if (NUMA_FOUND)
    target_link_libraries(multitask PUBLIC PkgConfig::NUMA)
    target_compile_options(multitask PUBLIC -D HAVE_NUMA)
endif()


add_executable(xb-memtest xb-memtest.c)
target_link_libraries(xb-memtest PRIVATE Threads::Threads)
if (NUMA_FOUND)
    target_link_libraries(xb-memtest PRIVATE PkgConfig::NUMA)
    target_compile_options(xb-memtest PRIVATE -D HAVE_NUMA)
endif()

add_executable(xb-prime xb-prime.c)
target_link_libraries(xb-prime PRIVATE Threads::Threads m)

add_executable(xb-openssl xb-openssl.c )
target_link_libraries(xb-openssl PRIVATE multitask OpenSSL::Crypto)
