// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_VM_EXECUTE_HPP_INCLUDED
#define SRC_LAUF_VM_EXECUTE_HPP_INCLUDED

#include <lauf/asm/instruction.hpp>
#include <lauf/config.h>
#include <lauf/runtime/builtin.h>

namespace lauf
{
#if LAUF_CONFIG_DISPATCH_JUMP_TABLE

extern lauf_runtime_builtin_impl* const _vm_dispatch_table[];

#    define LAUF_VM_DISPATCH                                                                       \
        LAUF_TAIL_CALL return lauf::_vm_dispatch_table[int(ip->op())](ip, vstack_ptr, frame_ptr,   \
                                                                      process)

#else

bool _vm_dispatch(const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr,
                  lauf_runtime_stack_frame* frame_ptr, lauf_runtime_process* process);

#    define LAUF_VM_DISPATCH                                                                       \
        LAUF_TAIL_CALL return lauf::_vm_dispatch(ip, vstack_ptr, frame_ptr, process)

#endif
} // namespace lauf

namespace lauf
{
constexpr lauf_asm_inst trampoline_code[3] = {
    // We need one nop instruction in front, so we can use it for fiber creation.
    // (Resume will always increment the ip first, which goes to the real call instruction)
    lauf_asm_inst(),
    [] {
        // We first want to call the function specified in the trampoline stack frame.
        lauf_asm_inst result;
        result.call.op     = lauf::asm_op::call;
        result.call.offset = 0;
        return result;
    }(),
    [] {
        // We then want to exit.
        lauf_asm_inst result;
        result.exit.op = lauf::asm_op::exit;
        return result;
    }(),
};

inline bool execute(const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr,
                    lauf_runtime_stack_frame* frame_ptr, lauf_runtime_process* process)
{
    LAUF_VM_DISPATCH;
}
} // namespace lauf

// Clang supports inline of previously non-inlined function
// GCC does
#if !defined(__clang__) && (defined(__GNUC__) || defined(__GNUG__))
extern "C" [[gnu::always_inline]] inline bool lauf_runtime_builtin_dispatch_inline(
    const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr, lauf_runtime_stack_frame* frame_ptr,
    lauf_runtime_process* process)
#else
inline bool lauf_runtime_builtin_dispatch(const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr,
                                          lauf_runtime_stack_frame* frame_ptr,
                                          lauf_runtime_process*     process)
#endif
{
    assert(ip[1].op() == lauf::asm_op::call_builtin_sig);
    ip += 2;
    LAUF_VM_DISPATCH;
}

#if !defined(__clang__) && (defined(__GNUC__) || defined(__GNUG__))
#    define lauf_runtime_builtin_dispatch lauf_runtime_builtin_dispatch_inline
#    undef LAUF_RUNTIME_BUILTIN_DISPATCH
#    define LAUF_RUNTIME_BUILTIN_DISPATCH                                                          \
        LAUF_TAIL_CALL return lauf_runtime_builtin_dispatch_inline(ip, vstack_ptr, frame_ptr,      \
                                                                   process)
#endif

#endif // SRC_LAUF_VM_EXECUTE_HPP_INCLUDED
