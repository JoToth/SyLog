add_executable(SyLogTests
  test_main.cpp
  test_async.cpp
  test_filters.cpp
  test_renderers.cpp
  test_queue_strategies.cpp
  test_crossbar_sink.cpp
  test_file_rotation.cpp
  test_pipeline_lifecycle.cpp
)

if(NOT TARGET cpptest::cpptest)
  message(FATAL_ERROR "SyLog tests require cpptest::cpptest from deps.cmake/cppdeps")
endif()

target_link_libraries(SyLogTests PRIVATE SyLog cpptest::cpptest)

target_include_directories(SyLogTests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(SyLogTests PRIVATE cxx_std_20)

add_test(NAME SyLogTests COMMAND SyLogTests)

sylog_apply_options(SyLogTests)


add_executable(SyLogStressTests
  test_stress_main.cpp
  test_pipeline_stress.cpp
)

target_link_libraries(SyLogStressTests PRIVATE SyLog cpptest::cpptest)
target_include_directories(SyLogStressTests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(SyLogStressTests PRIVATE cxx_std_20)
add_test(NAME SyLogStressTests COMMAND SyLogStressTests)
set_tests_properties(SyLogStressTests PROPERTIES LABELS "stress" TIMEOUT 15)
sylog_apply_options(SyLogStressTests)
