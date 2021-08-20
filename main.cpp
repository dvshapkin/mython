#include "tests/test_runner_p.h"

#include <iostream>

namespace parse {
    void RunOpenLexerTests(TestRunner& tr);
}

namespace runtime {
    void RunObjectHolderTests(TestRunner& tr);
    void RunObjectsTests(TestRunner& tr);
}

namespace {

    void TestAll() {
        TestRunner tr;
        //-------------------------------------- Lexer"
        parse::RunOpenLexerTests(tr);
        //-------------------------------------- Runtime"
        runtime::RunObjectHolderTests(tr);
        runtime::RunObjectsTests(tr);
    }

}  // namespace

int main() {
    try {
        TestAll();
    } catch (const std::exception& e) {
        std::cerr << e.what();
        return 1;
    }
    return 0;
}
