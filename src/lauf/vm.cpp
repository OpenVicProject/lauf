// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/vm.hpp>

#include <cassert>
#include <cstdio>
#include <lauf/asm/module.hpp>
#include <lauf/asm/program.hpp>
#include <lauf/lib/debug.h>
#include <lauf/runtime/builtin.h>
#include <lauf/runtime/process.h>
#include <lauf/runtime/stacktrace.h>

const lauf_vm_options lauf_default_vm_options
    = {512 * 1024ull, 16 * 1024ull, [](lauf_runtime_process* process, const char* msg) {
           std::fprintf(stderr, "[lauf] panic: %s\n",
                        msg == nullptr ? "(invalid message pointer)" : msg);
           lauf_lib_debug_print_cstack.impl(process, nullptr, nullptr);
       }};

lauf_vm* lauf_create_vm(lauf_vm_options options)
{
    return lauf_vm::create(options);
}

void lauf_destroy_vm(lauf_vm* vm)
{
    lauf_vm::destroy(vm);
}

namespace
{
lauf::allocation allocate_global(lauf::intrinsic_arena<lauf_vm>* arena, lauf_asm_global global)
{
    lauf::allocation result;

    if (global.memory != nullptr)
    {
        result.ptr = arena->memdup(global.memory, global.size);
    }
    else
    {
        result.ptr = arena->allocate(global.size, alignof(void*));
        std::memset(result.ptr, 0, global.size);
    }

    // If bigger than 32bit, only the lower parts are addressable.
    result.size = std::uint32_t(global.size);

    result.source     = global.perms == lauf_asm_global::read_write
                            ? lauf::allocation_source::static_mut_memory
                            : lauf::allocation_source::static_const_memory;
    result.status     = lauf::allocation_status::allocated;
    result.generation = 0;

    return result;
}

void start_process(lauf_runtime_process* process, lauf_vm* vm, const lauf_asm_program* program)
{
    process->vm      = vm;
    process->program = program;

    process->allocations.resize_uninitialized(*vm, program->mod->globals_count);
    for (auto global = program->mod->globals; global != nullptr; global = global->next)
        process->allocations[global->allocation_idx] = allocate_global(vm, *global);
}

bool root_call(lauf_runtime_process* process, lauf_runtime_value* vstack_ptr, void* cstack_base,
               const lauf_asm_function* fn, const lauf_runtime_value* input,
               lauf_runtime_value* output)
{
    auto sig = lauf_asm_function_signature(fn);

    // Create the initial stack frame.
    auto frame_ptr = ::new (cstack_base) lauf::stack_frame{fn, nullptr, nullptr};
    assert(frame_ptr->is_trampoline_frame());

    // Push input values onto the value stack.
    for (auto i = 0u; i != sig.input_count; ++i)
    {
        --vstack_ptr;
        vstack_ptr[0] = input[i];
    }

    // Create the trampoline.
    lauf::asm_inst trampoline[2];
    trampoline[0].call.op = lauf::asm_op::call;
    // The current function of the stack frame is the one we want to call.
    trampoline[0].call.offset = 0;
    trampoline[1].exit.op     = lauf::asm_op::exit;

    // Execute the trampoline.
    if (!lauf::execute(trampoline, vstack_ptr, frame_ptr, process))
        return false;

    // Pop output values from the value stack.
    vstack_ptr = process->vm->vstack_base - sig.output_count;
    for (auto i = 0u; i != sig.output_count; ++i)
    {
        output[sig.output_count - i - 1] = vstack_ptr[0];
        ++vstack_ptr;
    }

    return true;
}
} // namespace

bool lauf_runtime_call(lauf_runtime_process* process, const lauf_asm_function* fn,
                       const lauf_runtime_value* input, lauf_runtime_value* output)
{
    auto frame_ptr      = process->frame_ptr;
    auto vstack_ptr     = process->vstack_ptr;
    auto result         = root_call(process, vstack_ptr, frame_ptr->prev + 1, fn, input, output);
    process->frame_ptr  = frame_ptr;
    process->vstack_ptr = vstack_ptr;
    return result;
}

bool lauf_vm_execute(lauf_vm* vm, lauf_asm_program* program, const lauf_runtime_value* input,
                     lauf_runtime_value* output)
{
    auto fn = lauf_asm_entry_function(program);

    lauf_runtime_process process;
    start_process(&process, vm, program);
    return root_call(&process, vm->vstack_base, vm->cstack_base, fn, input, output);
}

bool lauf_vm_execute_oneshot(lauf_vm* vm, lauf_asm_program* program,
                             const lauf_runtime_value* input, lauf_runtime_value* output)
{
    auto result = lauf_vm_execute(vm, program, input, output);
    lauf_asm_destroy_program(program);
    return result;
}

