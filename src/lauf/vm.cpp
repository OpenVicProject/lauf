// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/vm.hpp>

#include <cassert>
#include <cstdio>
#include <lauf/asm/module.hpp>
#include <lauf/asm/program.hpp>

const lauf_vm_options lauf_default_vm_options
    = {512 * 1024ull, 16 * 1024ull, [](lauf_runtime_process*, const char* msg) {
           std::fprintf(stderr, "[lauf] panic: %s\n", msg);
       }};

lauf_vm* lauf_create_vm(lauf_vm_options options)
{
    return lauf_vm::create(options);
}

void lauf_destroy_vm(lauf_vm* vm)
{
    lauf_vm::destroy(vm);
}

bool lauf_vm_execute(lauf_vm* vm, lauf_asm_program* program, const lauf_runtime_value* input,
                     lauf_runtime_value* output)
{
    auto fn  = program->entry;
    auto sig = fn->sig;

    // Setup a new process.
    assert(vm->process.vm == nullptr);
    vm->process.vm = vm;

    // Create the initial stack frame.
    auto frame_ptr = ::new (vm->cstack_base) lauf::stack_frame{fn, nullptr, nullptr};

    // Push input values onto the value stack.
    auto vstack_ptr = vm->vstack_base;
    for (auto i = 0u; i != sig.input_count; ++i)
    {
        --vstack_ptr;
        vstack_ptr[0] = input[i];
    }

    // Create the trampoline.
    lauf::asm_inst trampoline[2];
    trampoline[0].call.op = lauf::asm_op::call;
    trampoline[0].call.offset
        = 0; // The current function of the stack frame is the one we want to call.
    trampoline[1].exit.op = lauf::asm_op::exit;

    // Execute the trampoline.
    if (!lauf::execute(trampoline, vstack_ptr, frame_ptr, &vm->process))
    {
        // It paniced.
        vm->process.vm = nullptr;
        return false;
    }

    // Pop output values from the value stack.
    vstack_ptr = vm->vstack_base - sig.output_count;
    for (auto i = 0u; i != sig.output_count; ++i)
    {
        output[sig.output_count - i - 1] = vstack_ptr[0];
        ++vstack_ptr;
    }

    return true;
}

bool lauf_vm_execute_oneshot(lauf_vm* vm, lauf_asm_program* program,
                             const lauf_runtime_value* input, lauf_runtime_value* output)
{
    auto result = lauf_vm_execute(vm, program, input, output);
    lauf_asm_destroy_program(program);
    return result;
}

