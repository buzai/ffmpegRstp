cmake_minimum_required(VERSION 3.16)
project(ffmpegDemo)

set(CMAKE_CXX_STANDARD 17)

include_directories(/usr/local/Cellar/ffmpeg/4.3.2_3/include)
link_directories(/usr/local/Cellar/ffmpeg/4.3.2_3/lib)

include_directories(/usr/local/Cellar/sdl2/2.0.14_1/include)
link_directories(/usr/local/Cellar/sdl2/2.0.14_1/lib)

add_executable(ffmpegDemo main.cpp)

target_link_libraries(
    ffmpegDemo
    avcodec
    avfilter
    avresample
    postproc
    swscale
    avdevice
    avformat
    avutil
    swresample
    SDL2
)