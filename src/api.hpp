#pragma once

// The C interface that JavaScript calls (through WebAssembly).
// Every function returning const char* returns JSON; the pointer stays valid
// until the next call to the same function.
//
// Typical use from the browser:
//   sk_compile(source)  -> tokens, AST, bytecode, errors  (also prepares the VM)
//   sk_step() / sk_run(n)                                  (execute)
//   sk_state() / sk_draws()                                (inspect)

#define SK_EXPORT __attribute__((used, visibility("default")))

extern "C" {

SK_EXPORT const char* sk_compile(const char* source);
SK_EXPORT const char* sk_state();
SK_EXPORT const char* sk_draws();
SK_EXPORT int sk_step();               // returns 0 running, 1 finished, 2 error
SK_EXPORT int sk_run(int max_steps);   // returns 1 finished or 2 error (hitting max_steps is an error)
SK_EXPORT int sk_reset();              // restart the compiled program from the beginning
SK_EXPORT const char* sk_version();

}
