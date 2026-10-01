#include <cstring>
#include <exception>

#include "Test.h"

namespace test {

namespace {
int g_failures = 0;
bool g_currentFailed = false;
}  // namespace

std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

void fail(const char* file, int line, const std::string& message) {
    ++g_failures;
    g_currentFailed = true;
    std::cerr << "    FAIL " << file << ":" << line << "\n      " << message << "\n";
}

}  // namespace test

int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0, failedCases = 0;
    for (const auto& c : test::registry()) {
        if (filter && !std::strstr(c.name, filter)) continue;
        ++run;
        test::g_currentFailed = false;
        try {
            c.fn();
        } catch (const std::exception& e) {
            test::fail(__FILE__, __LINE__, std::string("exception: ") + e.what());
        }
        std::cout << (test::g_currentFailed ? "[FAIL] " : "[ OK ] ") << c.name << "\n";
        if (test::g_currentFailed) ++failedCases;
    }
    std::cout << "\n" << run - failedCases << "/" << run << " tests passed\n";
    return failedCases == 0 ? 0 : 1;
}
