#include "natives.hpp"
#include "vm.hpp"
#include <cmath>

namespace sk {

namespace {

double num(const char* fn, const Value* a, int i) {
    if (a[i].type != ValueType::Number) {
        throw RuntimeError{std::string(fn) + "() expects a number for argument " + std::to_string(i + 1) +
                           ", got " + typeName(a[i])};
    }
    return a[i].n;
}

uint8_t channel(double v) {
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    return static_cast<uint8_t>(std::lround(v));
}

// ---- drawing (canvas coordinates run from 0 to 100 in both directions)
Value nCircle(VM& vm, const Value* a) {
    vm.addDraw(DrawCmd::Kind::Circle, num("circle", a, 0), num("circle", a, 1), num("circle", a, 2), 0);
    return Value();
}
Value nRect(VM& vm, const Value* a) {
    vm.addDraw(DrawCmd::Kind::Rect, num("rect", a, 0), num("rect", a, 1), num("rect", a, 2), num("rect", a, 3));
    return Value();
}
Value nLine(VM& vm, const Value* a) {
    vm.addDraw(DrawCmd::Kind::Line, num("line", a, 0), num("line", a, 1), num("line", a, 2), num("line", a, 3));
    return Value();
}
Value nColor(VM& vm, const Value* a) {
    vm.setColor(channel(num("color", a, 0)), channel(num("color", a, 1)), channel(num("color", a, 2)));
    return Value();
}

// ---- output and math
Value nPrint(VM& vm, const Value* a) {
    vm.emitText(vm.format(a[0]));
    return Value();
}
Value nSin(VM&, const Value* a) { return Value::number(std::sin(num("sin", a, 0))); }
Value nCos(VM&, const Value* a) { return Value::number(std::cos(num("cos", a, 0))); }
Value nAbs(VM&, const Value* a) { return Value::number(std::fabs(num("abs", a, 0))); }
Value nFloor(VM&, const Value* a) { return Value::number(std::floor(num("floor", a, 0))); }
Value nSqrt(VM&, const Value* a) {
    double x = num("sqrt", a, 0);
    if (x < 0) throw RuntimeError{"sqrt() of a negative number"};
    return Value::number(std::sqrt(x));
}
Value nRandom(VM& vm, const Value*) { return Value::number(vm.nextRandom()); }

} // namespace

const std::vector<NativeDef>& nativeTable() {
    static const std::vector<NativeDef> table = {
        {"circle", 3, nCircle}, {"rect", 4, nRect},   {"line", 4, nLine},   {"color", 3, nColor},
        {"print", 1, nPrint},   {"sin", 1, nSin},     {"cos", 1, nCos},     {"abs", 1, nAbs},
        {"floor", 1, nFloor},   {"sqrt", 1, nSqrt},   {"random", 0, nRandom},
    };
    return table;
}

int findNative(const std::string& name) {
    const auto& t = nativeTable();
    for (size_t i = 0; i < t.size(); i++) {
        if (name == t[i].name) return static_cast<int>(i);
    }
    return -1;
}

} // namespace sk
