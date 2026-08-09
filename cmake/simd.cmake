add_library(simd INTERFACE)
target_compile_options(
  simd
  INTERFACE 
    $<$<CXX_COMPILER_ID:MSVC>:/arch:AVX2>
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-mavx2>
)
target_compile_definitions(
  simd
  INTERFACE 
    GLM_FORCE_INTRINSICS
    GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
)
