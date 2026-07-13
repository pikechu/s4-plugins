#include <exception>
#include <iostream>

int RunLoggerTests();
int RunHookSiteLayoutTests();
int RunListAttributionTests();
int RunListenerRemovalTests();
int RunModuleInventoryTests();
int RunPageObservationTests();
int RunRuntimePolicyTests();
int RunStopRequestTests();

int main() {
    try {
        RunLoggerTests();
        RunHookSiteLayoutTests();
        RunListAttributionTests();
        RunListenerRemovalTests();
        RunModuleInventoryTests();
        RunPageObservationTests();
        RunStopRequestTests();
        return RunRuntimePolicyTests();
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
