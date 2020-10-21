#include "JIT_compiler.h"

int main() {
    ExpressionParser parser("(1+a)*c + div(2+4,2)");
    ARM_JIT_Compiler compiler({});
    TransferParsingTree(parser, compiler);
    compiler.compile();

    auto out_iter = std::ostream_iterator<std::string>(std::cout, "");
    compiler.print_assembly(out_iter);
    return 0;
}