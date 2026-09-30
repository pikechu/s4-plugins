cmake_minimum_required(VERSION 3.24)
project(Settlers4Plugins LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(CampaignMarker)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/AIGoodCrashRepair/src/PileRepairDllMain.cpp")
  add_library(PileChainRepair SHARED
    AIGoodCrashRepair/src/PileRepairDllMain.cpp
    AIGoodCrashRepair/src/PileRepairRuntime.cpp
    AIGoodCrashRepair/src/PileChainCore.cpp
    CampaignMarker/src/diagnostics/Logger.cpp
    CampaignMarker/src/diagnostics/ModuleInventory.cpp
  )
  target_include_directories(PileChainRepair PRIVATE
    AIGoodCrashRepair/src
    CampaignMarker/src
    third_party/S4ModApi
  )
  target_compile_definitions(PileChainRepair PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
  )
  target_compile_options(PileChainRepair PRIVATE /W4 /WX /permissive-)
  target_link_options(PileChainRepair PRIVATE /INCREMENTAL:NO)
  target_link_libraries(PileChainRepair PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/S4ModApi/S4ModApi.lib"
    Version Advapi32 Crypt32 Psapi
  )
  set_target_properties(PileChainRepair PROPERTIES
    OUTPUT_NAME "PileChainRepair"
    SUFFIX ".asi"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  )

  add_executable(pile_chain_core_tests
    AIGoodCrashRepair/tests/PileChainCoreTests.cpp
    AIGoodCrashRepair/src/PileChainCore.cpp
  )
  target_include_directories(pile_chain_core_tests PRIVATE
    AIGoodCrashRepair/src
  )
  target_compile_definitions(
    pile_chain_core_tests PRIVATE PILE_CHAIN_CORE_STANDALONE
  )
  target_compile_options(pile_chain_core_tests PRIVATE /W4 /WX /permissive-)
  add_test(NAME pile_chain_core_tests COMMAND pile_chain_core_tests)
  if(WIN32)
    add_test(
      NAME pile_chain_installer_tests
      COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
        "${CMAKE_CURRENT_SOURCE_DIR}/AIGoodCrashRepair/tests/InstallerIntegrationTests.ps1"
    )
  endif()
endif()
