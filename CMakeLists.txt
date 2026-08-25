cmake_minimum_required(VERSION 3.15)

# ---- version from the VERSION file ----------------------------------------
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" _NET_VERSION_RAW LIMIT_COUNT 1)
string(STRIP "${_NET_VERSION_RAW}" NET_VERSION)

project(net VERSION ${NET_VERSION} LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(net src/net.c)

target_compile_options(net PRIVATE -Wall -Wextra)
target_compile_definitions(net PRIVATE NET_VERSION="${NET_VERSION}")

# macOS frameworks
find_library(LIB_SYSTEM_CONFIG   SystemConfiguration REQUIRED)
find_library(LIB_CORE_FOUNDATION CoreFoundation      REQUIRED)
find_library(LIB_IOKIT           IOKit               REQUIRED)
find_library(LIB_NCURSES NAMES ncurses curses        REQUIRED)

target_link_libraries(net PRIVATE
    ${LIB_SYSTEM_CONFIG}
    ${LIB_CORE_FOUNDATION}
    ${LIB_IOKIT}
    ${LIB_NCURSES}
)

install(TARGETS net DESTINATION bin)
install(FILES man/net.1 DESTINATION share/man/man1)
