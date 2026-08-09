cmake_minimum_required(VERSION 4.0.0)

project(rasterizer LANGUAGES CXX)

option(USE_SIMD "Use SIMD intrinsics" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(src)
