#include <exception>
#include <iostream>

int RunLoggerTests();
int RunFixedMapCaptureStateTests();
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

int main() {
    try {
        RunLoggerTests();
        RunFixedMapCaptureStateTests();
        RunFixedMapLoadHookTests();
        RunMapPathValidatorTests();
        RunHookSiteLayoutTests();
        RunListAttributionTests();
        RunListenerRemovalTests();
        RunModuleInventoryTests();
        RunMsvcX86WideStringTests();
        RunPageObservationTests();
        RunStopRequestTests();
        return RunRuntimePolicyTests();
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
