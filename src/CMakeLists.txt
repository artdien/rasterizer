find_package(fastgltf CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(X11 REQUIRED)

find_path(FENSTER_INCLUDE_DIRS "fenster.h")

add_executable(rasterizer)
target_link_libraries(
  rasterizer
  PRIVATE
    fastgltf::fastgltf
    glm::glm
    X11::X11
)
target_include_directories(
  rasterizer
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Stb_INCLUDE_DIR}
    ${FENSTER_INCLUDE_DIRS}
)
target_compile_definitions(
  rasterizer
  PRIVATE
    FENSTER_HEADER
    GLM_ENABLE_EXPERIMENTAL
    GLM_FORCE_INLINE
)
target_sources(
  rasterizer
  PRIVATE
    buffer/framebuffer.cpp
    impl/fenster.cpp
    impl/stb.cpp
    model/loading.cpp
    model/material.cpp
    platform/input.cpp
    platform/window.cpp
    rasterization/bin.cpp
    rasterization/camera.cpp
    rasterization/rasterizer.cpp
    rasterization/thread_pool.cpp
    rasterization/triangle.cpp
    utils/cli.cpp
    main.cpp
)

if(USE_SIMD)
  include(${CMAKE_SOURCE_DIR}/cmake/simd.cmake)
  target_link_libraries(
    rasterizer
    PRIVATE
      simd
  )
endif()
