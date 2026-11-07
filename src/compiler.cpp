#include "compiler.hpp"
#include "natives.hpp"
#include <unordered_map>

namespace sk {

namespace {

class Compiler {
public:
    CompileResult run(const Program& program);

private:
    struct Local {
        std::string name;
        int depth;
    };
    struct FuncState {
        int index = 0;
        std::vector<Local> locals;
        int scopeDepth = 0;
        bool isMain = true;
    };

    // ---- diagnostics and emitting
    void error(int line, int col, const std::string& msg) {
        errors_.push_back(std::to_string(line) + ":" + std::to_string(col) + ": " + msg);
    }
    Function& fn() { return module_.functions[static_cast<size_t>(cur_.index)]; }
    int here() { return static_cast<int>(fn().code.size()); }
    int emit(Op op, int a, int b, int line) {
        fn().code.push_back(Instr{op, a, b});
        fn().lines.push_back(line);
        return here() - 1;
    }
    void patch(int at) { fn().code[static_cast<size_t>(at)].a = here(); }

    // ---- constant pools
    int addConstant(Value v) {
        module_.constants.push_back(v);
        return static_cast<int>(module_.constants.size()) - 1;
    }
    int numberConstant(double d) {
        for (size_t i = 0; i < module_.constants.size(); i++) {
            const Value& c = module_.constants[i];
            if (c.type == ValueType::Number && c.n == d) return static_cast<int>(i);
        }
        return addConstant(Value::number(d));
    }
    int stringConstant(const std::string& s) {
        uint32_t idx;
        auto it = stringIndex_.find(s);
        if (it != stringIndex_.end()) {
            idx = it->second;
        } else {
            idx = static_cast<uint32_t>(module_.strings.size());
            module_.strings.push_back(s);
            stringIndex_[s] = idx;
        }
        for (size_t i = 0; i < module_.constants.size(); i++) {
            const Value& c = module_.constants[i];
            if (c.type == ValueType::String && c.s == idx) return static_cast<int>(i);
        }
        return addConstant(Value::string(idx));
    }
    int globalSlot(const std::string& name) {
        auto it = globalIndex_.find(name);
        if (it != globalIndex_.end()) return it->second;
        int idx = static_cast<int>(module_.globalNames.size());
        module_.globalNames.push_back(name);
        globalIndex_[name] = idx;
        return idx;
    }

    // ---- scopes and locals
    void beginScope() { cur_.scopeDepth++; }
    void endScope() {
        while (!cur_.locals.empty() && cur_.locals.back().depth == cur_.scopeDepth) cur_.locals.pop_back();
        cur_.scopeDepth--;
    }
    int addLocal(const std::string& name) {
        int slot = static_cast<int>(cur_.locals.size());
        cur_.locals.push_back(Local{name, cur_.scopeDepth});
        if (slot + 1 > fn().maxLocals) fn().maxLocals = slot + 1;
        return slot;
    }
    int resolveLocal(const std::string& name) const {
        for (int i = static_cast<int>(cur_.locals.size()) - 1; i >= 0; i--) {
            if (cur_.locals[static_cast<size_t>(i)].name == name) return i;
        }
        return -1;
    }
    bool declaredInCurrentScope(const std::string& name) const {
        for (int i = static_cast<int>(cur_.locals.size()) - 1; i >= 0; i--) {
            const Local& l = cur_.locals[static_cast<size_t>(i)];
            if (l.depth != cur_.scopeDepth) break;
            if (l.name == name) return true;
        }
        return false;
    }
    void compileBody(const Block& body) {
        beginScope();
        for (const auto& s : body) stmt(*s);
        endScope();
    }

    // ---- statements
    void stmt(const Stmt& s) {
        std::visit([&](const auto& n) { node(n, s.line, s.col); }, s.node);
    }
    void node(const LetStmt& s, int line, int col);
    void node(const AssignStmt& s, int line, int col);
    void node(const ExprStmt& s, int line, int col);
    void node(const RepeatStmt& s, int line, int col);
    void node(const WhileStmt& s, int line, int col);
    void node(const IfStmt& s, int line, int col);
    void node(const FnDecl& s, int line, int col);
    void node(const ReturnStmt& s, int line, int col);

    // ---- expressions
    void expr(const Expr& e) {
        std::visit([&](const auto& n) { node(n, e.line, e.col); }, e.node);
    }
    void node(const NumberLit& e, int line, int col);
    void node(const StringLit& e, int line, int col);
    void node(const BoolLit& e, int line, int col);
    void node(const Variable& e, int line, int col);
    void node(const Unary& e, int line, int col);
    void node(const Binary& e, int line, int col);
    void node(const Call& e, int line, int col);

    Module module_;
    std::vector<std::string> errors_;
    FuncState cur_;
    std::unordered_map<std::string, int> globalIndex_;
    std::unordered_map<std::string, uint32_t> stringIndex_;
    std::unordered_map<std::string, int> fnIndex_;
    std::unordered_map<const FnDecl*, int> declIndex_;
};

CompileResult Compiler::run(const Program& program) {
    module_.functions.push_back(Function{"<main>", 0, 0, {}, {}});

    // Pre-pass: register every top-level function and global so that code can
    // refer to things declared later in the file.
    for (const auto& sp : program) {
        if (const auto* f = std::get_if<FnDecl>(&sp->node)) {
            if (findNative(f->name) >= 0) {
                error(sp->line, sp->col, "'" + f->name + "' is a built-in function and cannot be redefined");
            } else if (fnIndex_.count(f->name)) {
                error(sp->line, sp->col, "Function '" + f->name + "' is already defined");
            } else {
                int idx = static_cast<int>(module_.functions.size());
                module_.functions.push_back(Function{f->name, static_cast<int>(f->params.size()), 0, {}, {}});
                fnIndex_[f->name] = idx;
                declIndex_[f] = idx;
            }
        } else if (const auto* l = std::get_if<LetStmt>(&sp->node)) {
            globalSlot(l->name);
        }
    }

    cur_ = FuncState{};
    int lastLine = 1;
    for (const auto& s : program) {
        stmt(*s);
        lastLine = s->line;
    }
    emit(Op::Nil, 0, 0, lastLine);
    emit(Op::Return, 0, 0, lastLine);

    return CompileResult{std::move(module_), std::move(errors_)};
}

// ------------------------------------------------------------- statements

void Compiler::node(const LetStmt& s, int line, int) {
    expr(*s.init);
    if (cur_.isMain && cur_.scopeDepth == 0) {
        emit(Op::DefGlobal, globalSlot(s.name), 0, line);
        return;
    }
    if (declaredInCurrentScope(s.name)) {
        error(line, 1, "Variable '" + s.name + "' is already declared in this scope");
        emit(Op::Pop, 0, 0, line);
        return;
    }
    int slot = addLocal(s.name);  // declared after the initializer, so `let x = x` sees the outer x
    emit(Op::SetLocal, slot, 0, line);
}

void Compiler::node(const AssignStmt& s, int line, int col) {
    expr(*s.value);
    int slot = resolveLocal(s.name);
    if (slot >= 0) {
        emit(Op::SetLocal, slot, 0, line);
    } else if (globalIndex_.count(s.name)) {
        emit(Op::SetGlobal, globalIndex_[s.name], 0, line);
    } else {
        error(line, col, "Undefined variable '" + s.name + "'");
        emit(Op::Pop, 0, 0, line);
    }
}

void Compiler::node(const ExprStmt& s, int line, int) {
    expr(*s.expr);
    emit(Op::Pop, 0, 0, line);  // every expression leaves exactly one value; discard it
}

// repeat n { body }  ==>  hidden counter local, counted down to zero
void Compiler::node(const RepeatStmt& s, int line, int) {
    beginScope();
    expr(*s.count);
    int counter = addLocal("$repeat");  // '$' cannot appear in user identifiers
    emit(Op::SetLocal, counter, 0, line);

    int start = here();
    emit(Op::GetLocal, counter, 0, line);
    emit(Op::Const, numberConstant(0), 0, line);
    emit(Op::Gt, 0, 0, line);
    int exitJump = emit(Op::JumpIfFalse, 0, 0, line);

    compileBody(s.body);

    emit(Op::GetLocal, counter, 0, line);
    emit(Op::Const, numberConstant(1), 0, line);
    emit(Op::Sub, 0, 0, line);
    emit(Op::SetLocal, counter, 0, line);
    emit(Op::Jump, start, 0, line);
    patch(exitJump);
    endScope();
}

void Compiler::node(const WhileStmt& s, int line, int) {
    int start = here();
    expr(*s.cond);
    int exitJump = emit(Op::JumpIfFalse, 0, 0, line);
    compileBody(s.body);
    emit(Op::Jump, start, 0, line);
    patch(exitJump);
}

void Compiler::node(const IfStmt& s, int line, int) {
    expr(*s.cond);
    int elseJump = emit(Op::JumpIfFalse, 0, 0, line);
    compileBody(s.thenBlock);
    if (s.hasElse) {
        int endJump = emit(Op::Jump, 0, 0, line);
        patch(elseJump);
        compileBody(s.elseBlock);
        patch(endJump);
    } else {
        patch(elseJump);
    }
}

void Compiler::node(const FnDecl& f, int line, int col) {
    if (!cur_.isMain || cur_.scopeDepth != 0) {
        error(line, col, "Functions can only be declared at the top level");
        return;
    }
    auto it = declIndex_.find(&f);
    if (it == declIndex_.end()) return;  // already reported in the pre-pass

    FuncState saved = std::move(cur_);
    cur_ = FuncState{};
    cur_.index = it->second;
    cur_.isMain = false;

    beginScope();
    for (const std::string& p : f.params) {
        if (declaredInCurrentScope(p)) error(line, col, "Duplicate parameter '" + p + "'");
        addLocal(p);
    }
    for (const auto& s : f.body) stmt(*s);
    emit(Op::Nil, 0, 0, line);
    emit(Op::Return, 0, 0, line);

    cur_ = std::move(saved);
}

void Compiler::node(const ReturnStmt& s, int line, int col) {
    if (cur_.isMain) {
        error(line, col, "Cannot return from top-level code");
        return;
    }
    if (s.value) expr(*s.value);
    else emit(Op::Nil, 0, 0, line);
    emit(Op::Return, 0, 0, line);
}

// ------------------------------------------------------------ expressions

void Compiler::node(const NumberLit& e, int line, int) {
    emit(Op::Const, numberConstant(e.value), 0, line);
}

void Compiler::node(const StringLit& e, int line, int) {
    emit(Op::Const, stringConstant(e.value), 0, line);
}

void Compiler::node(const BoolLit& e, int line, int) {
    emit(e.value ? Op::True : Op::False, 0, 0, line);
}

void Compiler::node(const Variable& e, int line, int col) {
    int slot = resolveLocal(e.name);
    if (slot >= 0) {
        emit(Op::GetLocal, slot, 0, line);
        return;
    }
    auto it = globalIndex_.find(e.name);
    if (it != globalIndex_.end()) {
        emit(Op::GetGlobal, it->second, 0, line);
        return;
    }
    error(line, col, "Undefined variable '" + e.name + "'");
    emit(Op::Nil, 0, 0, line);
}

void Compiler::node(const Unary& e, int line, int) {
    expr(*e.operand);
    emit(e.op == TokenType::Minus ? Op::Neg : Op::Not, 0, 0, line);
}

void Compiler::node(const Binary& e, int line, int) {
    expr(*e.left);
    expr(*e.right);
    Op op = Op::Add;
    switch (e.op) {
        case TokenType::Plus:      op = Op::Add; break;
        case TokenType::Minus:     op = Op::Sub; break;
        case TokenType::Star:      op = Op::Mul; break;
        case TokenType::Slash:     op = Op::Div; break;
        case TokenType::Percent:   op = Op::Mod; break;
        case TokenType::Eq:        op = Op::Eq;  break;
        case TokenType::NotEq:     op = Op::Neq; break;
        case TokenType::Less:      op = Op::Lt;  break;
        case TokenType::LessEq:    op = Op::Le;  break;
        case TokenType::Greater:   op = Op::Gt;  break;
        case TokenType::GreaterEq: op = Op::Ge;  break;
        default: break;
    }
    emit(op, 0, 0, line);
}

void Compiler::node(const Call& e, int line, int col) {
    int argc = static_cast<int>(e.args.size());

    int native = findNative(e.callee);
    if (native >= 0) {
        int arity = nativeTable()[static_cast<size_t>(native)].arity;
        if (argc != arity) {
            error(line, col, "'" + e.callee + "' expects " + std::to_string(arity) +
                                 " argument(s) but got " + std::to_string(argc));
            emit(Op::Nil, 0, 0, line);
            return;
        }
        for (const auto& a : e.args) expr(*a);
        emit(Op::CallNative, native, argc, line);
        return;
    }

    auto it = fnIndex_.find(e.callee);
    if (it == fnIndex_.end()) {
        error(line, col, "Undefined function '" + e.callee + "'");
        emit(Op::Nil, 0, 0, line);
        return;
    }
    int arity = module_.functions[static_cast<size_t>(it->second)].arity;
    if (argc != arity) {
        error(line, col, "'" + e.callee + "' expects " + std::to_string(arity) +
                             " argument(s) but got " + std::to_string(argc));
        emit(Op::Nil, 0, 0, line);
        return;
    }
    for (const auto& a : e.args) expr(*a);
    emit(Op::Call, it->second, argc, line);
}

} // namespace

CompileResult compile(const Program& program) {
    Compiler c;
    return c.run(program);
}

} // namespace sk
