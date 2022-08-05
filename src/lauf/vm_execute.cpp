// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/vm.hpp>

#include <cassert>
#include <lauf/asm/module.hpp>
#include <lauf/asm/type.h>
#include <lauf/runtime/builtin.h>

//=== execute ===//
#define LAUF_VM_DISPATCH                                                                           \
    [[clang::musttail]] return dispatch[std::size_t(ip->op())](ip, vstack_ptr, frame_ptr, process)

#define LAUF_VM_EXECUTE(Name)                                                                      \
    static bool execute_##Name(const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr,            \
                               lauf_runtime_stack_frame* frame_ptr, lauf_runtime_process* process)

#define LAUF_ASM_INST(Name, Type)                                                                  \
    static bool execute_##Name(const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr,            \
                               lauf_runtime_stack_frame* frame_ptr,                                \
                               lauf_runtime_process*     process);
#include <lauf/asm/instruction.def.hpp>
#undef LAUF_ASM_INST

namespace
{
using dispatch_fn = bool (*)(const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr,
                             lauf_runtime_stack_frame* frame_ptr, lauf_runtime_process* process);

constexpr dispatch_fn dispatch[] = {
#define LAUF_ASM_INST(Name, Type) &execute_##Name,
#include <lauf/asm/instruction.def.hpp>
#undef LAUF_ASM_INST
};
} // namespace

bool lauf::execute(const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr,
                   lauf_runtime_stack_frame* frame_ptr, lauf_runtime_process* process)
{
    LAUF_VM_DISPATCH;
}

bool lauf_runtime_builtin_dispatch(const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr,
                                   lauf_runtime_stack_frame* frame_ptr,
                                   lauf_runtime_process*     process)
{
    if (ip == nullptr)
        return true;

    ++ip;
    LAUF_VM_DISPATCH;
}

//=== helper functions ===//
namespace
{
bool do_panic(const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr,
              lauf_runtime_stack_frame* frame_ptr, lauf_runtime_process* process)
{
    auto msg = reinterpret_cast<const char*>(vstack_ptr);
    process->callstack_leaf_frame.assign_callstack_leaf_frame(ip, frame_ptr);
    return lauf_runtime_panic(process, msg);
}
#define LAUF_DO_PANIC(Msg)                                                                         \
    [[clang::musttail]] return do_panic(ip, (lauf_runtime_value*)(Msg), frame_ptr, process)

lauf::allocation make_local_alloc(void* memory, std::size_t size, std::uint8_t generation)
{
    lauf::allocation alloc;
    alloc.ptr        = memory;
    alloc.size       = std::uint32_t(size);
    alloc.source     = lauf::allocation_source::local_memory;
    alloc.status     = lauf::allocation_status::allocated;
    alloc.generation = generation;
    return alloc;
}

// We move the expensive call into a separate function, so the hot path contains no function calls.
[[gnu::noinline]] bool grow_allocation_array(const lauf_asm_inst*      ip,
                                             lauf_runtime_value*       vstack_ptr,
                                             lauf_runtime_stack_frame* frame_ptr,
                                             lauf_runtime_process*     process)
{
    process->allocations.reserve(*process->vm, process->allocations.size() + 1);
    LAUF_VM_DISPATCH;
}
} // namespace

//=== control flow ===//
LAUF_VM_EXECUTE(nop)
{
    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(return_)
{
    ip        = frame_ptr->return_ip;
    frame_ptr = frame_ptr->prev;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(jump)
{
    ip += ip->jump.offset;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(branch_false)
{
    auto condition = vstack_ptr[0].as_uint;
    ++vstack_ptr;

    if (condition == 0)
        ip += ip->jump.offset;
    else
        ++ip;

    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(branch_eq)
{
    auto condition = vstack_ptr[0].as_sint;

    if (condition == 0)
    {
        ip += ip->branch_eq.offset;
        ++vstack_ptr;
    }
    else
        ++ip;

    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(branch_gt)
{
    auto condition = vstack_ptr[0].as_sint;
    ++vstack_ptr;

    if (condition > 0)
        ip += ip->branch_gt.offset;
    else
        ++ip;

    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(panic)
{
    auto msg = lauf_runtime_get_cstr(process, vstack_ptr[0].as_address);
    ++vstack_ptr;

    LAUF_DO_PANIC(msg);
}

LAUF_VM_EXECUTE(exit)
{
    (void)ip;
    (void)frame_ptr;
    (void)vstack_ptr;
    (void)process;
    return true;
}

//=== calls ===//
LAUF_VM_EXECUTE(call_builtin)
{
    process->callstack_leaf_frame.assign_callstack_leaf_frame(ip, frame_ptr);
    [[clang::musttail]] return execute_call_builtin_no_process(ip, vstack_ptr, frame_ptr, process);
}

LAUF_VM_EXECUTE(call_builtin_no_process)
{
    auto callee
        = lauf::uncompress_pointer_offset<lauf_runtime_builtin_impl>(&lauf_runtime_builtin_dispatch,
                                                                     ip->call_builtin.offset);

    [[clang::musttail]] return callee(ip, vstack_ptr, frame_ptr, process);
}

LAUF_VM_EXECUTE(call)
{
    auto callee
        = lauf::uncompress_pointer_offset<lauf_asm_function>(frame_ptr->function, ip->call.offset);

    // Check that we have enough space left on the vstack.
    if (auto remaining = vstack_ptr - process->vstack_end; remaining < callee->max_vstack_size)
        LAUF_DO_PANIC("vstack overflow");

    // Check that we have enough space left on the cstack.
    auto next_frame  = frame_ptr->next_frame();
    auto size_needed = sizeof(lauf_runtime_stack_frame) + callee->max_cstack_size;
    auto size_remaining
        = std::size_t(process->cstack_end - static_cast<unsigned char*>(next_frame));
    if (size_needed > size_remaining)
        LAUF_DO_PANIC("cstack overflow");

    // Create a new stack frame.
    frame_ptr = ::new (next_frame) auto(
        lauf_runtime_stack_frame::make_call_frame(callee, process, ip, frame_ptr));

    // And start executing the function.
    ip = callee->insts;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(call_indirect)
{
    auto ptr = vstack_ptr[0].as_function_address;
    ++vstack_ptr;

    auto callee = lauf_runtime_get_function_ptr(process, ptr,
                                                {ip->call_indirect.input_count,
                                                 ip->call_indirect.output_count});
    if (callee == nullptr)
        LAUF_DO_PANIC("invalid function address");

    // Check that we have enough space left on the vstack.
    if (auto remaining = vstack_ptr - process->vstack_end; remaining < callee->max_vstack_size)
        LAUF_DO_PANIC("vstack overflow");

    // Check that we have enough space left on the cstack.
    auto next_frame  = frame_ptr->next_frame();
    auto size_needed = sizeof(lauf_runtime_stack_frame) + callee->max_cstack_size;
    auto size_remaining
        = std::size_t(process->cstack_end - static_cast<unsigned char*>(next_frame));
    if (size_needed > size_remaining)
        LAUF_DO_PANIC("cstack overflow");

    // Create a new stack frame.
    frame_ptr = ::new (next_frame) auto(
        lauf_runtime_stack_frame::make_call_frame(callee, process, ip, frame_ptr));

    // And start executing the function.
    ip = callee->insts;
    LAUF_VM_DISPATCH;
}

//=== value instructions ===//
LAUF_VM_EXECUTE(push)
{
    --vstack_ptr;
    vstack_ptr[0].as_uint = ip->push.value;

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(pushn)
{
    --vstack_ptr;
    vstack_ptr[0].as_uint = ~lauf_uint(ip->push.value);

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(push2)
{
    vstack_ptr[0].as_uint |= lauf_uint(ip->push2.value) << 24;

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(push3)
{
    vstack_ptr[0].as_uint |= lauf_uint(ip->push2.value) << 48;

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(global_addr)
{
    --vstack_ptr;
    vstack_ptr[0].as_address.allocation = ip->global_addr.value;
    vstack_ptr[0].as_address.offset     = 0;
    vstack_ptr[0].as_address.generation = 0; // Always true for globals.

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(function_addr)
{
    auto fn = lauf::uncompress_pointer_offset<lauf_asm_function>(frame_ptr->function,
                                                                 ip->function_addr.offset);

    --vstack_ptr;
    vstack_ptr[0].as_function_address.index        = fn->function_idx;
    vstack_ptr[0].as_function_address.input_count  = fn->sig.input_count;
    vstack_ptr[0].as_function_address.output_count = fn->sig.output_count;

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(local_addr)
{
    auto allocation_idx = frame_ptr->first_local_alloc + ip->local_addr.value;

    --vstack_ptr;
    vstack_ptr[0].as_address.allocation = std::uint32_t(allocation_idx);
    vstack_ptr[0].as_address.offset     = 0;
    vstack_ptr[0].as_address.generation = frame_ptr->local_generation;

    ++ip;
    LAUF_VM_DISPATCH;
}

//=== stack manipulation ===//
LAUF_VM_EXECUTE(pop)
{
    // Move everything above one over.
    std::memmove(vstack_ptr + 1, vstack_ptr, ip->pop.idx * sizeof(lauf_runtime_value));
    // Remove the now duplicate top value.
    ++vstack_ptr;

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(pop_top)
{
    assert(ip->pop_top.idx == 0);
    ++vstack_ptr;

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(pick)
{
    auto value = vstack_ptr[ip->pick.idx];
    --vstack_ptr;
    vstack_ptr[0] = value;

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(dup)
{
    assert(ip->dup.idx == 0);
    --vstack_ptr;
    vstack_ptr[0] = vstack_ptr[1];

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(roll)
{
    // Remember the value as we're about to overwrite it.
    auto value = vstack_ptr[ip->roll.idx];
    // Move everything above one over.
    std::memmove(vstack_ptr + 1, vstack_ptr, ip->pop.idx * sizeof(lauf_runtime_value));
    // Replace the now duplicate top value.
    vstack_ptr[0] = value;

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(swap)
{
    assert(ip->swap.idx == 1);
    auto tmp      = vstack_ptr[0];
    vstack_ptr[0] = vstack_ptr[1];
    vstack_ptr[1] = tmp;

    ++ip;
    LAUF_VM_DISPATCH;
}

//=== memory ===//
LAUF_VM_EXECUTE(local_alloc)
{
    // The builder has taken care of ensuring alignment.
    assert(ip->local_alloc.alignment() == alignof(void*));
    assert(lauf::is_aligned(frame_ptr->next_frame(), alignof(void*)));

    // If necessary, grow the allocation array - this will then tail call back here.
    if (process->allocations.size() == process->allocations.capacity()) [[clang::musttail]]
        return grow_allocation_array(ip, vstack_ptr, frame_ptr, process);

    auto memory = frame_ptr->next_frame();
    frame_ptr->next_offset += ip->local_alloc.size;

    process->allocations.push_back_unchecked(
        make_local_alloc(memory, ip->local_alloc.size, frame_ptr->local_generation));

    ++ip;
    LAUF_VM_DISPATCH;
}
LAUF_VM_EXECUTE(local_alloc_aligned)
{
    // If necessary, grow the allocation array - this will then tail call back here.
    if (process->allocations.size() == process->allocations.capacity()) [[clang::musttail]]
        return grow_allocation_array(ip, vstack_ptr, frame_ptr, process);

    // The builder has taken care of ensuring alignment.
    frame_ptr->next_offset
        += lauf::align_offset(frame_ptr->next_frame(), ip->local_alloc_aligned.alignment());
    auto memory = frame_ptr->next_frame();
    frame_ptr->next_offset += ip->local_alloc.size;

    process->allocations.push_back_unchecked(
        make_local_alloc(memory, ip->local_alloc.size, frame_ptr->local_generation));

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(local_free)
{
    for (auto i = 0u; i != ip->local_free.value; ++i)
    {
        auto index                         = frame_ptr->first_local_alloc + i;
        process->allocations[index].status = lauf::allocation_status::freed;
    }
    process->try_free_allocations();

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(deref_const)
{
    auto address = vstack_ptr[0].as_address;

    if (auto alloc = process->get_allocation(address.allocation))
    {
        auto ptr = lauf::checked_offset(*alloc, address,
                                        {ip->deref_const.size, ip->deref_const.alignment()});
        if (ptr != nullptr)
        {
            vstack_ptr[0].as_native_ptr = const_cast<void*>(ptr);

            ++ip;
            LAUF_VM_DISPATCH;
        }
    }

    LAUF_DO_PANIC("invalid address");
}

LAUF_VM_EXECUTE(deref_mut)
{
    auto address = vstack_ptr[0].as_address;

    if (auto alloc = process->get_allocation(address.allocation);
        alloc != nullptr && !lauf::is_const(alloc->source))
    {
        auto ptr = lauf::checked_offset(*alloc, address,
                                        {ip->deref_const.size, ip->deref_const.alignment()});
        if (ptr != nullptr)
        {
            vstack_ptr[0].as_native_ptr = const_cast<void*>(ptr);

            ++ip;
            LAUF_VM_DISPATCH;
        }
    }

    LAUF_DO_PANIC("invalid address");
}

