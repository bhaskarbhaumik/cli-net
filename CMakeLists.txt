cmake_minimum_required(VERSION 3.15)
project(net LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(net src/net.c)

target_compile_options(net PRIVATE -Wall -Wextra)

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
