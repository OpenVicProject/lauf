// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/backend/dump.h>

#include <cctype>
#include <lauf/asm/module.hpp>
#include <lauf/runtime/builtin.h>
#include <lauf/writer.hpp>

const lauf_backend_dump_options lauf_backend_default_dump_options = {nullptr, 0};

namespace
{
void dump_global(lauf_writer* writer, lauf_backend_dump_options, const lauf_asm_global* global)
{
    writer->write("global ");
    if (global->perms == lauf_asm_global::read_only)
        writer->write("const ");
    else
        writer->write("mut ");

    writer->format("@global_%u = ", global->allocation_idx);

    if (global->memory == nullptr)
    {
        writer->format("[00] * %zu", std::size_t(global->size));
    }
    else
    {
        writer->write("[");
        for (auto i = 0u; i != global->size; ++i)
        {
            if (i > 0)
                writer->write(",");

            writer->format("%02X", global->memory[i]);
        }
        writer->write("]");
    }

    writer->write(";\n");
}

const lauf_runtime_builtin_function* find_builtin(lauf_backend_dump_options          opts,
                                                  lauf_runtime_builtin_function_impl impl)
{
    for (auto i = 0u; i != opts.builtin_libs_count; ++i)
        for (auto builtin = opts.builtin_libs[i]; builtin != nullptr; builtin = builtin->next)
            if (builtin->impl == impl)
                return builtin;

    return nullptr;
}

void dump_function(lauf_writer* writer, lauf_backend_dump_options opts, const lauf_asm_function* fn)
{
    writer->format("function @'%s'(%d => %d)", fn->name, fn->sig.input_count, fn->sig.output_count);
    if (fn->insts == nullptr)
    {
        writer->write(";\n");
        return;
    }

    writer->write("\n{\n");

    for (auto ip = fn->insts; ip != fn->insts + fn->insts_count; ++ip)
    {
        writer->format("  <%04zx>: ", ip - fn->insts);
        switch (ip->op())
        {
        case lauf::asm_op::data:
            assert(false);
            break;
        case lauf::asm_op::nop:
            writer->write("nop");
            break;
        case lauf::asm_op::return_:
            writer->write("return");
            break;
        case lauf::asm_op::jump:
            writer->format("jump <%04zx>", ip + ip->jump.offset - fn->insts);
            break;
        case lauf::asm_op::branch_false:
            writer->format("branch.false <%04zx>", ip + ip->branch_false.offset - fn->insts);
            break;
        case lauf::asm_op::branch_eq:
            writer->format("branch.eq <%04zx>", ip + ip->branch_eq.offset - fn->insts);
            break;
        case lauf::asm_op::branch_gt:
            writer->format("branch.gt <%04zx>", ip + ip->branch_gt.offset - fn->insts);
            break;
        case lauf::asm_op::panic:
            writer->write("panic");
            break;
        case lauf::asm_op::exit:
            writer->write("exit");
            break;

        case lauf::asm_op::call: {
            auto callee = lauf::uncompress_pointer_offset<lauf_asm_function>(fn, ip->call.offset);
            writer->format("call @'%s'", callee->name);
            break;
        }
        case lauf::asm_op::call_builtin:
        case lauf::asm_op::call_builtin_no_panic:
        case lauf::asm_op::call_builtin_no_process: {
            auto data   = lauf::read_call_builtin_data(ip);
            auto callee = reinterpret_cast<lauf_runtime_builtin_function_impl*>(data); // NOLINT
            if (auto builtin = find_builtin(opts, callee))
                writer->format("$'%s'", builtin->name);
            else
                writer->format("$'%p'", reinterpret_cast<void*>(callee));
            ip += 2;
            break;
        }

        case lauf::asm_op::push:
            writer->format("push 0x%X", ip->push.value);
            break;
        case lauf::asm_op::push2:
            writer->format("push2 0x%X", ip->push2.value);
            break;
        case lauf::asm_op::push3:
            writer->format("push3 0x%X", ip->push3.value);
            break;
        case lauf::asm_op::pushn:
            writer->format("pushn 0x%X", ip->pushn.value);
            break;
        case lauf::asm_op::global_addr: {
            writer->format("global_addr @global_%u", ip->global_addr.value);
            break;
        }

        case lauf::asm_op::pop:
            writer->format("pop %d", ip->pop.idx);
            break;
        case lauf::asm_op::pick:
            writer->format("pick %d", ip->pick.idx);
            break;
        case lauf::asm_op::roll:
            writer->format("roll %d", ip->roll.idx);
            break;
        }
        writer->write(";\n");
    }

    writer->write("}\n");
}
} // namespace

void lauf_backend_dump(lauf_writer* writer, lauf_backend_dump_options options,
                       const lauf_asm_module* mod)
{
    writer->format("module @'%s';\n", mod->name);
    writer->write("\n");

    for (auto global = mod->globals; global != nullptr; global = global->next)
        dump_global(writer, options, global);
    writer->write("\n");

    for (auto function = mod->functions; function != nullptr; function = function->next)
    {
        dump_function(writer, options, function);
        writer->write("\n");
    }
}

