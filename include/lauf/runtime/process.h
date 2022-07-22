// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_RUNTIME_PROCESS_H_INCLUDED
#define LAUF_RUNTIME_PROCESS_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_asm_layout               lauf_asm_layout;
typedef struct lauf_asm_program              lauf_asm_program;
typedef struct lauf_asm_function             lauf_asm_function;
typedef union lauf_asm_inst                  lauf_asm_inst;
typedef struct lauf_asm_signature            lauf_asm_signature;
typedef struct lauf_runtime_stacktrace       lauf_runtime_stacktrace;
typedef struct lauf_runtime_address          lauf_runtime_address;
typedef struct lauf_runtime_function_address lauf_runtime_function_address;
typedef union lauf_runtime_value             lauf_runtime_value;

/// Represents a currently running lauf program.
typedef struct lauf_runtime_process lauf_runtime_process;

/// A stack frame of the process.
typedef struct lauf_runtime_stack_frame lauf_runtime_stack_frame;

//=== queries ===//
/// The program that is running.
const lauf_asm_program* lauf_runtime_get_program(lauf_runtime_process* p);

/// Returns the base of the vstack (highest address as it grows down).
const lauf_runtime_value* lauf_runtime_get_vstack_base(lauf_runtime_process* p);

/// Returns the current stacktrace of the process.
lauf_runtime_stacktrace* lauf_runtime_get_stacktrace(lauf_runtime_process* p);

//=== actions ===//
/// Calls the given function.
///
/// It behaves like `lauf_vm_execute()` but re-uses the existing VM of the process.
/// The function must be part of the program.
bool lauf_runtime_call(lauf_runtime_process* p, const lauf_asm_function* fn,
                       lauf_runtime_value* vstack_ptr);

/// Triggers a panic.
///
/// It invokes the panic handler, but does not abort or anything.
/// The builtin needs to do that by returning false.
///
/// The function always returns false for convenience.
bool lauf_runtime_panic(lauf_runtime_process* p, const lauf_asm_inst* ip, const char* msg);

//=== address ===//
/// Converts an address to a pointer if the address is readable for the layout.
/// Returns null otherwise.
const void* lauf_runtime_get_const_ptr(lauf_runtime_process* p, lauf_runtime_address addr,
                                       lauf_asm_layout layout);

/// Converts an address to a pointer if the address is readable and writeable for the layout.
/// Returns null otherwise.
void* lauf_runtime_get_mut_ptr(lauf_runtime_process* p, lauf_runtime_address addr,
                               lauf_asm_layout layout);

/// Converts an address to a pointer if there is a null-terminated string starting at address.
/// Returns null otherwise.
const char* lauf_runtime_get_cstr(lauf_runtime_process* p, lauf_runtime_address addr);

/// Converts a function address into a function pointer if it is valid.
/// Returns null otherwise.
const lauf_asm_function* lauf_runtime_get_function_ptr_any(lauf_runtime_process*         p,
                                                           lauf_runtime_function_address addr);

/// Converts a function address into a function pointer if it is valid and has the specified
/// signature. Returns null otherwise.
const lauf_asm_function* lauf_runtime_get_function_ptr(lauf_runtime_process*         p,
                                                       lauf_runtime_function_address addr,
                                                       lauf_asm_signature            signature);

LAUF_HEADER_END

#endif // LAUF_RUNTIME_PROCESS_H_INCLUDED

