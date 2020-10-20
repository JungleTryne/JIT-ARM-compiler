/*
 * Danila Mishin
 * ARM Just-In-Time Compiler
 */

#include <algorithm>
#include <cassert>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <tuple>
#include <string>
#include <vector>
#include <utility>

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
    size_t order_number = 0;
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

/* Class constructor */
ExpressionParser::ExpressionParser(std::string expression) : expression_(std::move(expression)) {
    GetRidOfSpaces();
    root_ = std::make_unique<Node>();
    this->ParseExpression(root_.get(), expression_.begin(), expression_.end());
}

/* This function helps to get rid of unnecessary spaces in the expression */
void ExpressionParser::GetRidOfSpaces() {
    std::string expression_without_spaces;
    std::for_each(expression_.begin(), expression_.end(), [&expression_without_spaces](char current){
        if(current != ' ') expression_without_spaces.push_back(current);
    });
    expression_ = expression_without_spaces;
}

/* Finds out if Parenthesis balance is correct for give subexpression */
bool ExpressionParser::IsParBalance(str_iter left, str_iter right) {
    int64_t balance = 0;
    bool correct = true;

    std::for_each(left, right, [&balance, &correct](char current_char) {
        if(balance < 0) correct = false;
        if(current_char == '(') {
            ++balance;
        } else if(current_char == ')') {
            --balance;
        }
    });
    return correct;
}

/* Gives priority of specific operation */
size_t ExpressionParser::GetPriority(ExpressionType operation) {
    switch (operation) {
        case ExpressionType::Plus:
        case ExpressionType::Minus:
            return 0;
        case ExpressionType::Product:
            return 1;
        default:
            return 2;
    }
}

/* Finds arithmetic operation in the given subexpression.
 * If there is no any, returns {std::nullopt, 0}
 */
auto ExpressionParser::FindArithmeticOperation(str_iter left, str_iter right)
-> std::pair<std::optional<ExpressionType>, str_iter> {

    std::optional<ExpressionType> type = std::nullopt;
    auto pos = left;
    int64_t par_balance = 0;

    for(auto current_iter = left; current_iter != right; ++current_iter) {
        char current_char = *current_iter;

        if(current_char == '(') {
            ++par_balance;
        } else if (current_char == ')') {
            --par_balance;
        }

        if(par_balance != 0) {
            continue;
        }

        ExpressionType current_type;
        switch (current_char) {
            case '+':
                current_type = ExpressionType::Plus;
                break;
            case '-':
                current_type = ExpressionType::Minus;
                break;
            case '*':
                current_type = ExpressionType::Product;
                break;
            default:
                current_type = ExpressionType::Default;
                break;
        }

        if(current_type == ExpressionType::Default) {
            continue;
        }

        if(!type) {
            type = current_type;
            pos = current_iter;
        } else {
            size_t current_priority = GetPriority(current_type);
            size_t min_priority = GetPriority(*type);
            if(current_priority < min_priority) {
                type = current_type;
                pos = current_iter;
            }
            ++current_iter;

            //TODO: how to deal with 5*-+-+-+5?
            while(*current_iter == '*' || *current_iter == '-' || *current_iter == '+') ++current_iter;
        }
    }
    return std::make_pair(type, pos);
}

/* Main function for Expression parsing process
 * Recursively called. Builds the tree
 */
void ExpressionParser::ParseExpression(Node* current_node, str_iter left, str_iter right) {
    int64_t surplus_pars = 0;

    while(*(left + surplus_pars) == '(' &&
        *(right - 1 - surplus_pars) == ')' &&
        IsParBalance(left + surplus_pars + 1, right - surplus_pars - 1))
    {
        ++surplus_pars;
    }

    std::advance(left, surplus_pars);
    std::advance(right, -surplus_pars);

    auto [type, pos] = FindArithmeticOperation(left, right);
    if(type) {
        current_node->type = *type;
        current_node->content = std::nullopt;
        for(size_t i = 0; i < 2; ++i) {
            current_node->sub_expressions.push_back(std::make_unique<Node>());
            current_node->sub_expressions[i]->order_number = i;
        }
        ParseExpression(current_node->sub_expressions[0].get(), left, pos);
        ParseExpression(current_node->sub_expressions[1].get(), pos + 1, right);

    } else {
        if(IsConstant(left)) {
            current_node->type = ExpressionType::Constant;
            current_node->content = expression_.substr(std::distance(expression_.begin(), left),
                                                       std::distance(left, right));
        } else if(IsFunction(left, right)) {
            current_node->type = ExpressionType::Function;
            current_node->content = GetFunctionName(left, right);

            size_t counter = 0;
            auto params = GetFunctionParameters(left, right);
            for(auto [pair_left, pair_right] : params) {
                current_node->sub_expressions.push_back(std::make_unique<Node>());
                current_node->sub_expressions[counter]->order_number = counter;
                ParseExpression(current_node->sub_expressions[counter].get(), pair_left, pair_right);
                ++counter;
            }
        } else {
            if(std::distance(left, right) < 1) {
                current_node->type = ExpressionType::Constant;
                current_node->content = "0"; //In case we get -10, left constant will be void, so we make it 0-10
            } else {
                current_node->type = ExpressionType::Variable;
                current_node->content = expression_.substr(std::distance(expression_.begin(), left),
                                                           std::distance(left, right));
            }
        }
    }
}

/* This functions can be called only if expression cannot be divided with arithmetic operation*/
bool ExpressionParser::IsConstant(str_iter left) const {
    return ('0' <= *left && *left <= '9');
}

/* This functions can be called only if expression cannot be divided with arithmetic operation*/
bool ExpressionParser::IsFunction(str_iter left, str_iter right) const {
    for(auto current_iter = left; current_iter != right; ++current_iter) {
        if(*current_iter == '(') {
            return true;
        }
    }
    return false;
}

/* This functions can be called only if expression[left:right] is actually a function*/
std::string ExpressionParser::GetFunctionName(str_iter_const left, str_iter_const right) const {
    auto current_iter = std::find(left, right, '(');

    size_t pos = std::distance(expression_.begin(), left);
    size_t length = std::distance(left, current_iter);

    return expression_.substr(pos, length);
}

/* This functions can be called only if expression[left:right] is actually a function*/
auto ExpressionParser::GetFunctionParameters(str_iter left, str_iter right) const
    -> std::vector<std::pair<str_iter, str_iter>>
{
    std::vector<std::pair<str_iter , str_iter>> answer = {};
    auto current_left = left;

    for(; *current_left != '('; ++current_left);
    ++current_left;

    auto current_right = current_left;

    while(current_right != right) {
        int64_t pron_balance = 0;

        for(; *current_right != ',' || pron_balance > 0; ++current_right) {
            if(*current_right == '(') ++pron_balance;
            if(*current_right == ')') --pron_balance;

            if(pron_balance == -1 && *current_right == ')') {
                answer.emplace_back(current_left, current_right);
                return answer;
            }
        }
        answer.emplace_back(current_left, current_right);
        ++current_right;
        current_left = current_right;
    }
    assert(false);
}

class ARM_JIT_Compiler {
public:
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

    void compile_(Node* current);
    void handle_const(Node* current);
    void handle_variable(Node* current);
    void handle_plus(Node* current);
    void handle_minus(Node* current);
    void handle_product(Node* current);

    void handle_function(Node* current);
};

void TransferParsingTree(ExpressionParser& parser, ARM_JIT_Compiler& compiler) {
    compiler.parse_tree_ = std::move(parser.root_);
}

void ARM_JIT_Compiler::compile() {
    compile_(parse_tree_.get());
}

void ARM_JIT_Compiler::compile_(Node *current) {
    switch (current->type) {
        case ExpressionType::Constant:
            handle_const(current);
            break;
        case ExpressionType::Variable:
            handle_variable(current);
            break;
        case ExpressionType::Plus:
            handle_plus(current);
            break;
        case ExpressionType::Minus:
            handle_minus(current);
            break;
        case ExpressionType::Product:
            handle_product(current);
            break;
        case ExpressionType::Function:
            handle_function(current);
            break;
        case ExpressionType::Default:
            assert(false);
    }
}

void ARM_JIT_Compiler::handle_const(Node *current) {
    /* Handling Constant Type
     * ARM instructions for that:
     *
     * ldr r0, [pc, #4]
     * .word 0x05
     * push {r0}
     *
     * P.S. 0x05 is given for example
     */

    instructions_.emplace_back ( //ldr r0, [pc, #4]
            ARM_I::LDR_FROM_NEXT,
            ARM_R::R0,
            std::nullopt,
            std::nullopt
    );

    instructions_.emplace_back ( //.word *constant*
            ARM_I::WORD_DECL,
            std::nullopt,
            std::nullopt,
            current->content
    );

    instructions_.emplace_back ( // push {r0}
            ARM_I::PUSH_REG,
            ARM_R::R0,
            std::nullopt,
            std::nullopt
    );
}

void ARM_JIT_Compiler::handle_variable(Node *current) {
    /* Handling Variable Type
     * ARM instructions for that:
     *
     * ldr r0, [pc, #4]
     * .word 0xfb1cfcd0
     * ldr r0, [r0]
     * push {r0}
     *
     * P.S. 0xfb1cfcd0 is given for example
     */

    instructions_.emplace_back ( //ldr r0, [pc, #4]
            ARM_I::LDR_FROM_NEXT,
            ARM_R::R0,
            std::nullopt,
            std::nullopt
    );

    instructions_.emplace_back ( //.word *constant*
            ARM_I::WORD_DECL,
            std::nullopt,
            std::nullopt,
            current->content
    );

    instructions_.emplace_back ( //ldr r0, [r0]
            ARM_I::LDR_REG,
            ARM_R::R0,
            ARM_R::R0,
            std::nullopt
    );

    instructions_.emplace_back ( // push {r0}
            ARM_I::PUSH_REG,
            ARM_R::R0,
            std::nullopt,
            std::nullopt
    );
}

void ARM_JIT_Compiler::handle_plus(Node *current) {
    /* Handling Plus operation
     * ARM instructions for that:
     *
     * pop {r0-r1}
     * add r0, r1, r0
     * push {r0}
     */
    compile_(current->sub_expressions[0].get());
    compile_(current->sub_expressions[1].get());

    instructions_.emplace_back ( //pop {r0-r1}
            ARM_I::POP_MULT_REG,
            ARM_R::R0,
            ARM_R::R1,
            std::nullopt
    );

    instructions_.emplace_back( //add r0, r1, r0
            ARM_I::ADD,
            ARM_R::R0,
            ARM_R::R1,
            std::nullopt
    );

    instructions_.emplace_back( //push {r0}
            ARM_I::PUSH_REG,
            ARM_R::R0,
            std::nullopt,
            std::nullopt
    );
}

void ARM_JIT_Compiler::handle_minus(Node *current) {
    /* Handling Minus operation
     * ARM instructions for that:
     *
     * pop {r0-r1}
     * sub r0, r1, r0
     * push {r0}
     */
    compile_(current->sub_expressions[0].get());
    compile_(current->sub_expressions[1].get());

    instructions_.emplace_back ( //pop {r0-r1}
            ARM_I::POP_MULT_REG,
            ARM_R::R0,
            ARM_R::R1,
            std::nullopt
    );

    instructions_.emplace_back( //sub r0, r1, r0
            ARM_I::SUB,
            ARM_R::R0,
            ARM_R::R1,
            std::nullopt
    );

    instructions_.emplace_back( //push {r0}
            ARM_I::PUSH_REG,
            ARM_R::R0,
            std::nullopt,
            std::nullopt
    );
}

void ARM_JIT_Compiler::handle_product(Node *current) {
    /* Handling Product operation
     * ARM instructions for that:
     *
     * pop {r0-r1}
     * mul r0, r1, r0
     * push {r0}
     */

    compile_(current->sub_expressions[0].get());
    compile_(current->sub_expressions[1].get());

    instructions_.emplace_back ( //pop {r0-r1}
            ARM_I::POP_MULT_REG,
            ARM_R::R0,
            ARM_R::R1,
            std::nullopt
    );

    instructions_.emplace_back( //mul r0, r1, r0
            ARM_I::MUL,
            ARM_R::R0,
            ARM_R::R1,
            std::nullopt
    );

    instructions_.emplace_back( //push {r0}
            ARM_I::PUSH_REG,
            ARM_R::R0,
            std::nullopt,
            std::nullopt
    );
}

void ARM_JIT_Compiler::handle_function(Node *current) {
    /* Handling Function call
     * ARM instructions for that:
     *
     * pop {r0-ri}
     * ldr r4, [pc, #4]
     * .word 0xfb1cfcd0
     * blx r4
     * push {r0}
     */

    std::for_each(current->sub_expressions.begin(),
                  current->sub_expressions.end(),
                  [this](const std::unique_ptr<Node>& unique_pointer) {
        compile_(unique_pointer.get());
    });

    size_t arguments_number = current->sub_expressions.size();
    assert(arguments_number > 0);

    instructions_.emplace_back ( //pop {r0-ri}
            ARM_I::POP_MULT_REG,
            ARM_R::R0,
            static_cast<ARM_R>(static_cast<size_t>(ARM_R::R0) + arguments_number - 1),
            std::nullopt
    );

    instructions_.emplace_back ( //ldr r0, [pc, #4]
            ARM_I::LDR_FROM_NEXT,
            ARM_R::R4,
            std::nullopt,
            std::nullopt
    );

    instructions_.emplace_back ( //.word 0xfb1cfcd0
            ARM_I::WORD_DECL,
            std::nullopt,
            std::nullopt,
            current->content
    );

    instructions_.emplace_back(
            ARM_I::BLX,
            ARM_R::R4,
            std::nullopt,
            std::nullopt
    );

    instructions_.emplace_back ( // push {r0}
            ARM_I::PUSH_REG,
            ARM_R::R0,
            std::nullopt,
            std::nullopt
    );
}

template<typename OutputIterator>
void ARM_JIT_Compiler::print_assembly(OutputIterator& output) {
    std::for_each(instructions_.begin(),
                  instructions_.end(),
                  [this, &output](const instruction_t& instruction)
    {
        switch (std::get<0>(instruction)) {
            case ARM_I::ADD:
                *output = std::string("add\t") +
                          "r" + std::to_string(static_cast<int>(*std::get<1>(instruction))) + ",\t" +
                          "r" + std::to_string(static_cast<int>(*std::get<2>(instruction))) + ",\t" +
                          "r" + std::to_string(static_cast<int>(*std::get<1>(instruction))) + "\n";
                break;

            case ARM_I::SUB:
                *output = std::string("sub\t") +
                          "r" + std::to_string(static_cast<int>(*std::get<1>(instruction))) + ",\t" +
                          "r" + std::to_string(static_cast<int>(*std::get<2>(instruction))) + ",\t" +
                          "r" + std::to_string(static_cast<int>(*std::get<1>(instruction))) + "\n";
                break;

            case ARM_I::MUL:
                *output = std::string("mul\t") +
                          "r" + std::to_string(static_cast<int>(*std::get<1>(instruction))) + ",\t" +
                          "r" + std::to_string(static_cast<int>(*std::get<2>(instruction))) + ",\t" +
                          "r" + std::to_string(static_cast<int>(*std::get<1>(instruction))) + "\n";
                break;

            case ARM_I::BLX:
                *output = std::string("blx\t") +
                          "r" + std::to_string(static_cast<int>(*std::get<1>(instruction))) + "\n";
                break;

            case ARM_I::LDR_FROM_NEXT:
                *output = std::string("ldr\t") +
                          "r" + std::to_string(static_cast<int>(*std::get<1>(instruction))) + ",\t" +
                          "[pc, #4]" + "\n";
                break;

            case ARM_I::LDR_REG:
                *output = std::string("ldr\t") +
                          "r" + std::to_string(static_cast<int>(*std::get<1>(instruction))) + ",\t" +
                          "[" + "r" + std::to_string(static_cast<int>(*std::get<2>(instruction))) + "]" + "\n";
                break;

            case ARM_I::PUSH_REG:
                *output = std::string("push\t{r") +
                          std::to_string(static_cast<int>(*std::get<1>(instruction))) + "}\n";
                break;

            case ARM_I::PUSH_MULT_REG:
                *output = std::string("push\t{r") +
                          std::to_string(static_cast<int>(*std::get<1>(instruction))) + "-r" +
                          std::to_string(static_cast<int>(*std::get<2>(instruction))) +
                          + "}\n";
                break;

            case ARM_I::WORD_DECL:
                *output = std::string(".word\t") +
                          std::string(*std::get<3>(instruction)) + "\n";
                break;

            case ARM_I::POP_MULT_REG:
                *output = std::string("pop\t{r") +
                          std::to_string(static_cast<int>(*std::get<1>(instruction))) + "-r" +
                          std::to_string(static_cast<int>(*std::get<2>(instruction))) +
                          + "}\n";
                break;

            default:
                *output = std::string("UNKNOWN_INSTRUCTION\n");
                break;
        }
        ++output;
    });
}


int main() {
    ExpressionParser parser("(1+a)*c + div(2+4,2)");
    ARM_JIT_Compiler compiler;
    TransferParsingTree(parser, compiler);
    compiler.compile();

    auto out_iter = std::ostream_iterator<std::string>(std::cout, "");
    compiler.print_assembly(out_iter);
    return 0;
}