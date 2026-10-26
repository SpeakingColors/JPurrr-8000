#pragma once
#include <cstdio>
#include <utility>
#include <vector>
struct TestReg {
    using Fn = void (*)();
    static std::vector<std::pair<const char*, Fn>>& list() { static std::vector<std::pair<const char*, Fn>> l; return l; }
    TestReg(const char* n, Fn f) { list().push_back({ n, f }); }
};
extern int g_fail;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)
#define TEST(name) static void name(); static TestReg reg_##name(#name, name); static void name()
