#include "vm.hpp"
#include <cmath>

namespace sk {

VM::VM(const Module& module) : mod_(module) {
    stack_.reserve(1024);
    frames_.reserve(kMaxFrames);
    globals_.assign(mod_.globalNames.size(), Value());
    defined_.assign(mod_.globalNames.size(), 0);
    const Function& main = mod_.functions[0];
    stack_.resize(static_cast<size_t>(main.maxLocals));
    frames_.push_back(Frame{&main, 0, 0});
}

double VM::nextRandom() {
    rng_ ^= rng_ >> 12;
    rng_ ^= rng_ << 25;
    rng_ ^= rng_ >> 27;
    uint64_t r = rng_ * 2685821657736338717ull;
    return static_cast<double>(r >> 11) * (1.0 / 9007199254740992.0);
}

void VM::fail(const std::string& message) {
    status_ = Status::Error;
    error_ = message;
    if (!frames_.empty()) {
        const Frame& f = frames_.back();
        size_t at = f.ip > 0 ? f.ip - 1 : 0;
        if (at < f.fn->lines.size()) errorLine_ = f.fn->lines[at];
    }
}

Status VM::run(uint64_t maxSteps) {
    uint64_t limit = steps_ + maxSteps;
    try {
        while (status_ == Status::Running) {
            if (steps_ >= limit) throw RuntimeError{"Instruction limit exceeded (possible infinite loop)"};
            execute();
        }
    } catch (const RuntimeError& e) {
        fail(e.message);
    }
    return status_;
}

Status VM::step() {
    if (status_ != Status::Running) return status_;
    try {
        execute();
    } catch (const RuntimeError& e) {
        fail(e.message);
    }
    return status_;
}

void VM::execute() {
    Frame& f = frames_.back();
    const Instr in = f.fn->code[f.ip++];
    ++steps_;

    switch (in.op) {
        case Op::Const: push(mod_.constants[static_cast<size_t>(in.a)]); break;
        case Op::Nil:   push(Value()); break;
        case Op::True:  push(Value::boolean(true)); break;
        case Op::False: push(Value::boolean(false)); break;
        case Op::Pop:   stack_.pop_back(); break;

        case Op::GetLocal: {
            Value v = stack_[f.base + static_cast<size_t>(in.a)];
            push(v);
            break;
        }
        case Op::SetLocal: {
            Value v = pop();
            stack_[f.base + static_cast<size_t>(in.a)] = v;
            break;
        }

        case Op::GetGlobal: {
            size_t g = static_cast<size_t>(in.a);
            if (!defined_[g]) throw RuntimeError{"Undefined variable '" + mod_.globalNames[g] + "'"};
            push(globals_[g]);
            break;
        }
        case Op::SetGlobal: {
            size_t g = static_cast<size_t>(in.a);
            if (!defined_[g]) throw RuntimeError{"Undefined variable '" + mod_.globalNames[g] + "'"};
            globals_[g] = pop();
            break;
        }
        case Op::DefGlobal: {
            size_t g = static_cast<size_t>(in.a);
            globals_[g] = pop();
            defined_[g] = 1;
            break;
        }

        case Op::Add:
        case Op::Sub:
        case Op::Mul:
        case Op::Div:
        case Op::Mod: {
            Value b = pop();
            Value a = pop();
            if (a.type != ValueType::Number || b.type != ValueType::Number) {
                throw RuntimeError{std::string("Operands must be numbers, got ") + typeName(a) + " and " + typeName(b)};
            }
            double r = 0;
            switch (in.op) {
                case Op::Add: r = a.n + b.n; break;
                case Op::Sub: r = a.n - b.n; break;
                case Op::Mul: r = a.n * b.n; break;
                case Op::Div:
                    if (b.n == 0) throw RuntimeError{"Division by zero"};
                    r = a.n / b.n;
                    break;
                default:
                    if (b.n == 0) throw RuntimeError{"Modulo by zero"};
                    r = std::fmod(a.n, b.n);
                    break;
            }
            push(Value::number(r));
            break;
        }

        case Op::Neg: {
            Value a = pop();
            if (a.type != ValueType::Number) throw RuntimeError{std::string("Operand must be a number, got ") + typeName(a)};
            push(Value::number(-a.n));
            break;
        }
        case Op::Not: {
            Value a = pop();
            if (a.type != ValueType::Bool) throw RuntimeError{std::string("Operand of '!' must be a boolean, got ") + typeName(a)};
            push(Value::boolean(!a.b));
            break;
        }

        case Op::Eq:
        case Op::Neq: {
            Value b = pop();
            Value a = pop();
            bool eq = valuesEqual(a, b);
            push(Value::boolean(in.op == Op::Eq ? eq : !eq));
            break;
        }

        case Op::Lt:
        case Op::Le:
        case Op::Gt:
        case Op::Ge: {
            Value b = pop();
            Value a = pop();
            if (a.type != ValueType::Number || b.type != ValueType::Number) {
                throw RuntimeError{std::string("Can only compare numbers, got ") + typeName(a) + " and " + typeName(b)};
            }
            bool r = false;
            switch (in.op) {
                case Op::Lt: r = a.n < b.n; break;
                case Op::Le: r = a.n <= b.n; break;
                case Op::Gt: r = a.n > b.n; break;
                default:     r = a.n >= b.n; break;
            }
            push(Value::boolean(r));
            break;
        }

        case Op::Jump:
            f.ip = static_cast<size_t>(in.a);
            break;

        case Op::JumpIfFalse: {
            Value c = pop();
            if (c.type != ValueType::Bool) throw RuntimeError{std::string("Condition must be a boolean, got ") + typeName(c)};
            if (!c.b) f.ip = static_cast<size_t>(in.a);
            break;
        }

        case Op::Call: {
            if (frames_.size() >= kMaxFrames) throw RuntimeError{"Stack overflow (too many nested calls)"};
            const Function& callee = mod_.functions[static_cast<size_t>(in.a)];
            size_t base = stack_.size() - static_cast<size_t>(in.b);   // arguments become locals 0..n-1
            stack_.resize(base + static_cast<size_t>(callee.maxLocals));
            frames_.push_back(Frame{&callee, 0, base});
            break;
        }

        case Op::CallNative: {
            const NativeDef& nd = nativeTable()[static_cast<size_t>(in.a)];
            size_t argc = static_cast<size_t>(in.b);
            Value result = nd.fn(*this, stack_.data() + (stack_.size() - argc));
            stack_.resize(stack_.size() - argc);
            push(result);
            break;
        }

        case Op::Return: {
            Value result = pop();
            size_t base = f.base;
            if (frames_.size() == 1) {
                status_ = Status::Finished;
                return;
            }
            stack_.resize(base);
            frames_.pop_back();
            push(result);
            break;
        }
    }
}

} // namespace sk
