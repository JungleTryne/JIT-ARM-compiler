/*
 * Danila Mishin
 * ARM Just-In-Time Compiler
 */

#include "../include/JIT_compiler.hpp"

/* Class constructor */
ExpressionParser::ExpressionParser(std::string expression) : expression_(std::move(expression)) {
    GetRidOfSpaces();
    root_ = std::make_unique<Node>();
    this->ParseExpression(root_.get(), expression_.begin(), expression_.end());
}

/* This function helps to get rid of unnecessary spaces in the expression */
void ExpressionParser::GetRidOfSpaces() {
    std::string expression_without_spaces;

    std::for_each(expression_.begin(),
                  expression_.end(),
                  [&expression_without_spaces](char current) {
        if (current != ' ') expression_without_spaces.push_back(current);
    });

    expression_ = expression_without_spaces;
}

/* Finds out if Parenthesis balance is correct for give subexpression */
bool ExpressionParser::IsParBalance(str_iter left, str_iter right) {
    int64_t balance = 0;
    bool correct = true;

    std::for_each(left, right, [&balance, &correct](char current_char) {
        if (balance < 0) correct = false;
        if (current_char == '(') {
            ++balance;
        } else if (current_char == ')') {
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

    for (auto current_iter = left; current_iter != right; ++current_iter) {
        char current_char = *current_iter;

        if (current_char == '(') { ++par_balance; }
        if (current_char == ')') { --par_balance; }

        if (par_balance != 0) {
            continue;
        }

        ExpressionType current_type = GetTypeFromChar(current_char);
        if (current_type == ExpressionType::Default) {
            continue;
        }

        if (!type) {
            type = current_type;
            pos = current_iter;
        } else {
            size_t current_priority = GetPriority(current_type);
            size_t min_priority = GetPriority(*type);
            if (current_priority <= min_priority && *(current_iter-1) != '*') {
                type = current_type;
                pos = current_iter;
            }
            ++current_iter;

            while (*current_iter == '*' || *current_iter == '-' || *current_iter == '+') ++current_iter;
        }
    }
    return std::make_pair(type, pos);
}

/* Main function for Expression parsing process
 * Recursively called. Builds the tree
 */
void ExpressionParser::ParseExpression(Node* current_node, str_iter left, str_iter right) {
    int64_t surplus_pars = 0;

    while (*(left + surplus_pars) == '(' &&
          *(right - 1 - surplus_pars) == ')' &&
          IsParBalance(left + surplus_pars + 1, right - surplus_pars - 1))
    {
        ++surplus_pars;
    }

    std::advance(left, surplus_pars);
    std::advance(right, -surplus_pars);

    auto [type, pos] = FindArithmeticOperation(left, right);
    if (type) {
        ParseArithmetic(current_node, left, right, *type, pos);
    } else {
        if (IsConstant(left)) {
            ParseConstant(current_node, left, right);
        } else if (IsFunction(left, right)) {
            ParseFunction(current_node, left, right);
        } else {
            ParseVariable(current_node, left, right);
        }
    }
}

/* This functions can be called only if expression cannot be divided with arithmetic operation*/
bool ExpressionParser::IsConstant(str_iter left) const {
    return ('0' <= *left && *left <= '9');
}

/* This functions can be called only if expression cannot be divided with arithmetic operation*/
bool ExpressionParser::IsFunction(str_iter left, str_iter right) const {
    for (auto current_iter = left; current_iter != right; ++current_iter) {
        if (*current_iter == '(') {
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

    for (; *current_left != '('; ++current_left);
    ++current_left;

    auto current_right = current_left;

    while (current_right != right) {
        int64_t pron_balance = 0;

        for (; *current_right != ',' || pron_balance > 0; ++current_right) {
            if (*current_right == '(') ++pron_balance;
            if (*current_right == ')') --pron_balance;

            if (pron_balance == -1 && *current_right == ')') {
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

//--------------------------------------------------------------------------------------
//COMPILER
//--------------------------------------------------------------------------------------

void TransferParsingTree(ExpressionParser& parser, ARM_JIT_Compiler& compiler) {
    compiler.parse_tree_ = std::move(parser.root_);
}

void ExpressionParser::ParseConstant(Node* current_node, str_iter left, str_iter right) {
    current_node->type = ExpressionType::Constant;
    std::string dec_view = expression_.substr(std::distance(expression_.begin(), left),
                                              std::distance(left, right));
    std::stringstream hex_stream;
    hex_stream << std::hex << std::stoi(dec_view);
    std::string hex_view("0x" + hex_stream.str());
    current_node->content = hex_view;
}

void ExpressionParser::ParseArithmetic(Node *current_node, str_iter left, str_iter right, ExpressionType type,
                                       str_iter pos) {
    current_node->type = type;
    current_node->content = std::nullopt;
    for (size_t i = 0; i < 2; ++i) {
        current_node->sub_expressions.push_back(std::make_unique<Node>());
    }
    ParseExpression(current_node->sub_expressions[0].get(), left, pos);
    ParseExpression(current_node->sub_expressions[1].get(), pos + 1, right);
}

void ExpressionParser::ParseFunction(Node *current_node, str_iter left, str_iter right) {
    current_node->type = ExpressionType::Function;
    current_node->content = GetFunctionName(left, right);

    size_t counter = 0;
    auto params = GetFunctionParameters(left, right);
    for (auto [pair_left, pair_right] : params) {
        current_node->sub_expressions.push_back(std::make_unique<Node>());
        ParseExpression(current_node->sub_expressions[counter].get(), pair_left, pair_right);
        ++counter;
    }
}

void ExpressionParser::ParseVariable(Node *current_node, str_iter left, str_iter right) {
    if (std::distance(left, right) < 1) {
        current_node->type = ExpressionType::Constant;
        current_node->content = "0x0"; //In case we get -10, left constant will be void, so we make it 0-10
    } else {
        current_node->type = ExpressionType::Variable;
        current_node->content = expression_.substr(std::distance(expression_.begin(), left),
                                                   std::distance(left, right));
    }
}

ExpressionType ExpressionParser::GetTypeFromChar(char current_char) {
    switch (current_char) {
        case '+':
            return ExpressionType::Plus;
        case '-':
            return ExpressionType::Minus;
        case '*':
            return ExpressionType::Product;
        default:
            return ExpressionType::Default;
    }
}

void ARM_JIT_Compiler::compile() {
    add_header();
    compile_(parse_tree_.get());
    add_footer();
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
     * ldr r0, [pc]
     * b skip
     * .word 0x05
     * skip:
     * push {r0}
     *
     * P.S. 0x05 is given for the example
     */

    instructions_.emplace_back ( //ldr r0, [pc]
            ARM_I::LDR_FROM_NEXT,
            ARM_R::R0,
            std::nullopt,
            current->content
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
     * ldr r0, [pc]
     * b skip
     * .word 0xfb1cfcd0
     * skip:
     * ldr r0, [r0]
     * push {r0}
     *
     * P.S. 0xfb1cfcd0 is given for the example
     */

#ifndef DEBUG
    std::stringstream address;
    address << address_map_.at(*current->content);
    std::string address_str = address.str();
#endif

#ifdef DEBUG
    std::string address_str = "0x11111111";
#endif

    instructions_.emplace_back ( //ldr r0, [pc]
            ARM_I::LDR_FROM_NEXT,
            ARM_R::R0,
            std::nullopt,
            address_str
    );

    instructions_.emplace_back ( //.word *constant*
            ARM_I::WORD_DECL,
            std::nullopt,
            std::nullopt,
            address_str
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

    instructions_.emplace_back( //pop {r0-r1}
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
     * ldr r4, [pc]
     * b skip
     * .word 0xfb1cfcd0
     * skip:
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

    #ifndef DEBUG

    std::stringstream address;
    address << address_map_.at(*current->content);
    std::string content = address.str();

    #endif

    #ifdef DEBUG

    std::string content = current->content;

    #endif

    for (int32_t i = arguments_number; i > 0; --i) {
        instructions_.emplace_back ( //pop {r0-ri}
                ARM_I::POP_REG,
                static_cast<ARM_R>(static_cast<size_t>(ARM_R::R0) + i - 1),
                std::nullopt,
                std::nullopt
        );
    }

    instructions_.emplace_back ( //ldr r0, [pc]
            ARM_I::LDR_FROM_NEXT,
            ARM_R::R4,
            std::nullopt,
            content
    );

    instructions_.emplace_back ( //.word *constant*
            ARM_I::WORD_DECL,
            std::nullopt,
            std::nullopt,
            content
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

ARM_JIT_Compiler::ARM_JIT_Compiler(std::map<std::string, void*> address_map) : address_map_(std::move(address_map)) {}

std::vector<uint32_t> ARM_JIT_Compiler::GetCompiledBinary() {
    std::vector<uint32_t> binary = {};
    size_t counter = 0;

    //binary.push_back(0xe52de004); //push {lr}
    //binary.push_back(0xe52d4004); //push {r4}

    for (auto [type, reg1_o, reg2_o, str] : instructions_) {
        uint32_t reg1 = reg1_o.has_value() ? static_cast<uint8_t>(*reg1_o) : 0;
        uint32_t reg2 = reg2_o.has_value() ? static_cast<uint8_t>(*reg2_o) : 0;
        uint32_t instruction = 0x0;
        ++counter;

        switch (type) {
                case ARM_I::ADD:
                    instruction |= reg1 << 12u; //first register
                    instruction |= reg2 << 16u; //second register
                    instruction |= 0u << 20u;   //S = 0
                    instruction |= 0x4u << 21u; //prefix 0100
                    instruction |= 0xeu << 28u; //condition 1110 -> always run
                    binary.push_back(instruction);
                    break;

                case ARM_I::SUB:
                    instruction |= reg1 << 12u; //first register
                    instruction |= reg2 << 16u; //second register
                    instruction |= 0u << 20u;   //S = 0
                    instruction |= 0x2u << 21u; //prefix 0010
                    instruction |= 0xeu << 28u; //condition 1110 -> always run
                    binary.push_back(instruction);
                    break;

                case ARM_I::MUL:
                    instruction |= reg2;        //Rm
                    instruction |= 0x9u << 4u;  //1001 suffix
                    instruction |= reg1 << 8u;  //Rs
                    instruction |= reg1 << 16u; //Rd
                    instruction |= 0xeu << 28u; //condition 1110 -> always run
                    binary.push_back(instruction);
                    break;

                case ARM_I::BLX:
                    if(reg1 == 4) {
                        binary.push_back(0xe12fff34);   //blx r4
                    } else {
                        assert(false);
                    }
                    break;

                case ARM_I::LDR_FROM_NEXT:
                    //this is multiple instructions case
                    /*  ldr r0, [pc]    -> e59f0000
                     *  b skip          -> ea000000
                     *  .word 0x004932  -> [word]
                     *  skip: ...
                     */

                    if (reg1 == 0) {
                        binary.push_back(0xe59f0000);
                    } else {
                        binary.push_back(0xe59f4000);
                    }

                    binary.push_back(0xea000000);

                    #ifdef DEBUG
                    binary.push_back(0x11111111);
                    #endif

                    #ifndef DEBUG
                    binary.push_back(std::stoul(*str, nullptr, 0));
                    #endif

                    break;

                case ARM_I::LDR_REG:
                    if (reg1 == 0) {
                        binary.push_back(0xe5900000);   //ldr r0, [r0]
                    } else if(reg1 == 4) {
                        binary.push_back(0xe5944000);   //ldr r4, [r4]
                    } else {
                        assert(false);
                    }

                    break;

                case ARM_I::PUSH_REG:
                    switch (reg1) {
                        case 0:
                            binary.push_back(0xe52d0004);   //push {r0}
                            break;
                        case 1:
                            binary.push_back(0xe52d1004);   //push {r1}
                            break;
                        case 2:
                            binary.push_back(0xe52d2004);   //push {r2}
                            break;
                        case 3:
                            binary.push_back(0xe52d3004);   //push {r3}
                            break;
                        case 4:
                            binary.push_back(0xe52d4004);   //push {r4}
                            break;
                        case 5:
                            binary.push_back(0xe52de004);   //push {lr}
                            break;
                        default:
                            assert(false);
                    }
                    break;

                case ARM_I::PUSH_MULT_REG:
                    switch (reg2) {
                        case 0:
                            assert(false);
                        case 1:
                            binary.push_back(0xe92d0003);   //push {r0-r1}
                            break;
                        case 2:
                            binary.push_back(0xe92d0007);   //push {r0-r2}
                            break;
                        case 3:
                            binary.push_back(0xe92d000f);   //push {r0-r3}
                            break;
                        default:
                            assert(false);
                    }
                    break;

                case ARM_I::WORD_DECL:
                    //not used - compiled in LDR_NEXT
                    break;

                case ARM_I::POP_REG:
                    switch (reg1) {
                        case 0:
                            binary.push_back(0xe49d0004);   //pop {r0}
                            break;
                        case 1:
                            binary.push_back(0xe49d1004);   //pop {r1}
                            break;
                        case 2:
                            binary.push_back(0xe49d2004);   //pop {r2}
                            break;
                        case 3:
                            binary.push_back(0xe49d3004);   //pop {r3}
                            break;
                        case 4:
                            binary.push_back(0xe49d4004);   //pop {r4}
                            break;
                        default:
                            assert(false);
                    }
                    break;

                case ARM_I::POP_MULT_REG:
                    switch (reg2) {
                        case 0:
                            assert(false);
                        case 1:
                            binary.push_back(0xe8bd0003); //pop {r0-r1}
                            break;
                        case 2:
                            binary.push_back(0xe8bd0007); //pop {r0-r2}
                            break;
                        case 3:
                            binary.push_back(0xe8bd000f); //pop {r0-r3}
                            break;
                        case 6:
                            if(reg1 != 4) assert(false);
                            binary.push_back(0xe8bd8010); //pop {r4-pc}
                            break;
                        default:
                            assert(false);
                    }
                    break;

                default:
                    assert(false);
        }
    }

    return binary;
}

void ARM_JIT_Compiler::add_header() {
    /* Adding
     * push {lr}
     * push {r4}
     * to the beginning of the code
     */

    instructions_.emplace_back(
        ARM_I::PUSH_REG,
        ARM_R::LR,
        std::nullopt,
        std::nullopt
    );

    instructions_.emplace_back(
        ARM_I::PUSH_REG,
        ARM_R::R4,
        std::nullopt,
        std::nullopt
    );
}

void ARM_JIT_Compiler::add_footer() {
    /* Adding
     * pop  {r0}
     * pop  {r4-pc}
     * to the end of the code
     */

    instructions_.emplace_back(
            ARM_I::POP_REG,
            ARM_R::R0,
            std::nullopt,
            std::nullopt
    );

    instructions_.emplace_back(
            ARM_I::POP_MULT_REG,
            ARM_R::R4,
            ARM_R::PC,
            std::nullopt
    );
}


extern void
jit_compile_expression_to_arm(const char * expression,
                              const symbol_t * externs,
                              void * out_buffer) {
    std::string expression_cpp{expression};
    std::map<std::string, void*> address_map = {};

    symbol_t* current = const_cast<symbol_t*>(externs);
    for (;current->pointer && current->name; ++current) {
        address_map[current->name] = current->pointer;
    }

    ExpressionParser parser(expression_cpp);
    ARM_JIT_Compiler compiler(address_map);
    TransferParsingTree(parser, compiler);
    compiler.compile();

    auto bin = compiler.GetCompiledBinary();

    uint32_t* u32_buffer = static_cast<uint32_t*>(out_buffer);
    for (size_t i = 0; i < bin.size(); ++i) {
        *u32_buffer = bin[i];
        ++u32_buffer;
    }
}