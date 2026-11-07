#include "api.hpp"
#include "compiler.hpp"
#include "json.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "vm.hpp"
#include <memory>
#include <string>

namespace {

struct Session {
    std::unique_ptr<sk::Module> module;
    std::unique_ptr<sk::VM> vm;
};

Session g_session;
std::string g_compileOut, g_stateOut, g_drawsOut;

int statusCode(sk::Status s) {
    return s == sk::Status::Running ? 0 : s == sk::Status::Finished ? 1 : 2;
}

// Diagnostics look like "3:7: message". Split them into fields for the UI.
void writeDiagnostic(sk::JsonWriter& w, const char* stage, const std::string& text) {
    int line = 0, col = 0;
    std::string message = text;
    size_t a = text.find(':');
    size_t b = a == std::string::npos ? a : text.find(':', a + 1);
    if (a != std::string::npos && b != std::string::npos) {
        try {
            line = std::stoi(text.substr(0, a));
            col = std::stoi(text.substr(a + 1, b - a - 1));
            message = text.substr(b + 1);
            if (!message.empty() && message[0] == ' ') message.erase(0, 1);
        } catch (...) {
            line = col = 0;
            message = text;
        }
    }
    w.beginObject();
    w.key("stage");   w.value(stage);
    w.key("line");    w.value(line);
    w.key("col");     w.value(col);
    w.key("message"); w.value(message);
    w.endObject();
}

} // namespace

extern "C" {

const char* sk_compile(const char* source) {
    g_session.vm.reset();
    g_session.module.reset();

    std::string src = source ? source : "";
    std::vector<sk::Token> tokens = sk::Lexer(src).tokenize();

    sk::Parser parser(tokens);
    sk::ParseResult parsed = parser.parse();

    std::vector<std::pair<const char*, std::string>> diagnostics;
    for (const auto& e : parsed.errors) diagnostics.push_back({"parse", e});

    bool compiled = false;
    sk::CompileResult result;
    if (parsed.ok()) {
        result = sk::compile(parsed.program);
        for (const auto& e : result.errors) diagnostics.push_back({"compile", e});
        compiled = result.ok();
    }

    if (compiled) {
        g_session.module = std::make_unique<sk::Module>(std::move(result.module));
        g_session.vm = std::make_unique<sk::VM>(*g_session.module);
    }

    sk::JsonWriter w;
    w.beginObject();
    w.key("ok"); w.value(compiled);
    w.key("errors");
    w.beginArray();
    for (const auto& d : diagnostics) writeDiagnostic(w, d.first, d.second);
    w.endArray();
    w.key("tokens"); sk::writeTokens(w, tokens);
    w.key("ast");
    if (parsed.ok()) sk::writeAst(w, parsed.program); else w.nullValue();
    w.key("bytecode");
    if (compiled) sk::writeBytecode(w, *g_session.module); else w.nullValue();
    w.endObject();

    g_compileOut = w.str();
    return g_compileOut.c_str();
}

const char* sk_state() {
    sk::JsonWriter w;
    if (!g_session.vm) {
        w.beginObject();
        w.key("status"); w.value("none");
        w.endObject();
    } else {
        sk::writeVmState(w, *g_session.vm);
    }
    g_stateOut = w.str();
    return g_stateOut.c_str();
}

const char* sk_draws() {
    sk::JsonWriter w;
    if (!g_session.vm) {
        w.beginArray();
        w.endArray();
    } else {
        sk::writeDraws(w, g_session.vm->draws());
    }
    g_drawsOut = w.str();
    return g_drawsOut.c_str();
}

int sk_step() {
    if (!g_session.vm) return 2;
    return statusCode(g_session.vm->step());
}

int sk_run(int max_steps) {
    if (!g_session.vm) return 2;
    if (max_steps < 0) max_steps = 0;
    return statusCode(g_session.vm->run(static_cast<uint64_t>(max_steps)));
}

int sk_reset() {
    if (!g_session.module) return 2;
    g_session.vm = std::make_unique<sk::VM>(*g_session.module);
    return 0;
}

const char* sk_version() { return "sketchlang 0.4"; }

} // extern "C"
