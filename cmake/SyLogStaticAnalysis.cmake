# SyLogStaticAnalysis.cmake

function(sylog_add_static_analysis_targets)
  set(options)
  set(oneValueArgs TARGET)
  set(multiValueArgs TIDY_FILES CPPCHECK_SUPPRESS)

  cmake_parse_arguments(SA "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if (NOT SA_TARGET)
    message(FATAL_ERROR "sylog_add_static_analysis_targets: TARGET is required")
  endif()

  # ---- clang-tidy ----
  find_program(CLANG_TIDY_EXE NAMES clang-tidy clang-tidy.exe)
  if (CLANG_TIDY_EXE)
    if (NOT SA_TIDY_FILES)
      message(WARNING "clang-tidy target enabled but no TIDY_FILES provided.")
    endif()

    add_custom_target(tidy
      COMMAND ${CLANG_TIDY_EXE} -p ${CMAKE_BINARY_DIR} ${SA_TIDY_FILES}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "Run clang-tidy"
      VERBATIM
    )
  else()
    add_custom_target(tidy
      COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy not found; install LLVM/clang tools."
    )
  endif()

  # ---- cppcheck ----
  find_program(CPPCHECK_EXE NAMES cppcheck cppcheck.exe)
  if (CPPCHECK_EXE)
    set(suppress_args "")
    foreach(sup ${SA_CPPCHECK_SUPPRESS})
      list(APPEND suppress_args "--suppress=${sup}")
    endforeach()

    add_custom_target(cppcheck
      COMMAND ${CPPCHECK_EXE}
        --project=${CMAKE_BINARY_DIR}/compile_commands.json
        --std=c++20
        --enable=warning,style,performance,portability
        --inconclusive
        --inline-suppr
        ${suppress_args}
        --error-exitcode=1
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "Run cppcheck"
      VERBATIM
    )
  else()
    add_custom_target(cppcheck
      COMMAND ${CMAKE_COMMAND} -E echo "cppcheck not found; install cppcheck."
    )
  endif()

endfunction()
