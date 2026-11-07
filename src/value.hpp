#pragma once
#include <cstdint>
#include <string>

namespace sk {

enum class ValueType : uint8_t { Nil, Bool, Number, String };

// A small, trivially-copyable runtime value (16 bytes).
// Strings are interned: `s` is an index into Module::strings, so two equal
// strings always have the same index and comparing them is a single integer compare.
struct Value {
    ValueType type = ValueType::Nil;
    union {
        bool b;
        double n;
        uint32_t s;
    };

    Value() : n(0) {}
    static Value boolean(bool v) { Value r; r.type = ValueType::Bool; r.b = v; return r; }
    static Value number(double v) { Value r; r.type = ValueType::Number; r.n = v; return r; }
    static Value string(uint32_t idx) { Value r; r.type = ValueType::String; r.s = idx; return r; }
};

inline const char* typeName(ValueType t) {
    switch (t) {
        case ValueType::Nil:    return "nil";
        case ValueType::Bool:   return "boolean";
        case ValueType::Number: return "number";
        case ValueType::String: return "string";
    }
    return "?";
}
inline const char* typeName(const Value& v) { return typeName(v.type); }

inline bool valuesEqual(const Value& a, const Value& b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case ValueType::Nil:    return true;
        case ValueType::Bool:   return a.b == b.b;
        case ValueType::Number: return a.n == b.n;
        case ValueType::String: return a.s == b.s;
    }
    return false;
}

} // namespace sk
