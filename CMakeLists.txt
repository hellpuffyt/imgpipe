cmake_minimum_required(VERSION 3.16)
project(imgpipe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(IMGPIPE_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)
option(IMGPIPE_ENABLE_SANITIZERS "Build with -fsanitize=address,undefined" OFF)
option(IMGPIPE_BUILD_TESTS "Build the test suite" ON)

add_library(imgpipe_warnings INTERFACE)
if(MSVC)
  target_compile_options(imgpipe_warnings INTERFACE /W4)
  if(IMGPIPE_WARNINGS_AS_ERRORS)
    target_compile_options(imgpipe_warnings INTERFACE /WX)
  endif()
else()
  target_compile_options(imgpipe_warnings INTERFACE -Wall -Wextra -Wpedantic)
  if(IMGPIPE_WARNINGS_AS_ERRORS)
    target_compile_options(imgpipe_warnings INTERFACE -Werror)
  endif()
endif()

add_library(imgpipe_sanitizers INTERFACE)
if(IMGPIPE_ENABLE_SANITIZERS AND NOT MSVC)
  target_compile_options(imgpipe_sanitizers INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer -g)
  target_link_options(imgpipe_sanitizers INTERFACE -fsanitize=address,undefined)
endif()

add_library(imgpipe_core
  src/image.cpp
  src/ops_resize.cpp
  src/ops_blur.cpp
  src/ops_color.cpp
  src/ops_convolution.cpp
  src/ops_transform.cpp
  src/pipeline.cpp
  src/bench.cpp
)
target_include_directories(imgpipe_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(imgpipe_core PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
)
target_link_libraries(imgpipe_core PRIVATE imgpipe_warnings imgpipe_sanitizers)
target_link_libraries(imgpipe_core PUBLIC imgpipe_sanitizers)

# stb's implementation files trip -Wpedantic/-Wextra warnings that are not
# ours to fix; relax warnings specifically for the translation unit that
# includes them.
if(NOT MSVC)
  set_source_files_properties(src/image.cpp PROPERTIES
    COMPILE_OPTIONS "-Wno-unused-function;-Wno-missing-field-initializers"
  )
endif()

add_executable(imgpipe src/main.cpp)
target_link_libraries(imgpipe PRIVATE imgpipe_core imgpipe_warnings imgpipe_sanitizers)

if(IMGPIPE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
