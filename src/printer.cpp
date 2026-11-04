#include "printer.hpp"
#include <iomanip>
#include <sstream>

namespace sk {

namespace {

template <class... Ts> struct Overloaded : Ts... { using Ts::operator()...; };
template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

std::string opSymbol(TokenType t) {
    switch (t) {
        case TokenType::Plus:      return "+";
        case TokenType::Minus:     return "-";
        case TokenType::Star:      return "*";
        case TokenType::Slash:     return "/";
        case TokenType::Percent:   return "%";
        case TokenType::Bang:      return "!";
        case TokenType::Eq:        return "==";
        case TokenType::NotEq:     return "!=";
        case TokenType::Less:      return "<";
        case TokenType::LessEq:    return "<=";
        case TokenType::Greater:   return ">";
        case TokenType::GreaterEq: return ">=";
        default:                   return "?";
    }
}

std::string blockToSexpr(const Block& b) {
    std::string out;
    for (const auto& s : b) out += " " + toSexpr(*s);
    return out;
}

} // namespace

std::string toSexpr(const Expr& e) {
    return std::visit(Overloaded{
        [](const NumberLit& n) {
            std::ostringstream os;
            os << std::setprecision(15) << n.value;
            return os.str();
        },
        [](const StringLit& s) { return "\"" + s.value + "\""; },
        [](const BoolLit& b) { return std::string(b.value ? "true" : "false"); },
        [](const Variable& v) { return v.name; },
        [](const Unary& u) { return "(" + opSymbol(u.op) + " " + toSexpr(*u.operand) + ")"; },
        [](const Binary& b) {
            return "(" + opSymbol(b.op) + " " + toSexpr(*b.left) + " " + toSexpr(*b.right) + ")";
        },
        [](const Call& c) {
            std::string out = "(call " + c.callee;
            for (const auto& a : c.args) out += " " + toSexpr(*a);
            return out + ")";
        },
    }, e.node);
}

std::string toSexpr(const Stmt& s) {
    return std::visit(Overloaded{
        [](const LetStmt& l) { return "(let " + l.name + " " + toSexpr(*l.init) + ")"; },
        [](const AssignStmt& a) { return "(assign " + a.name + " " + toSexpr(*a.value) + ")"; },
        [](const ExprStmt& e) { return "(expr " + toSexpr(*e.expr) + ")"; },
        [](const RepeatStmt& r) { return "(repeat " + toSexpr(*r.count) + blockToSexpr(r.body) + ")"; },
        [](const WhileStmt& w) { return "(while " + toSexpr(*w.cond) + blockToSexpr(w.body) + ")"; },
        [](const IfStmt& i) {
            std::string out = "(if " + toSexpr(*i.cond) + " (then" + blockToSexpr(i.thenBlock) + ")";
            if (i.hasElse) out += " (else" + blockToSexpr(i.elseBlock) + ")";
            return out + ")";
        },
        [](const FnDecl& f) {
            std::string params;
            for (size_t k = 0; k < f.params.size(); k++) params += (k ? " " : "") + f.params[k];
            return "(fn " + f.name + " (" + params + ")" + blockToSexpr(f.body) + ")";
        },
        [](const ReturnStmt& r) {
            return r.value ? "(return " + toSexpr(*r.value) + ")" : std::string("(return)");
        },
    }, s.node);
}

std::string toSexpr(const Program& p) {
    std::string out;
    for (size_t k = 0; k < p.size(); k++) {
        if (k) out += "\n";
        out += toSexpr(*p[k]);
    }
    return out;
}

} // namespace sk
