cmake_minimum_required(VERSION 3.10)

project(media_coverter)

set(CMAKE_C_STANDARD 11)

#enable for debugging
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAVFORMAT REQUIRED libavformat)
pkg_check_modules(LIBAVCODEC REQUIRED libavcodec)
pkg_check_modules(LIBAVUTIL REQUIRED libavutil)
pkg_check_modules(LIBSWRESAMPLE REQUIRED libswresample)
pkg_check_modules(LIBOPUS REQUIRED opus)

include_directories(
    ./
)

add_executable(media_converter media_converter.c media_converter_opus.c)

target_link_libraries(media_converter
    avformat
    avcodec
    avutil
    swresample
    opus
    m
)
