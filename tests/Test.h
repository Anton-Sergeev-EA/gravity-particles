#pragma once
// Минимальный тестовый каркас без внешних зависимостей.
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace test {

struct Case {
    const char* name;
    void (*fn)();
};

std::vector<Case>& registry();
void fail(const char* file, int line, const std::string& message);

struct Registrar {
    Registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

template <typename A, typename B>
void checkEqual(const A& a, const B& b, const char* ea, const char* eb, const char* file, int line) {
    if (!(a == b)) {
        std::ostringstream os;
        os << ea << " == " << eb << "\n      left:  " << a << "\n      right: " << b;
        fail(file, line, os.str());
    }
}

}  // namespace test

#define TEST(name)                                              \
    static void name();                                         \
    static const test::Registrar registrar_##name(#name, name); \
    static void name()

#define CHECK(cond)                                         \
    do {                                                    \
        if (!(cond)) test::fail(__FILE__, __LINE__, #cond); \
    } while (0)

#define CHECK_EQ(a, b) test::checkEqual((a), (b), #a, #b, __FILE__, __LINE__)
