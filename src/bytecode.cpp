#include "bytecode.hpp"
#include "natives.hpp"
#include <iomanip>
#include <sstream>

namespace sk {

const char* opName(Op op) {
    switch (op) {
        case Op::Const:       return "CONST";
        case Op::Nil:         return "NIL";
        case Op::True:        return "TRUE";
        case Op::False:       return "FALSE";
        case Op::Pop:         return "POP";
        case Op::GetLocal:    return "GET_LOCAL";
        case Op::SetLocal:    return "SET_LOCAL";
        case Op::GetGlobal:   return "GET_GLOBAL";
        case Op::SetGlobal:   return "SET_GLOBAL";
        case Op::DefGlobal:   return "DEF_GLOBAL";
        case Op::Add:         return "ADD";
        case Op::Sub:         return "SUB";
        case Op::Mul:         return "MUL";
        case Op::Div:         return "DIV";
        case Op::Mod:         return "MOD";
        case Op::Neg:         return "NEG";
        case Op::Not:         return "NOT";
        case Op::Eq:          return "EQ";
        case Op::Neq:         return "NEQ";
        case Op::Lt:          return "LT";
        case Op::Le:          return "LE";
        case Op::Gt:          return "GT";
        case Op::Ge:          return "GE";
        case Op::Jump:        return "JUMP";
        case Op::JumpIfFalse: return "JUMP_IF_FALSE";
        case Op::Call:        return "CALL";
        case Op::CallNative:  return "CALL_NATIVE";
        case Op::Return:      return "RETURN";
    }
    return "?";
}

std::string formatValue(const Value& v, const std::vector<std::string>& strings) {
    switch (v.type) {
        case ValueType::Nil:    return "nil";
        case ValueType::Bool:   return v.b ? "true" : "false";
        case ValueType::String: return v.s < strings.size() ? strings[v.s] : "?";
        case ValueType::Number: {
            std::ostringstream os;
            os << std::setprecision(15) << v.n;
            return os.str();
        }
    }
    return "?";
}

InstrView describeInstr(const Module& m, const Function& fn, size_t ip) {
    const Instr& in = fn.code[ip];
    std::string operand, note;
    switch (in.op) {
        case Op::Const:
            operand = std::to_string(in.a);
            note = formatValue(m.constants[in.a], m.strings);
            if (m.constants[in.a].type == ValueType::String) note = "\"" + note + "\"";
            break;
        case Op::GetLocal:
        case Op::SetLocal:
            operand = std::to_string(in.a);
            break;
        case Op::GetGlobal:
        case Op::SetGlobal:
        case Op::DefGlobal:
            operand = std::to_string(in.a);
            note = m.globalNames[in.a];
            break;
        case Op::Jump:
        case Op::JumpIfFalse:
            operand = std::to_string(in.a);
            break;
        case Op::Call:
            operand = std::to_string(in.a);
            note = m.functions[in.a].name + " (" + std::to_string(in.b) + " args)";
            break;
        case Op::CallNative:
            operand = std::to_string(in.a);
            note = std::string(nativeTable()[in.a].name) + " (" + std::to_string(in.b) + " args)";
            break;
        default:
            break;
    }
    return InstrView{operand, note};
}

std::string disassembleInstr(const Module& m, const Function& fn, size_t ip) {
    const Instr& in = fn.code[ip];
    InstrView view = describeInstr(m, fn, ip);
    const std::string& operand = view.operand;
    const std::string& note = view.note;
    std::ostringstream os;
    os << std::setfill('0') << std::setw(4) << ip << std::setfill(' ')
       << "  L" << std::left << std::setw(4) << fn.lines[ip]
       << std::setw(15) << opName(in.op) << std::setw(5) << operand;
    std::string out = os.str();
    if (!note.empty()) out += "; " + note;
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

std::string disassemble(const Module& m) {
    std::string out;
    for (const Function& fn : m.functions) {
        out += "== " + fn.name + " ==\n";
        for (size_t ip = 0; ip < fn.code.size(); ip++) out += disassembleInstr(m, fn, ip) + "\n";
        out += "\n";
    }
    return out;
}

} // namespace sk
