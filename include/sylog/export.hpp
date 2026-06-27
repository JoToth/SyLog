#pragma once

// Shared library export/import
#if defined(_WIN32) || defined(__CYGWIN__)
  #if defined(SYLOG_BUILDING_LIBRARY)
    #define SYLOG_API __declspec(dllexport)
  #elif defined(SYLOG_USING_LIBRARY)
    #define SYLOG_API __declspec(dllimport)
  #else
    #define SYLOG_API
  #endif
#else
  #if defined(SYLOG_BUILDING_LIBRARY)
    #define SYLOG_API __attribute__((visibility("default")))
  #else
    #define SYLOG_API
  #endif
#endif
