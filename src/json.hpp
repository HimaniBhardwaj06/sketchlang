#pragma once
#include "ast.hpp"
#include "bytecode.hpp"
#include "token.hpp"
#include "vm.hpp"
#include <string>
#include <type_traits>
#include <vector>

namespace sk {

// A tiny JSON writer. It handles commas and string escaping so the rest of
// the code only says what to write, not how.
class JsonWriter {
public:
    void beginObject() { sep(); out_ += '{'; first_.push_back(true); }
    void endObject()   { out_ += '}'; first_.pop_back(); }
    void beginArray()  { sep(); out_ += '['; first_.push_back(true); }
    void endArray()    { out_ += ']'; first_.pop_back(); }
    void key(const std::string& k) { sep(); writeString(k); out_ += ':'; afterKey_ = true; }

    void value(const std::string& s) { sep(); writeString(s); }
    void value(const char* s) { sep(); writeString(s); }
    void value(bool b) { sep(); out_ += b ? "true" : "false"; }
    void value(double d);
    template <class T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
    void value(T v) { sep(); out_ += std::to_string(v); }
    void nullValue() { sep(); out_ += "null"; }

    const std::string& str() const { return out_; }

private:
    void sep();
    void writeString(const std::string& s);

    std::string out_;
    std::vector<bool> first_;
    bool afterKey_ = false;
};

void writeTokens(JsonWriter& w, const std::vector<Token>& tokens);
void writeAst(JsonWriter& w, const Program& program);
void writeBytecode(JsonWriter& w, const Module& module);
void writeVmState(JsonWriter& w, const VM& vm);
void writeDraws(JsonWriter& w, const std::vector<DrawCmd>& draws);

} // namespace sk
