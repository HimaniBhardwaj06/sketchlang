#pragma once
#include "value.hpp"
#include <string>
#include <vector>

namespace sk {

class VM;
using NativeFn = Value (*)(VM& vm, const Value* args);

// A built-in function. The compiler checks `arity`; the VM calls `fn`.
struct NativeDef {
    const char* name;
    int arity;
    NativeFn fn;
};

const std::vector<NativeDef>& nativeTable();
int findNative(const std::string& name);  // index into nativeTable(), or -1

} // namespace sk
