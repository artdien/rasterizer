find_package(fastgltf CONFIG REQUIRED)
find_package(Stb REQUIRED)

add_executable(rasterizer)
target_link_libraries(
  rasterizer
  PRIVATE
    fastgltf::fastgltf
)
target_include_directories(
  rasterizer
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Stb_INCLUDE_DIR}
)
target_sources(
  rasterizer
  PRIVATE
    main.cpp
)
