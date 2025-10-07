cmake_minimum_required(VERSION 4.0)
project(ascii_kanji)

set(CMAKE_CXX_STANDARD 20)

find_package(Fontconfig REQUIRED)
find_package(Freetype)

add_executable(ascii-kanji main.cpp)


if(NOT Freetype_FOUND OR NOT Freetype_INCLUDE_DIRS OR NOT Freetype_LIBRARIES)
    message(WARNING "CMake could not find Freetype, falling back to pkg-config.")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FT2 REQUIRED freetype2)
    target_include_directories(ascii-kanji PRIVATE Fontconfig::Fontconfig ${FT2_INCLUDE_DIRS})
    target_link_libraries(ascii-kanji PRIVATE Fontconfig::Fontconfig ${FT2_LIBRARIES})
else()
    target_include_directories(ascii-kanji PRIVATE Fontconfig::Fontconfig Freetype::Freetype)
    target_link_libraries(ascii-kanji PRIVATE Fontconfig::Fontconfig Freetype::Freetype)
endif()
