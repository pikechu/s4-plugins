#include <exception>
#include <iostream>

int RunLoggerTests();
int RunModuleInventoryTests();
int RunRuntimePolicyTests();

int main() {
    try {
        RunLoggerTests();
        RunModuleInventoryTests();
        return RunRuntimePolicyTests();
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
