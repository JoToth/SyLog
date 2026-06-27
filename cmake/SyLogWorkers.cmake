# Workers dependency integration for SyLog.
# Workers is required. There is intentionally no vendored/offline fallback here:
# deps.cmake/cppdeps must provide Workers::Workers before this file is included.

if(NOT TARGET Workers::Workers)
  message(FATAL_ERROR
    "SyLog requires Workers::Workers. Resolve dependencies through deps.cmake/cppdeps before configuring SyLog.")
endif()
