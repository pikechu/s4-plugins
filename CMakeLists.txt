cmake_minimum_required(VERSION 3.24)
project(CampaignCompletion LANGUAGES CXX)

include(cmake/VerifyWin32.cmake)
include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/dllmain.cpp")
  add_library(CampaignCompletionDebug SHARED
    src/dllmain.cpp
    src/diagnostics/Logger.cpp
    src/diagnostics/ModuleInventory.cpp
    src/runtime/DiagnosticRuntime.cpp
    src/runtime/S4Listeners.cpp
  )

  target_include_directories(CampaignCompletionDebug PRIVATE
    src
    third_party/S4ModApi
  )
  target_compile_definitions(CampaignCompletionDebug PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
  )
  target_compile_options(CampaignCompletionDebug PRIVATE /W4 /WX /permissive-)
  target_link_libraries(CampaignCompletionDebug PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/S4ModApi/S4ModApi.lib"
    Version Advapi32 Crypt32 Psapi
  )
  set_target_properties(CampaignCompletionDebug PROPERTIES
    OUTPUT_NAME "CampaignCompletionDebug"
    SUFFIX ".asi"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  )
endif()

add_executable(campaign_completion_tests
  tests/TestMain.cpp
  tests/LoggerTests.cpp
)
target_include_directories(campaign_completion_tests PRIVATE src)
target_compile_definitions(campaign_completion_tests PRIVATE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
  UNICODE
  _UNICODE
)
target_compile_options(campaign_completion_tests PRIVATE /W4 /WX /permissive-)
target_link_libraries(campaign_completion_tests PRIVATE Version Advapi32 Crypt32 Psapi)
set_target_properties(campaign_completion_tests PROPERTIES
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

add_test(NAME campaign_completion_tests COMMAND campaign_completion_tests)
