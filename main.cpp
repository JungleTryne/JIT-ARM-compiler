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

    [[nodiscard]] auto FindArithmeticOperation(size_t left, size_t right) const
        -> std::pair<std::optional<ExpressionType>, size_t>;

    void ParseExpression(Node* current_node, size_t left, size_t right);
    bool IsParBalance(size_t left, size_t right);

    [[nodiscard]] inline bool IsConstant(size_t left) const;
    [[nodiscard]] inline bool IsFunction(size_t left, size_t right) const;
    [[nodiscard]] std::string GetFunctionName(size_t left, size_t right) const;
    [[nodiscard]] std::vector<std::pair<size_t, size_t>> GetFunctionParameters(size_t left, size_t right) const;
};

/* Class constructor */
ExpressionParser::ExpressionParser(std::string expression) : expression_(std::move(expression)) {
    GetRidOfSpaces();
    root_ = std::make_unique<Node>();
    this->ParseExpression(root_.get(), 0, expression_.size());
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
bool ExpressionParser::IsParBalance(size_t left, size_t right) {
    int64_t balance = 0;
    for(size_t i = left; i < right; ++i) {
        if(balance < 0) return false;
        if(expression_[i] == '(') {
            ++balance;
        } else if(expression_[i] == ')') {
            --balance;
        }
    }
    return true;
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
auto ExpressionParser::FindArithmeticOperation(size_t left, size_t right) const
-> std::pair<std::optional<ExpressionType>, size_t> {

    std::optional<ExpressionType> type = std::nullopt;
    size_t pos = 0;
    int64_t par_balance = 0;

    for(size_t i = left; i < right; ++i) {
        char current_char = expression_[i];
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
            pos = i;
        } else {
            size_t current_priority = GetPriority(current_type);
            size_t min_priority = GetPriority(*type);
            if(current_priority < min_priority) {
                type = current_type;
                pos = i;
            }
            ++i;
            //TODO: how to deal with 5*-+-+-+5?
            while(expression_[i] == '*' || expression_[i] == '-' || expression_[i] == '+') ++i;
        }
    }
    return std::make_pair(type, pos);
}

/* Main function for Expression parsing process
 * Recursively called. Builds the tree
 */
void ExpressionParser::ParseExpression(Node* current_node, size_t left, size_t right) {
    size_t surplus_pars = 0;

    while(expression_[left + surplus_pars] == '(' &&
        expression_[right - 1 - surplus_pars] == ')' &&
        IsParBalance(left + surplus_pars + 1, right - surplus_pars - 1))
    {
        ++surplus_pars;
    }

    left += surplus_pars;
    right -= surplus_pars;

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
            current_node->content = expression_.substr(left, right - left);
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
                current_node->content = expression_.substr(left, right - left);
            }
        }
    }
}

/* This functions can be called only if expression cannot be divided with arithmetic operation*/
bool ExpressionParser::IsConstant(size_t left) const {
    return ('0' <= expression_[left] && expression_[left] <= '9');
}

/* This functions can be called only if expression cannot be divided with arithmetic operation*/
bool ExpressionParser::IsFunction(size_t left, size_t right) const {
    for(size_t i = left; i < right; ++i) {
        if(expression_[i] == '(') {
            return true;
        }
    }
    return false;
}

/* This functions can be called only if expression[left:right] is actually a function*/
std::string ExpressionParser::GetFunctionName(size_t left, size_t right) const {
    for(size_t i = left; i < right; ++i) {
        if(expression_[i] == '(') {
            return expression_.substr(left, i - left);
        }
    }
    assert(false);
}

/* This functions can be called only if expression[left:right] is actually a function*/
std::vector<std::pair<size_t, size_t>> ExpressionParser::GetFunctionParameters(size_t left, size_t right) const {
    std::vector<std::pair<size_t, size_t>> answer = {};
    size_t current_left = left;
    for(; expression_[current_left] != '('; ++current_left);
    ++current_left;
    size_t current_right = current_left;

    while(current_right < right) {
        int64_t pron_balance = 0;

        for(; expression_[current_right] != ',' || pron_balance > 0; ++current_right) {
            if(expression_[current_right] == '(') ++pron_balance;
            if(expression_[current_right] == ')') --pron_balance;

            if(pron_balance == -1 && expression_[current_right] == ')') {
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
    ExpressionParser parser("-+-+-+5");
    return 0;
}