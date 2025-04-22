// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_RUNTIME_BUILTIN_H_INCLUDED
#define LAUF_RUNTIME_BUILTIN_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

#if defined(__clang__)
#    if defined(__APPLE__) && defined(__MACH__)
#        define LAUF_RUNTIME_BUILTIN_IMPL __attribute__((section(".text,lauf_builtin"), aligned(8)))
#    else
#        define LAUF_RUNTIME_BUILTIN_IMPL __attribute__((section("text.lauf_builtin"), aligned(8)))
#    endif
#elif defined(__GNUC__) || defined(__GNUG__)
#    define LAUF_RUNTIME_BUILTIN_IMPL __attribute__((section(".text.lauf_builtin"), aligned(8)))
#elif defined(_MSC_VER)
#    define LAUF_RUNTIME_BUILTIN_IMPL __declspec(code_seg(".text.lauf_builtin"))
#else
#    define LAUF_RUNTIME_BUILTIN_IMPL
#endif

typedef union lauf_asm_inst             lauf_asm_inst;
typedef struct lauf_asm_type            lauf_asm_type;
typedef union lauf_runtime_value        lauf_runtime_value;
typedef struct lauf_runtime_process     lauf_runtime_process;
typedef struct lauf_runtime_stack_frame lauf_runtime_stack_frame;

typedef enum lauf_runtime_builtin_flags
{
    LAUF_RUNTIME_BUILTIN_DEFAULT = 0,

    /// The builtin will never panic.
    LAUF_RUNTIME_BUILTIN_NO_PANIC = 1 << 0,
    /// The builtin does not need the process.
    /// It may only use the `process` argument to call `lauf_runtime_panic()`.
    LAUF_RUNTIME_BUILTIN_NO_PROCESS = 1 << 1,

    /// The builtin is a VM directive, with a signature N => 0.
    /// If used with other backends, it has no effect besides removing the arguments.
    LAUF_RUNTIME_BUILTIN_VM_DIRECTIVE = 1 << 2,
    /// The builtin can be constant folded.
    /// Builtin can only access `vstack_ptr`; everything else is `nullptr`.
    LAUF_RUNTIME_BUILTIN_CONSTANT_FOLD = 1 << 3,
    /// The builtin will always panic.
    /// Calls to it are treated as a block terminator.
    LAUF_RUNTIME_BUILTIN_ALWAYS_PANIC = 1 << 4,
} lauf_runtime_builtin_flags;

#if LAUF_HAS_TAIL_CALL_ELIMINATION
#    define LAUF_BUILTIN_RETURN_TYPE bool
#else
typedef union lauf_tail_call_result lauf_tail_call_result;
#    define LAUF_BUILTIN_RETURN_TYPE lauf_tail_call_result
#endif

/// Must be tail-called when a buitlin finishes succesfully.
LAUF_RUNTIME_BUILTIN_IMPL LAUF_BUILTIN_RETURN_TYPE lauf_runtime_builtin_dispatch(
    const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr, lauf_runtime_stack_frame* frame_ptr,
    lauf_runtime_process* process);

/// The signature of the implementation of a builtin.
typedef LAUF_BUILTIN_RETURN_TYPE lauf_runtime_builtin_impl(const lauf_asm_inst*      ip,
                                                           lauf_runtime_value*       vstack_ptr,
                                                           lauf_runtime_stack_frame* frame_ptr,
                                                           lauf_runtime_process*     process);

/// A builtin function.
typedef struct lauf_runtime_builtin
{
    /// The actual implementation.
    lauf_runtime_builtin_impl* impl;
    /// The signature.
    uint8_t input_count;
    uint8_t output_count;
    /// Any flags.
    int flags;
    /// The name, used for debugging and some frontends/backends.
    const char* name;
    /// A next pointer so a linked list of all builtins in a builtin library can be formed.
    const lauf_runtime_builtin* next;
} lauf_runtime_builtin;

#if !LAUF_HAS_TAIL_CALL_ELIMINATION
typedef union lauf_tail_call_result
{
    struct
    {
        bool                       is_function;
        lauf_runtime_builtin_impl* next_func;
        const lauf_asm_inst*       cur_ip;
        lauf_runtime_value*        cur_stack_ptr;
        lauf_runtime_stack_frame*  cur_frame_ptr;
        lauf_runtime_process*      cur_process;
    } function;
    struct
    {
        bool is_function;
        bool value;
    } value;
} lauf_tail_call_result;

#    define LAUF_BUILTIN_RETURN(...)                                                               \
        return                                                                                     \
        {                                                                                          \
            .value                                                                                 \
            {                                                                                      \
                false, __VA_ARGS__                                                                 \
            }                                                                                      \
        }
#else
#    define LAUF_BUILTIN_RETURN(...) return __VA_ARGS__
#endif

#define LAUF_RUNTIME_BUILTIN(ConstantName, InputCount, OutputCount, Flags, Name, Next)             \
    static LAUF_BUILTIN_RETURN_TYPE ConstantName##_impl(const lauf_asm_inst*      ip,              \
                                                        lauf_runtime_value*       vstack_ptr,      \
                                                        lauf_runtime_stack_frame* frame_ptr,       \
                                                        lauf_runtime_process*     process);            \
    const lauf_runtime_builtin      ConstantName                                                   \
        = {&ConstantName##_impl, InputCount, OutputCount, Flags, Name, Next};                      \
    LAUF_RUNTIME_BUILTIN_IMPL static LAUF_BUILTIN_RETURN_TYPE                                      \
        ConstantName##_impl(const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr,               \
                            lauf_runtime_stack_frame* frame_ptr, lauf_runtime_process* process)

#define LAUF_RUNTIME_BUILTIN_DISPATCH                                                              \
    LAUF_TAIL_CALL return lauf_runtime_builtin_dispatch(ip, vstack_ptr, frame_ptr, process)

/// A builtin library.
typedef struct lauf_runtime_builtin_library
{
    /// A prefix that will be added to all functions in the library (separated by `.`).
    const char* prefix;
    /// The first builtin function of the library.
    const lauf_runtime_builtin* functions;
    /// The first type of the library.
    const lauf_asm_type* types;
} lauf_runtime_builtin_library;

LAUF_HEADER_END

#endif // LAUF_RUNTIME_BUILTIN_H_INCLUDED
