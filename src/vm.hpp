#pragma once
#include "bytecode.hpp"
#include "natives.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace sk {

struct RuntimeError {
    std::string message;
};

struct DrawCmd {
    enum class Kind : uint8_t { Circle, Rect, Line };
    Kind kind;
    double a, b, c, d;   // circle: x y r _   rect: x y w h   line: x1 y1 x2 y2
    uint8_t r, g, bl;    // colour
};

enum class Status { Running, Finished, Error };

struct Frame {
    const Function* fn;
    size_t ip;     // index of the NEXT instruction to execute
    size_t base;   // index in the value stack where this frame's locals start
};

class VM {
public:
    explicit VM(const Module& module);  // the module must outlive the VM

    // Run until the program finishes, fails, or `maxSteps` instructions have executed.
    Status run(uint64_t maxSteps = 500'000'000ull);
    // Execute exactly one instruction (used by the step-through debugger UI).
    Status step();

    // ---- inspection
    Status status() const { return status_; }
    const std::string& error() const { return error_; }
    int errorLine() const { return errorLine_; }
    uint64_t steps() const { return steps_; }
    const std::vector<Value>& stack() const { return stack_; }
    const std::vector<Frame>& frames() const { return frames_; }
    const std::vector<Value>& globals() const { return globals_; }
    bool globalDefined(size_t i) const { return defined_[i] != 0; }
    const std::vector<DrawCmd>& draws() const { return draws_; }
    const std::vector<std::string>& output() const { return output_; }
    const Module& module() const { return mod_; }
    std::string format(const Value& v) const { return formatValue(v, mod_.strings); }

    // ---- services used by native functions
    void addDraw(DrawCmd::Kind kind, double a, double b, double c, double d) {
        if (draws_.size() >= kMaxDraws) throw RuntimeError{"Too many shapes (limit is 100000)"};
        draws_.push_back(DrawCmd{kind, a, b, c, d, cr_, cg_, cb_});
    }
    void setColor(uint8_t r, uint8_t g, uint8_t b) { cr_ = r; cg_ = g; cb_ = b; }
    void emitText(std::string line) {
        if (output_.size() >= kMaxOutputLines) throw RuntimeError{"Too much output (limit is 10000 lines)"};
        output_.push_back(std::move(line));
    }
    double nextRandom();  // uniform in [0, 1), deterministic

private:
    void execute();
    void fail(const std::string& message);

    void push(Value v) { stack_.push_back(v); }
    Value pop() { Value v = stack_.back(); stack_.pop_back(); return v; }

    static constexpr size_t kMaxFrames = 256;
    static constexpr size_t kMaxDraws = 100000;
    static constexpr size_t kMaxOutputLines = 10000;

    const Module& mod_;
    std::vector<Value> stack_;
    std::vector<Frame> frames_;
    std::vector<Value> globals_;
    std::vector<char> defined_;
    std::vector<DrawCmd> draws_;
    std::vector<std::string> output_;
    Status status_ = Status::Running;
    std::string error_;
    int errorLine_ = 0;
    uint64_t steps_ = 0;
    uint64_t rng_ = 0x9E3779B97F4A7C15ull;
    uint8_t cr_ = 0, cg_ = 0, cb_ = 0;
};

} // namespace sk
