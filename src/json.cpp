#include "json.hpp"
#include "printer.hpp"
#include <cmath>
#include <cstdio>
#include <sstream>

namespace sk {

// ------------------------------------------------------------ JsonWriter

void JsonWriter::sep() {
    if (afterKey_) {
        afterKey_ = false;
        return;
    }
    if (!first_.empty()) {
        if (!first_.back()) out_ += ',';
        first_.back() = false;
    }
}

void JsonWriter::value(double d) {
    sep();
    if (!std::isfinite(d)) {
        out_ += "null";
        return;
    }
    std::ostringstream os;
    os.precision(15);
    os << d;
    out_ += os.str();
}

void JsonWriter::writeString(const std::string& s) {
    out_ += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out_ += "\\\""; break;
            case '\\': out_ += "\\\\"; break;
            case '\n': out_ += "\\n"; break;
            case '\r': out_ += "\\r"; break;
            case '\t': out_ += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out_ += buf;
                } else {
                    out_ += static_cast<char>(c);
                }
        }
    }
    out_ += '"';
}

// ---------------------------------------------------------------- tokens

void writeTokens(JsonWriter& w, const std::vector<Token>& tokens) {
    w.beginArray();
    for (const Token& t : tokens) {
        if (t.type == TokenType::Eof) continue;
        w.beginObject();
        w.key("type");   w.value(tokenTypeName(t.type));
        w.key("lexeme"); w.value(t.lexeme);
        w.key("line");   w.value(t.line);
        w.key("col");    w.value(t.col);
        w.endObject();
    }
    w.endArray();
}

// ------------------------------------------------------------------- AST
// Every node is { type, label, line, children[] } so the UI can draw any
// node with one generic renderer.

namespace {

void node(JsonWriter& w, const char* type, const std::string& label, int line) {
    w.beginObject();
    w.key("type");  w.value(type);
    w.key("label"); w.value(label);
    w.key("line");  w.value(line);
}

const char* opText(TokenType t) {
    switch (t) {
        case TokenType::Plus: return "+";   case TokenType::Minus: return "-";
        case TokenType::Star: return "*";   case TokenType::Slash: return "/";
        case TokenType::Percent: return "%"; case TokenType::Bang: return "!";
        case TokenType::Eq: return "==";    case TokenType::NotEq: return "!=";
        case TokenType::Less: return "<";   case TokenType::LessEq: return "<=";
        case TokenType::Greater: return ">"; case TokenType::GreaterEq: return ">=";
        default: return "?";
    }
}

void writeExpr(JsonWriter& w, const Expr& e);
void writeStmt(JsonWriter& w, const Stmt& s);

void writeBlock(JsonWriter& w, const char* name, const Block& b, int line) {
    node(w, "Block", name, line);
    w.key("children");
    w.beginArray();
    for (const auto& s : b) writeStmt(w, *s);
    w.endArray();
    w.endObject();
}

void writeExpr(JsonWriter& w, const Expr& e) {
    std::visit([&](const auto& n) {
        using T = std::decay_t<decltype(n)>;
        if constexpr (std::is_same_v<T, NumberLit>) {
            node(w, "Number", toSexpr(e), e.line);
            w.key("children"); w.beginArray(); w.endArray();
        } else if constexpr (std::is_same_v<T, StringLit>) {
            node(w, "String", "\"" + n.value + "\"", e.line);
            w.key("children"); w.beginArray(); w.endArray();
        } else if constexpr (std::is_same_v<T, BoolLit>) {
            node(w, "Bool", n.value ? "true" : "false", e.line);
            w.key("children"); w.beginArray(); w.endArray();
        } else if constexpr (std::is_same_v<T, Variable>) {
            node(w, "Variable", n.name, e.line);
            w.key("children"); w.beginArray(); w.endArray();
        } else if constexpr (std::is_same_v<T, Unary>) {
            node(w, "Unary", opText(n.op), e.line);
            w.key("children"); w.beginArray(); writeExpr(w, *n.operand); w.endArray();
        } else if constexpr (std::is_same_v<T, Binary>) {
            node(w, "Binary", opText(n.op), e.line);
            w.key("children"); w.beginArray(); writeExpr(w, *n.left); writeExpr(w, *n.right); w.endArray();
        } else if constexpr (std::is_same_v<T, Call>) {
            node(w, "Call", n.callee, e.line);
            w.key("children"); w.beginArray();
            for (const auto& a : n.args) writeExpr(w, *a);
            w.endArray();
        }
        w.endObject();
    }, e.node);
}

void writeStmt(JsonWriter& w, const Stmt& s) {
    std::visit([&](const auto& n) {
        using T = std::decay_t<decltype(n)>;
        if constexpr (std::is_same_v<T, LetStmt>) {
            node(w, "Let", n.name, s.line);
            w.key("children"); w.beginArray(); writeExpr(w, *n.init); w.endArray();
        } else if constexpr (std::is_same_v<T, AssignStmt>) {
            node(w, "Assign", n.name, s.line);
            w.key("children"); w.beginArray(); writeExpr(w, *n.value); w.endArray();
        } else if constexpr (std::is_same_v<T, ExprStmt>) {
            node(w, "ExprStmt", "", s.line);
            w.key("children"); w.beginArray(); writeExpr(w, *n.expr); w.endArray();
        } else if constexpr (std::is_same_v<T, RepeatStmt>) {
            node(w, "Repeat", "", s.line);
            w.key("children"); w.beginArray();
            writeExpr(w, *n.count);
            writeBlock(w, "body", n.body, s.line);
            w.endArray();
        } else if constexpr (std::is_same_v<T, WhileStmt>) {
            node(w, "While", "", s.line);
            w.key("children"); w.beginArray();
            writeExpr(w, *n.cond);
            writeBlock(w, "body", n.body, s.line);
            w.endArray();
        } else if constexpr (std::is_same_v<T, IfStmt>) {
            node(w, "If", "", s.line);
            w.key("children"); w.beginArray();
            writeExpr(w, *n.cond);
            writeBlock(w, "then", n.thenBlock, s.line);
            if (n.hasElse) writeBlock(w, "else", n.elseBlock, s.line);
            w.endArray();
        } else if constexpr (std::is_same_v<T, FnDecl>) {
            std::string label = n.name + "(";
            for (size_t i = 0; i < n.params.size(); i++) label += (i ? ", " : "") + n.params[i];
            label += ")";
            node(w, "Fn", label, s.line);
            w.key("children"); w.beginArray();
            writeBlock(w, "body", n.body, s.line);
            w.endArray();
        } else if constexpr (std::is_same_v<T, ReturnStmt>) {
            node(w, "Return", "", s.line);
            w.key("children"); w.beginArray();
            if (n.value) writeExpr(w, *n.value);
            w.endArray();
        }
        w.endObject();
    }, s.node);
}

} // namespace

void writeAst(JsonWriter& w, const Program& program) {
    node(w, "Program", "", 1);
    w.key("children");
    w.beginArray();
    for (const auto& s : program) writeStmt(w, *s);
    w.endArray();
    w.endObject();
}

// -------------------------------------------------------------- bytecode

void writeBytecode(JsonWriter& w, const Module& m) {
    w.beginArray();
    for (const Function& fn : m.functions) {
        w.beginObject();
        w.key("name");      w.value(fn.name);
        w.key("arity");     w.value(fn.arity);
        w.key("maxLocals"); w.value(fn.maxLocals);
        w.key("code");
        w.beginArray();
        for (size_t ip = 0; ip < fn.code.size(); ip++) {
            InstrView v = describeInstr(m, fn, ip);
            w.beginObject();
            w.key("op");      w.value(opName(fn.code[ip].op));
            w.key("operand"); w.value(v.operand);
            w.key("note");    w.value(v.note);
            w.key("line");    w.value(fn.lines[ip]);
            w.endObject();
        }
        w.endArray();
        w.endObject();
    }
    w.endArray();
}

// -------------------------------------------------------------- VM state

void writeVmState(JsonWriter& w, const VM& vm) {
    const Module& m = vm.module();
    const char* status = vm.status() == Status::Running ? "running"
                       : vm.status() == Status::Finished ? "finished" : "error";
    w.beginObject();
    w.key("status");     w.value(status);
    w.key("error");      w.value(vm.error());
    w.key("errorLine");  w.value(vm.errorLine());
    w.key("steps");      w.value(vm.steps());

    // Where execution is right now: the NEXT instruction to run.
    const Frame& top = vm.frames().back();
    bool running = vm.status() == Status::Running;
    int fnIndex = static_cast<int>(top.fn - m.functions.data());
    w.key("fn");   w.value(running ? fnIndex : -1);
    w.key("ip");   w.value(running ? static_cast<long long>(top.ip) : -1LL);
    int line = 0;
    if (running && top.ip < top.fn->lines.size()) line = top.fn->lines[top.ip];
    else if (vm.status() == Status::Error) line = vm.errorLine();
    w.key("line"); w.value(line);

    w.key("stack");
    w.beginArray();
    for (const Value& v : vm.stack()) {
        w.beginObject();
        w.key("t"); w.value(typeName(v));
        w.key("v"); w.value(vm.format(v));
        w.endObject();
    }
    w.endArray();

    w.key("frames");
    w.beginArray();
    for (const Frame& f : vm.frames()) {
        w.beginObject();
        w.key("name"); w.value(f.fn->name);
        w.key("base"); w.value(f.base);
        w.endObject();
    }
    w.endArray();

    w.key("globals");
    w.beginArray();
    for (size_t i = 0; i < m.globalNames.size(); i++) {
        w.beginObject();
        w.key("name");    w.value(m.globalNames[i]);
        w.key("defined"); w.value(vm.globalDefined(i));
        w.key("t");       w.value(vm.globalDefined(i) ? typeName(vm.globals()[i]) : "undefined");
        w.key("v");       w.value(vm.globalDefined(i) ? vm.format(vm.globals()[i]) : std::string("-"));
        w.endObject();
    }
    w.endArray();

    w.key("output");
    w.beginArray();
    for (const std::string& line2 : vm.output()) w.value(line2);
    w.endArray();

    w.key("drawCount"); w.value(vm.draws().size());
    w.endObject();
}

// Each shape is [kind, a, b, c, d, r, g, b]  (kind: 0 circle, 1 rect, 2 line)
void writeDraws(JsonWriter& w, const std::vector<DrawCmd>& draws) {
    w.beginArray();
    for (const DrawCmd& d : draws) {
        w.beginArray();
        w.value(static_cast<int>(d.kind));
        w.value(d.a); w.value(d.b); w.value(d.c); w.value(d.d);
        w.value(static_cast<int>(d.r)); w.value(static_cast<int>(d.g)); w.value(static_cast<int>(d.bl));
        w.endArray();
    }
    w.endArray();
}

} // namespace sk
