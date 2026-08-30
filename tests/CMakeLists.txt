add_executable(imgpipe_tests
  test_main.cpp
  test_image.cpp
  test_resize.cpp
  test_blur.cpp
  test_color.cpp
  test_convolution.cpp
  test_transform.cpp
  test_pipeline.cpp
  test_bench.cpp
)
target_link_libraries(imgpipe_tests PRIVATE imgpipe_core imgpipe_warnings imgpipe_sanitizers)
target_include_directories(imgpipe_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME imgpipe_tests COMMAND imgpipe_tests WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
