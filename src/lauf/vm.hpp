// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_VM_HPP_INCLUDED
#define SRC_LAUF_VM_HPP_INCLUDED

#include <lauf/vm.h>

#include <lauf/asm/instruction.hpp>
#include <lauf/runtime/process.hpp>
#include <lauf/runtime/value.h>
#include <lauf/support/arena.hpp>

struct lauf_vm : lauf::intrinsic_arena<lauf_vm>
{
    lauf_vm_panic_handler panic_handler;

    // Grows up.
    unsigned char* cstack_base;
    std::size_t    cstack_size;

    // Grows down.
    lauf_runtime_value* vstack_base;
    std::size_t         vstack_size;

    explicit lauf_vm(lauf::arena_key key, lauf_vm_options options)
    : lauf::intrinsic_arena<lauf_vm>(key), panic_handler(options.panic_handler),
      cstack_size(options.cstack_size_in_bytes), vstack_size(options.vstack_size_in_elements)
    {
        // We allocate the stacks using new, as unlike the arena, their memory should not be freed
        // between executions.
        cstack_base = static_cast<unsigned char*>(::operator new(cstack_size));

        // It grows down, so the base is at the end.
        vstack_base = static_cast<lauf_runtime_value*>(
                          ::operator new(vstack_size * sizeof(lauf_runtime_value)))
                      + vstack_size;
    }

    ~lauf_vm()
    {
        ::operator delete(cstack_base);
        ::operator delete(vstack_base - vstack_size);
    }

    lauf_runtime_value* vstack_end() const
    {
        // We keep a buffer of UINT8_MAX.
        // This ensures that we can always call a builtin, which can push at most that many values.
        return vstack_base - vstack_size + UINT8_MAX;
    }

    unsigned char* cstack_end() const
    {
        return cstack_base + cstack_size;
    }
};

#endif // SRC_LAUF_VM_HPP_INCLUDED

