cmake_minimum_required(VERSION 3.21)
project(SyLog VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(SyLogOptions)
include(SyLogStaticAnalysis OPTIONAL)

# Dependencies are part of the project contract, not optional feature flags.
# deps.cmake must resolve these targets before SyLog is configured:
#   Workers::Workers
#   cpptest::cpptest
#   argtree::argtree
include(cmake/CppDeps.cmake)
include(deps.cmake)
cppdeps_install_all(WRITE_LOCKFILE)
include(SyLogWorkers)

add_subdirectory(src)

if(SYLOG_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(SYLOG_BUILD_TOOLS)
  add_subdirectory(tools)
endif()

if(SYLOG_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()

if(SYLOG_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# ---- Install/export package ----
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(TARGETS SyLog
  EXPORT SyLogTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT SyLogTargets
  NAMESPACE SyLog::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SyLog
)

configure_package_config_file(
  ${PROJECT_SOURCE_DIR}/cmake/SyLogConfig.cmake.in
  ${PROJECT_BINARY_DIR}/SyLogConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SyLog
)

write_basic_package_version_file(
  ${PROJECT_BINARY_DIR}/SyLogConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  ${PROJECT_BINARY_DIR}/SyLogConfig.cmake
  ${PROJECT_BINARY_DIR}/SyLogConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SyLog
)
