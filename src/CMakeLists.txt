set(SYLOG_SOURCES
  logger.cpp
  delivery.cpp
  async/workers_event_queue.cpp
  async/sink_crossbar.cpp
  sinks/console_sink.cpp
  sinks/file_sink.cpp
  renderers/pattern_renderer.cpp
  renderers/json_renderer.cpp
)

if(SYLOG_BUILD_SHARED)
  add_library(SyLog SHARED ${SYLOG_SOURCES})
  target_compile_definitions(SyLog PRIVATE SYLOG_BUILDING_LIBRARY)
else()
  add_library(SyLog STATIC ${SYLOG_SOURCES})
endif()

target_include_directories(SyLog
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(SyLog::SyLog ALIAS SyLog)

target_compile_features(SyLog PUBLIC cxx_std_20)

target_link_libraries(SyLog PUBLIC Workers::Workers)

sylog_apply_options(SyLog)
