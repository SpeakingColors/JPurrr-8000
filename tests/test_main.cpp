#include <string>
#include "test_util.h"
int g_fail = 0;
int main(int argc, char** argv) {
    const char* only = argc > 1 ? argv[1] : nullptr;
    int ran = 0;
    for (auto& t : TestReg::list()) {
        if (only && std::string(t.first).find(only) == std::string::npos) continue;
        int before = g_fail; t.second(); ++ran;
        std::printf("%s %s\n", g_fail == before ? "ok  " : "FAIL", t.first);
    }
    std::printf("%d tests, %s\n", ran, g_fail ? "FAILED" : "all passed");
    return g_fail ? 1 : 0;
}
