#pragma once
#include "ast.hpp"
#include "bytecode.hpp"
#include <string>
#include <vector>

namespace sk {

struct CompileResult {
    Module module;
    std::vector<std::string> errors;  // "line:col: message"
    bool ok() const { return errors.empty(); }
};

CompileResult compile(const Program& program);

} // namespace sk
