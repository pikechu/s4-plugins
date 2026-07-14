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
    src/diagnostics/CaptureTrace.cpp
    src/diagnostics/Logger.cpp
    src/diagnostics/ModuleInventory.cpp
    src/diagnostics/MarkerCalibrationTrace.cpp
    src/diagnostics/Phase3Trace.cpp
    src/completion/CompletionRecord.cpp
    src/completion/CompletionAdmission.cpp
    src/completion/CompletionCandidateCoordinator.cpp
    src/completion/CompletionJson.cpp
    src/completion/CompletionStore.cpp
    src/completion/CompletionWorker.cpp
    src/completion/Win32CompletionFileOps.cpp
    src/identity/ListAttribution.cpp
    src/identity/LuaMapSession.cpp
    src/identity/MapIdentityCoordinator.cpp
    src/identity/SuMapValue.cpp
    src/lua/LuaApi.cpp
    src/lua/SuLuaMapBridge.cpp
    src/marker/CompletionMarkerIndex.cpp
    src/native/NativeEventAdmission.cpp
    src/native/NativeEventRegistration.cpp
    src/native/NativeVictoryEventSubscriber.cpp
    src/runtime/CallbackGate.cpp
    src/runtime/DiagnosticRuntime.cpp
    src/runtime/ListenerRemoval.cpp
    src/runtime/PageObservation.cpp
    src/runtime/PluginPaths.cpp
    src/runtime/S4Listeners.cpp
    src/runtime/StopRequest.cpp
    src/victory/LaunchOrigin.cpp
    src/victory/MapSessionPolicy.cpp
    src/victory/SettlementUiProbe.cpp
    src/victory/VictoryEventProbe.cpp
  )

  target_include_directories(CampaignCompletionDebug PRIVATE
    src
    third_party/S4ModApi
  )
  target_compile_definitions(CampaignCompletionDebug PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
  )
  target_compile_options(CampaignCompletionDebug PRIVATE /W4 /WX /permissive-)
  target_link_options(CampaignCompletionDebug PRIVATE /INCREMENTAL:NO)
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
  tests/CaptureTraceTests.cpp
  tests/FixedMapCaptureStateTests.cpp
  tests/FixedMapIdentityProbeTests.cpp
  tests/FixedMapLoadHookTests.cpp
  tests/HookSiteLayoutTests.cpp
  tests/LoggerTests.cpp
  tests/MapPathValidatorTests.cpp
  tests/ListAttributionTests.cpp
  tests/ListenerRemovalTests.cpp
  tests/ModuleInventoryTests.cpp
  tests/MsvcX86WideStringTests.cpp
  tests/PageObservationTests.cpp
  tests/RuntimePolicyTests.cpp
  tests/StopRequestTests.cpp
  tests/SuMapValueTests.cpp
  tests/SuLuaMapBridgeTests.cpp
  tests/LuaMapSessionTests.cpp
  tests/MapIdentityCoordinatorTests.cpp
  tests/LaunchOriginTests.cpp
  tests/MapSessionPolicyTests.cpp
  tests/SettlementUiProbeTests.cpp
  tests/VictoryEventProbeTests.cpp
  tests/NativeEventAdmissionTests.cpp
  tests/NativeVictoryEventSubscriberTests.cpp
  tests/Phase3TraceTests.cpp
  tests/PluginPathsTests.cpp
  tests/CompletionRecordTests.cpp
  tests/CompletionCandidateCoordinatorTests.cpp
  tests/CompletionJsonTests.cpp
  tests/CompletionStoreTests.cpp
  tests/Win32CompletionFileOpsTests.cpp
  tests/CompletionWorkerTests.cpp
  tests/CompletionRuntimeFlowTests.cpp
  tests/MarkerCalibrationTraceTests.cpp
  tests/CompletionMarkerIndexTests.cpp
  src/diagnostics/CaptureTrace.cpp
  src/diagnostics/Logger.cpp
  src/diagnostics/ModuleInventory.cpp
  src/diagnostics/MarkerCalibrationTrace.cpp
  src/diagnostics/Phase3Trace.cpp
  src/completion/CompletionRecord.cpp
  src/completion/CompletionAdmission.cpp
  src/completion/CompletionCandidateCoordinator.cpp
  src/completion/CompletionJson.cpp
  src/completion/CompletionStore.cpp
  src/completion/CompletionWorker.cpp
  src/completion/Win32CompletionFileOps.cpp
  src/hook/HookSiteLayout.cpp
  src/hook/FixedMapLoadHook.cpp
  src/hook/MsvcX86WideString.cpp
  src/identity/FixedMapCaptureState.cpp
  src/identity/FixedMapIdentityProbe.cpp
  src/identity/ListAttribution.cpp
  src/identity/LuaMapSession.cpp
  src/identity/MapIdentityCoordinator.cpp
  src/identity/MapPathValidator.cpp
  src/identity/SuMapValue.cpp
  src/lua/SuLuaMapBridge.cpp
  src/marker/CompletionMarkerIndex.cpp
  src/runtime/CallbackGate.cpp
  src/runtime/ListenerRemoval.cpp
  src/runtime/PageObservation.cpp
  src/runtime/PluginPaths.cpp
  src/runtime/StopRequest.cpp
  src/victory/LaunchOrigin.cpp
  src/victory/MapSessionPolicy.cpp
  src/victory/SettlementUiProbe.cpp
  src/victory/VictoryEventProbe.cpp
  src/native/NativeEventAdmission.cpp
  src/native/NativeEventRegistration.cpp
  src/native/NativeVictoryEventSubscriber.cpp
)
target_include_directories(campaign_completion_tests PRIVATE
  src
  third_party/S4ModApi
)
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
