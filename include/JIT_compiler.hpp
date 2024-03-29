#pragma once

#include <fstream>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <tuple>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using str_iter = std::string::iterator;
using str_iter_const = std::string::const_iterator;

class ARM_JIT_Compiler;

/* ExpressionParser class
 * This class converts the given expression into tree
 * which can be used for ARM code generation
 */

enum class ExpressionType {
    Constant,
    Default,
    Variable,
    Plus,
    Minus,
    Product,
    Function
};

struct Node {
    ExpressionType type = ExpressionType::Default;
    std::optional<std::string> content;
    std::vector<std::unique_ptr<Node>> sub_expressions = {};
};

class ExpressionParser {
public:
    explicit ExpressionParser(std::string expression);
    friend void TransferParsingTree(ExpressionParser& parser, ARM_JIT_Compiler& compiler);
private:
    std::string expression_;
    std::unique_ptr<Node> root_;

    static size_t GetPriority(ExpressionType operation);
    void GetRidOfSpaces();

    static auto FindArithmeticOperation(str_iter left, str_iter right)
    -> std::pair<std::optional<ExpressionType>, str_iter>;

    void ParseExpression(Node* current_node, str_iter left, str_iter right);
    bool IsParBalance(str_iter left, str_iter right);

    void ParseConstant(Node* current_node, str_iter left, str_iter right);
    void ParseArithmetic(Node *current_node, str_iter left, str_iter right, ExpressionType type, str_iter pos);
    void ParseFunction(Node *current_node, str_iter left, str_iter right);
    void ParseVariable(Node *current_node, str_iter left, str_iter right);

    static ExpressionType GetTypeFromChar(char current_char);

    inline bool IsConstant(str_iter left) const;
    inline bool IsFunction(str_iter left, str_iter right) const;
    std::string GetFunctionName(str_iter_const left, str_iter_const right) const;
    std::vector<std::pair<str_iter, str_iter>> GetFunctionParameters(str_iter left, str_iter right) const;
};


class ARM_JIT_Compiler {
public:
    explicit ARM_JIT_Compiler(std::map<std::string, void*>  address_map);
    friend void TransferParsingTree(ExpressionParser& parser, ARM_JIT_Compiler& compiler);
    void compile();

    template<typename OutputIterator>
    void print_assembly(OutputIterator& output);
    std::vector<uint32_t> GetCompiledBinary();

private:

    enum class ARM_INSTRUCTION {
        ADD,            //r0 += r1
        SUB,            //r0 -= r1
        MUL,            //r0 *= r1

        BLX,            //blx *function*

        LDR_FROM_NEXT,  //ldr r_i [pc, #-4]
        LDR_REG,        //reading from address in register (Example: ldr r_i, [r_j])

        PUSH_MULT_REG,  //pushing several registers
        PUSH_REG,       //pushing register

        POP_MULT_REG,   //popping several registers
        POP_REG,        //popping register

        WORD_DECL,      //.word declaration
    };
    enum class ARM_REGISTER {
        R0 = 0,
        R1 = 1,
        R2 = 2,
        R3 = 3,
        R4 = 4,
        LR = 5,
        PC = 6
    };

    using ARM_R = ARM_REGISTER;

    using ARM_I = ARM_INSTRUCTION;

    using instruction_t = std::tuple<ARM_INSTRUCTION,
            std::optional<ARM_REGISTER>,
            std::optional<ARM_REGISTER>,
            std::optional<std::string>>;
    std::vector<instruction_t> instructions_;
    std::unique_ptr<Node> parse_tree_;

    std::map<std::string, void*> address_map_;
    void compile_(Node* current);

    void add_header();
    void add_footer();

    void handle_const(Node* current);
    void handle_variable(Node* current);
    void handle_plus(Node* current);
    void handle_minus(Node* current);
    void handle_product(Node* current);
    void handle_function(Node* current);
};

template<typename OutputIterator>
void ARM_JIT_Compiler::print_assembly(OutputIterator& output) {
    bool skipped = false;
    size_t counter = 0;

    for(const auto& instruction : instructions_) {
        std::string param_1 = std::get<1>(instruction) ?
                              std::to_string(static_cast<int>(*std::get<1>(instruction))) : "";

        std::string param_2 = std::get<2>(instruction) ?
                              std::to_string(static_cast<int>(*std::get<2>(instruction))) : "";


        switch (std::get<0>(instruction)) {
            case ARM_I::ADD:
                *output = std::string("add\t") +
                          "r" + param_1 + ", " +
                          "r" + param_2 + ", " +
                          "r" + param_1 + "\n";
                break;

            case ARM_I::SUB:
                *output = std::string("sub\t") +
                          "r" + param_1 + ", " +
                          "r" + param_2 + ", " +
                          "r" + param_1 + "\n";
                break;

            case ARM_I::MUL:
                *output = std::string("mul\t") +
                          "r" + param_1 + ", " +
                          "r" + param_2 + ", " +
                          "r" + param_1 + "\n";
                break;

            case ARM_I::BLX:
                *output = std::string("blx\t") +
                          "r" + param_1 + "\n";
                break;

            case ARM_I::LDR_FROM_NEXT:
                *output = std::string("ldr\t") +
                          "r" + param_1 + ", " +
                          "[pc]" + "\n";
                break;

            case ARM_I::LDR_REG:
                *output = std::string("ldr\t") +
                          "r" + param_1 + ", " +
                          "[" + "r" + param_2 + "]" + "\n";
                break;

            case ARM_I::PUSH_REG:
                *output = std::string("push\t{r") +
                          param_1 + "}\n";
                break;

            case ARM_I::PUSH_MULT_REG:
                *output = std::string("push\t{r") +
                          param_1 + "-r" +
                          param_2 +
                          + "}\n";
                break;

            case ARM_I::WORD_DECL:

                *output = std::string("b\tskip") + std::to_string(counter) +
                          std::string("\n.word\t") +
                          std::string(*std::get<3>(instruction)) + "\n" +
                          std::string("skip") + std::to_string(counter) + std::string(":\n");
                ++counter;
                break;

            case ARM_I::POP_MULT_REG:
                *output = std::string("pop\t{r") +
                          param_1 + "-r" +
                          param_2 +
                          + "}\n";
                break;

            case ARM_I::POP_REG:
                *output = std::string("pop\t{r") +
                          param_1
                          + "}\n";
                break;

            default:
                std::cout << static_cast<size_t>(std::get<0>(instruction));
                *output = std::string("UNKNOWN_INSTRUCTION\n");
                break;
        }

        ++output;
    }
}

typedef struct {
    const char * name;
    void       * pointer;
} symbol_t;

extern void
jit_compile_expression_to_arm(const char * expression,
                              const symbol_t * externs,
                              void * out_buffer);