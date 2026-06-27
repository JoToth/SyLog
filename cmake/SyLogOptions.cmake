# SyLogOptions.cmake
# Central place for build options and target configuration helpers.

include_guard(GLOBAL)

# Core build switches
option(SYLOG_BUILD_TESTS "Build SyLog tests" ON)
option(SYLOG_BUILD_EXAMPLES "Build SyLog examples" ON)
option(SYLOG_BUILD_BENCHMARKS "Build SyLog benchmarks" ON)
option(SYLOG_BUILD_TOOLS "Build SyLog command-line tools" OFF)

option(SYLOG_BUILD_SHARED "Build SyLog as a shared library" OFF)

# Developer / toolchain switches
option(SYLOG_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(SYLOG_ENABLE_LTO "Enable link-time optimization (IPO/LTO) when supported" OFF)

option(SYLOG_ENABLE_ASAN "Enable AddressSanitizer (Clang/GCC)" OFF)
option(SYLOG_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer (Clang/GCC)" OFF)
option(SYLOG_ENABLE_TSAN "Enable ThreadSanitizer (Clang/GCC)" OFF)

# Helper: apply common compile/link options to a target
function(sylog_apply_options target_name)
  if(NOT TARGET ${target_name})
    message(FATAL_ERROR "sylog_apply_options: target '${target_name}' does not exist")
  endif()

  # Warnings
  if(MSVC)
    target_compile_options(${target_name} PRIVATE /W4)
    if(SYLOG_WARNINGS_AS_ERRORS)
      target_compile_options(${target_name} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    if(SYLOG_WARNINGS_AS_ERRORS)
      target_compile_options(${target_name} PRIVATE -Werror)
    endif()
  endif()

  # LTO / IPO
  if(SYLOG_ENABLE_LTO)
    set_property(TARGET ${target_name} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()

  # Sanitizers (only meaningful on non-MSVC toolchains)
  if(NOT MSVC)
    set(_sylog_san "")
    if(SYLOG_ENABLE_ASAN)
      list(APPEND _sylog_san "address")
    endif()
    if(SYLOG_ENABLE_UBSAN)
      list(APPEND _sylog_san "undefined")
    endif()
    if(SYLOG_ENABLE_TSAN)
      list(APPEND _sylog_san "thread")
    endif()

    if(_sylog_san)
      list(JOIN _sylog_san "," _sylog_san_joined)
      target_compile_options(${target_name} PRIVATE -fsanitize=${_sylog_san_joined})
      target_link_options(${target_name} PRIVATE -fsanitize=${_sylog_san_joined})
    endif()
  endif()
endfunction()
