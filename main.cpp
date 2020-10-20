/*
 * Danila Mishin
 * ARM Just-In-Time Compiler
 */

#include <algorithm>
#include <cassert>
#include <memory>
#include <optional>
#include <tuple>
#include <string>
#include <vector>
#include <utility>

using str_iter = std::string::iterator;
using str_iter_const = std::string::const_iterator;

/* ExpressionParser class
 * This class converts the given expression into tree
 * which can be used for ARM code generation
 */

class ExpressionParser {
public:
    explicit ExpressionParser(std::string expression);
private:
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

    std::string expression_;
    std::unique_ptr<Node> root_;

    static size_t GetPriority(ExpressionType operation);
    void GetRidOfSpaces();

    [[nodiscard]] static auto FindArithmeticOperation(str_iter left, str_iter right)
        -> std::pair<std::optional<ExpressionType>, str_iter>;

    void ParseExpression(Node* current_node, str_iter left, str_iter right);
    bool IsParBalance(str_iter left, str_iter right);

    [[nodiscard]] inline bool IsConstant(str_iter left) const;
    [[nodiscard]] inline bool IsFunction(str_iter left, str_iter right) const;
    [[nodiscard]] std::string GetFunctionName(str_iter_const left, str_iter_const right) const;
    [[nodiscard]] std::vector<std::pair<str_iter, str_iter>> GetFunctionParameters(str_iter left, str_iter right) const;
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
    for(auto c : expression_) {
        if(c != ' ') {
            expression_without_spaces.push_back(c);
        }
    }
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
size_t ExpressionParser::GetPriority(ExpressionParser::ExpressionType operation) {
    switch (operation) {
        case ExpressionParser::ExpressionType::Plus:
        case ExpressionParser::ExpressionType::Minus:
            return 0;
        case ExpressionParser::ExpressionType::Product:
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
        if(current_type == ExpressionType::Default) continue;
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
            if(right - left < 1) {
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
    for(auto current_iter = left; current_iter != right; ++current_iter) {
        if(*current_iter == '(') {
            size_t pos = std::distance(expression_.begin(), left);
            return expression_.substr(pos, std::distance(left, current_iter));
        }
    }
    assert(false);
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

int main() {
    ExpressionParser parser("(1+a)*c + div(2+4,2)");
    return 0;
}