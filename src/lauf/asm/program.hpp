// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_ASM_PROGRAM_HPP_INCLUDED
#define SRC_LAUF_ASM_PROGRAM_HPP_INCLUDED

#include <lauf/asm/program.h>
#include <lauf/support/arena.hpp>
#include <lauf/support/array_list.hpp>

namespace lauf
{
struct native_function_definition
{
    const lauf_asm_function* fn_decl;
    lauf_asm_native_function native_fn;
    void*                    user_data;
};

struct native_global_definition
{
    const lauf_asm_global* global_decl;
    void*                  ptr;
    size_t                 size;
};

struct program_extra_data : lauf::intrinsic_arena<program_extra_data>
{
    lauf::array_list<native_function_definition> fn_defs;
    lauf::array_list<native_global_definition>   global_defs;

    program_extra_data(lauf::arena_key key) : lauf::intrinsic_arena<program_extra_data>(key) {}

    void add_definition(native_function_definition fn_def)
    {
        fn_defs.push_back(*this, fn_def);
    }
    void add_definition(native_global_definition global_def)
    {
        global_defs.push_back(*this, global_def);
    }

    const native_function_definition* find_definition(const lauf_asm_function* fn) const
    {
        for (auto& def : fn_defs)
            if (def.fn_decl == fn)
                return &def;
        return nullptr;
    }
    const native_global_definition* find_definition(const lauf_asm_global* global) const
    {
        for (auto& def : global_defs)
            if (def.global_decl == global)
                return &def;
        return nullptr;
    }
};

inline lauf::program_extra_data* try_get_extra_data(lauf_asm_program program)
{
    return static_cast<lauf::program_extra_data*>(program._extra_data);
}

inline lauf::program_extra_data& get_extra_data(lauf_asm_program* program)
{
    if (program->_extra_data == nullptr)
        program->_extra_data = lauf::program_extra_data::create();

    return *static_cast<lauf::program_extra_data*>(program->_extra_data);
}
} // namespace lauf

#endif // SRC_LAUF_ASM_PROGRAM_HPP_INCLUDED

