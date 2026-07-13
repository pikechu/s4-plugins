#include <exception>
#include <iostream>

int RunLoggerTests();

int main() {
    try {
        return RunLoggerTests();
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
