#include <exception>
#include <iostream>

int RunLoggerTests();
int RunCaptureTraceTests();
int RunFixedMapCaptureStateTests();
int RunFixedMapIdentityProbeTests();
int RunFixedMapLoadHookTests();
int RunMapPathValidatorTests();
int RunHookSiteLayoutTests();
int RunListAttributionTests();
int RunListenerRemovalTests();
int RunModuleInventoryTests();
int RunMsvcX86WideStringTests();
int RunPageObservationTests();
int RunRuntimePolicyTests();
int RunStopRequestTests();
int RunSuMapValueTests();
int RunSuLuaMapBridgeTests();
int RunLuaMapSessionTests();
int RunMapIdentityCoordinatorTests();
int RunLaunchOriginTests();
int RunMapSessionPolicyTests();
int RunSettlementUiProbeTests();
int RunVictoryEventProbeTests();
int RunNativeEventAdmissionTests();
int RunNativeVictoryEventSubscriberTests();
int RunPhase3TraceTests();
int RunPluginPathsTests();
int RunCompletionRecordTests();
int RunCompletionCandidateCoordinatorTests();
int RunCompletionJsonTests();

int main() {
    try {
        RunLoggerTests();
        RunCaptureTraceTests();
        RunFixedMapCaptureStateTests();
        RunFixedMapIdentityProbeTests();
        RunFixedMapLoadHookTests();
        RunMapPathValidatorTests();
        RunHookSiteLayoutTests();
        RunListAttributionTests();
        RunListenerRemovalTests();
        RunModuleInventoryTests();
        RunMsvcX86WideStringTests();
        RunPageObservationTests();
        RunStopRequestTests();
        RunSuMapValueTests();
        RunSuLuaMapBridgeTests();
        RunLuaMapSessionTests();
        RunMapIdentityCoordinatorTests();
        RunLaunchOriginTests();
        RunMapSessionPolicyTests();
        RunSettlementUiProbeTests();
        RunVictoryEventProbeTests();
        RunNativeEventAdmissionTests();
        RunNativeVictoryEventSubscriberTests();
        RunPhase3TraceTests();
        RunPluginPathsTests();
        RunCompletionRecordTests();
        RunCompletionCandidateCoordinatorTests();
        RunCompletionJsonTests();
        return RunRuntimePolicyTests();
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
