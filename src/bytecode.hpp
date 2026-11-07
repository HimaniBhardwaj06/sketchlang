#pragma once
#include "value.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace sk {

enum class Op : uint8_t {
    Const,        // push constants[a]
    Nil, True, False,
    Pop,
    GetLocal,     // push stack[base + a]
    SetLocal,     // pop -> stack[base + a]
    GetGlobal, SetGlobal, DefGlobal,   // a = global index (SetGlobal / DefGlobal pop)
    Add, Sub, Mul, Div, Mod,
    Neg, Not,
    Eq, Neq, Lt, Le, Gt, Ge,
    Jump,         // ip = a
    JumpIfFalse,  // pop condition (must be boolean); if false, ip = a
    Call,         // call functions[a] with b arguments
    CallNative,   // call natives[a] with b arguments
    Return,       // pop result, leave the frame, push result for the caller
};

// One fixed-size instruction. Jump targets are absolute instruction indices.
struct Instr {
    Op op;
    int32_t a = 0;
    int32_t b = 0;
};

struct Function {
    std::string name;
    int arity = 0;
    int maxLocals = 0;          // number of local slots the frame needs (includes parameters)
    std::vector<Instr> code;
    std::vector<int> lines;     // source line of each instruction
};

struct Module {
    std::vector<Function> functions;       // functions[0] is the top-level program ("<main>")
    std::vector<Value> constants;
    std::vector<std::string> strings;      // interned string constants
    std::vector<std::string> globalNames;
};

const char* opName(Op op);
std::string formatValue(const Value& v, const std::vector<std::string>& strings);
std::string disassembleInstr(const Module& m, const Function& fn, size_t ip);
std::string disassemble(const Module& m);

} // namespace sk
