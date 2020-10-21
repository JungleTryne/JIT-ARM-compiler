#pragma once

#define DEBUG

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
private:

    enum class ARM_INSTRUCTION {
        ADD,            //r0 += r1
        SUB,            //r0 -= r1
        MUL,            //r0 *= r1

        BLX,            //blx *function*

        LDR_FROM_NEXT,  //ldr r_i [pc, #4]
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
        LR, PC
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
    void handle_const(Node* current);
    void handle_variable(Node* current);
    void handle_plus(Node* current);
    void handle_minus(Node* current);
    void handle_product(Node* current);

    void handle_function(Node* current);
};

template<typename OutputIterator>
void ARM_JIT_Compiler::print_assembly(OutputIterator& output) {
    std::for_each(instructions_.begin(),
                  instructions_.end(),
                  [&output](const instruction_t& instruction)
                  {
                      std::string param_1 = std::get<1>(instruction) ?
                                            std::to_string(static_cast<int>(*std::get<1>(instruction))) : "";

                      std::string param_2 = std::get<2>(instruction) ?
                                            std::to_string(static_cast<int>(*std::get<2>(instruction))) : "";;


                      switch (std::get<0>(instruction)) {
                          case ARM_I::ADD:
                              *output = std::string("add\t\t") +
                                        "r" + param_1 + ",\t" +
                                        "r" + param_2 + ",\t" +
                                        "r" + param_1 + "\n";
                              break;

                          case ARM_I::SUB:
                              *output = std::string("sub\t\t") +
                                        "r" + param_1 + ",\t" +
                                        "r" + param_2 + ",\t" +
                                        "r" + param_1 + "\n";
                              break;

                          case ARM_I::MUL:
                              *output = std::string("mul\t\t") +
                                        "r" + param_1 + ",\t" +
                                        "r" + param_2 + ",\t" +
                                        "r" + param_1 + "\n";
                              break;

                          case ARM_I::BLX:
                              *output = std::string("blx\t\t") +
                                        "r" + param_1 + "\n";
                              break;

                          case ARM_I::LDR_FROM_NEXT:
                              *output = std::string("ldr\t\t") +
                                        "r" + param_1 + ",\t" +
                                        "[pc, #4]" + "\n";
                              break;

                          case ARM_I::LDR_REG:
                              *output = std::string("ldr\t\t") +
                                        "r" + param_1 + ",\t" +
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
                              *output = std::string(".word\t") +
                                        std::string(*std::get<3>(instruction)) + "\n";
                              break;

                          case ARM_I::POP_MULT_REG:
                              *output = std::string("pop\t\t{r") +
                                        param_1 + "-r" +
                                        param_2 +
                                        + "}\n";
                              break;

                          default:
                              *output = std::string("UNKNOWN_INSTRUCTION\n");
                              break;
                      }
                      ++output;
                  });
}