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
)
target_sources(
  rasterizer
  PRIVATE
    buffer/framebuffer.cpp
    impl/fenster.cpp
    impl/stb.cpp
    model/loading.cpp
    model/material.cpp
    platform/window.cpp
    utils/cli.cpp
    main.cpp
)
