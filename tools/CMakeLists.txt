option(SYLOG_BUILD_TOOLS "Build SyLog command-line tools" OFF)

if(SYLOG_BUILD_TOOLS)
  if(NOT TARGET argtree::argtree)
    message(FATAL_ERROR "sylogctl requires argtree::argtree from deps.cmake/cppdeps")
  endif()

  add_executable(sylogctl sylogctl.cpp)
  target_link_libraries(sylogctl PRIVATE SyLog argtree::argtree)
  target_compile_features(sylogctl PRIVATE cxx_std_20)
  target_compile_definitions(sylogctl PRIVATE SYLOG_HAVE_ARGTREE=1)

  sylog_apply_options(sylogctl)
endif()
