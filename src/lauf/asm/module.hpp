// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_ASM_MODULE_HPP_INCLUDED
#define SRC_LAUF_ASM_MODULE_HPP_INCLUDED

#include <lauf/asm/instruction.hpp>
#include <lauf/asm/module.h>
#include <lauf/support/arena.hpp>
#include <lauf/support/array_list.hpp>

namespace lauf
{
struct inst_debug_location
{
    std::uint16_t           function_idx;
    std::uint16_t           inst_idx;
    lauf_asm_debug_location location;
};
} // namespace lauf

struct lauf_asm_module : lauf::intrinsic_arena<lauf_asm_module>
{
    const char*        name;
    lauf_asm_global*   globals         = nullptr;
    lauf_asm_function* functions       = nullptr;
    std::uint32_t      globals_count   = 0;
    std::uint32_t      functions_count = 0;

    const char*                                 debug_path = nullptr;
    lauf::array_list<lauf::inst_debug_location> inst_debug_locations;

    lauf_asm_module(lauf::arena_key key, const char* name)
    : lauf::intrinsic_arena<lauf_asm_module>(key), name(this->strdup(name))
    {}
};

struct lauf_asm_global
{
    lauf_asm_global* next;

    const unsigned char* memory;
    std::uint64_t        size : 64;

    std::uint32_t allocation_idx;
    std::uint16_t alignment;
    enum permissions : std::uint8_t
    {
        read_only,
        read_write,
    } perms;

    const char* name = nullptr;

    explicit lauf_asm_global(lauf_asm_module* mod)
    : next(mod->globals), memory(nullptr), size(0), allocation_idx(mod->globals_count),
      alignment(alignof(lauf_uint)), perms(read_only)
    {
        mod->globals = this;
        ++mod->globals_count;
    }

    explicit lauf_asm_global(lauf_asm_module* mod, std::size_t size, std::size_t alignment)
    : lauf_asm_global(mod)
    {
        this->size      = size;
        this->alignment = std::uint16_t(alignment);
        this->perms     = read_write;
    }

    explicit lauf_asm_global(lauf_asm_module* mod, const void* memory, std::size_t size,
                             std::size_t alignment, permissions p)
    : lauf_asm_global(mod)
    {
        this->memory    = mod->memdup(memory, size);
        this->size      = size;
        this->alignment = std::uint16_t(alignment);
        this->perms     = p;
    }
};

struct lauf_asm_function
{
    lauf_asm_function* next;

    const char*        name;
    lauf_asm_signature sig;
    bool               exported = false;

    lauf_asm_inst* insts       = nullptr;
    std::uint16_t  insts_count = 0;
    std::uint16_t  function_idx;
    std::uint16_t  max_vstack_size = 0;
    // Includes size for stack frame as well.
    std::uint16_t max_cstack_size = 0;

    explicit lauf_asm_function(lauf_asm_module* mod, const char* name, lauf_asm_signature sig)
    : next(mod->functions), name(mod->strdup(name)), sig(sig),
      function_idx(std::uint16_t(mod->functions_count))
    {
        mod->functions = this;
        ++mod->functions_count;
    }
};

#endif // SRC_LAUF_ASM_MODULE_HPP_INCLUDED

