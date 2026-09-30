cmake_minimum_required(VERSION 3.24)
project(MusicDuplicatedRepair LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.."
                       ABSOLUTE)

if(WIN32)
  if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "MusicLoopRepair must be configured as 32-bit")
  endif()

  add_library(MusicLoopRepair SHARED
    src/music_repair/MusicRepairDllMain.cpp
    src/music_repair/MusicRepairRuntime.cpp
    src/music_repair/MusicIatHooks.cpp
    src/music_repair/MusicEventTracker.cpp
    "${REPOSITORY_ROOT}/CampaignMarker/src/diagnostics/Logger.cpp"
    "${REPOSITORY_ROOT}/CampaignMarker/src/diagnostics/ModuleInventory.cpp"
  )
  target_include_directories(MusicLoopRepair PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${REPOSITORY_ROOT}/CampaignMarker/src"
  )
  target_compile_definitions(MusicLoopRepair PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
  )
  if(MSVC)
    target_compile_options(MusicLoopRepair PRIVATE /W4 /WX /permissive-)
    target_link_options(MusicLoopRepair PRIVATE /INCREMENTAL:NO)
  else()
    target_compile_options(MusicLoopRepair PRIVATE -Wall -Wextra -Werror)
    target_link_options(MusicLoopRepair PRIVATE -static)
  endif()
  if(MSVC)
    target_link_libraries(MusicLoopRepair PRIVATE Version Advapi32 Crypt32)
  else()
    target_link_libraries(MusicLoopRepair PRIVATE version advapi32 crypt32)
  endif()
  add_custom_target(music_diagnostic_package
    COMMAND "${CMAKE_COMMAND}"
      "-DASI_PATH=$<TARGET_FILE:MusicLoopRepair>"
      "-DINI_PATH=${CMAKE_CURRENT_SOURCE_DIR}/config/MusicLoopRepair.ini.example"
      "-DPACKAGE_ROOT=${CMAKE_CURRENT_BINARY_DIR}/music-diagnostic-package"
      "-DZIP_PATH=${CMAKE_CURRENT_BINARY_DIR}/MusicLoopRepair-diagnostic.zip"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PackageMusicDiagnostic.cmake"
    DEPENDS MusicLoopRepair
    VERBATIM
  )
  set_target_properties(MusicLoopRepair PROPERTIES
    OUTPUT_NAME "MusicLoopRepair"
    PREFIX ""
    SUFFIX ".asi"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  )
endif()

if(BUILD_TESTING)
  add_executable(music_event_tracker_tests
    tests/MusicEventTrackerTests.cpp
    src/music_repair/MusicEventTracker.cpp
  )
  target_include_directories(music_event_tracker_tests PRIVATE src)
  target_compile_definitions(music_event_tracker_tests PRIVATE
    MUSIC_EVENT_TRACKER_STANDALONE
  )
  if(MSVC)
    target_compile_options(music_event_tracker_tests PRIVATE
      /W4 /WX /permissive-
    )
  else()
    target_compile_options(music_event_tracker_tests PRIVATE
      -Wall -Wextra -Werror
    )
  endif()
  add_test(NAME music_event_tracker_tests COMMAND music_event_tracker_tests)
endif()
