# SyLog dependency manifest.
#
# Dependencies are mandatory. This manifest is evaluated unconditionally by the
# root CMakeLists.txt through cmake/CppDeps.cmake. There is no offline fallback
# and no vendored replacement target: missing dependencies must fail configure.

cppdeps_add(
  NAME workers
  PROVIDES Workers::Workers
  GIT_REPOSITORY https://github.com/JoToth/workers.git
  TAG v0.1.0-preview
  BUILD CMAKE_ADD_SUBDIRECTORY
  BUILD_TARGET Workers
)

cppdeps_add(
  NAME cpptest
  PROVIDES cpptest::cpptest
  GIT_REPOSITORY https://github.com/cpptest/cpptest.git
  TAG 2.0.0
  BUILD CPPTST_STATIC
)

cppdeps_add(
  NAME argtree
  PROVIDES argtree::argtree
  GIT_REPOSITORY https://github.com/JoToth/argtree.git
  TAG v0.1
  BUILD HEADER_ONLY
)
