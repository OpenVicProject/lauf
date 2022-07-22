// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib/test.h>

#include <lauf/asm/module.h>
#include <lauf/lib/debug.h>
#include <lauf/runtime/builtin.h>
#include <lauf/runtime/process.h>
#include <lauf/runtime/value.h>
#include <lauf/vm.hpp>

LAUF_RUNTIME_BUILTIN(lauf_lib_test_unreachable, 0, 0, LAUF_RUNTIME_BUILTIN_DEFAULT, "unreachable",
                     nullptr)
{
    (void)vstack_ptr;
    (void)frame_ptr;
    return lauf_runtime_panic(process, ip, "unreachable code reached");
}

LAUF_RUNTIME_BUILTIN(lauf_lib_test_assert, 1, 0, LAUF_RUNTIME_BUILTIN_DEFAULT, "assert",
                     &lauf_lib_test_unreachable)
{
    auto value = vstack_ptr[0].as_uint;
    ++vstack_ptr;

    if (value == 0)
        LAUF_RUNTIME_BUILTIN_DISPATCH;
    else
        return lauf_runtime_panic(process, ip, "assert failed");
}

LAUF_RUNTIME_BUILTIN(lauf_lib_test_assert_eq, 2, 0, LAUF_RUNTIME_BUILTIN_DEFAULT, "assert_eq",
                     &lauf_lib_test_assert)
{
    auto lhs = vstack_ptr[1].as_uint;
    auto rhs = vstack_ptr[0].as_uint;
    vstack_ptr += 2;

    if (lhs == rhs)
        LAUF_RUNTIME_BUILTIN_DISPATCH;
    else
        return lauf_runtime_panic(process, ip, "assert_eq failed");
}

LAUF_RUNTIME_BUILTIN(lauf_lib_test_assert_panic, 2, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY, "assert_panic",
                     &lauf_lib_test_assert_eq)
{
    auto expected_msg = lauf_runtime_get_cstr(process, vstack_ptr[0].as_address);
    auto fn = lauf_runtime_get_function_ptr(process, vstack_ptr[1].as_function_address, {0, 0});
    vstack_ptr += 2;

    if (fn == nullptr)
        return lauf_runtime_panic(process, ip, "invalid function");

    // We temporarily replace the panic handler with one that simply remembers the message.
    auto                            handler = process->vm->panic_handler;
    static thread_local const char* panic_msg;
    process->vm->panic_handler = [](lauf_runtime_process*, const char* msg) { panic_msg = msg; };

    auto did_not_panic = lauf_runtime_call(process, fn, vstack_ptr);

    process->vm->panic_handler = handler;

    if (did_not_panic)
        return lauf_runtime_panic(process, ip, "assert_panic failed: no panic");
    else if (expected_msg == nullptr && panic_msg != nullptr)
        return lauf_runtime_panic(process, ip, "assert_panic failed: did not expect message");
    else if (expected_msg != nullptr && std::strcmp(expected_msg, panic_msg) != 0)
        return lauf_runtime_panic(process, ip, "assert_panic failed: different message");
    else
        LAUF_RUNTIME_BUILTIN_DISPATCH;
}

const lauf_runtime_builtin_library lauf_lib_test = {"lauf.test", &lauf_lib_test_assert_panic};

