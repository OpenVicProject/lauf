// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_CONFIG_H_INCLUDED
#define LAUF_CONFIG_H_INCLUDED

//=== basic definitions ===//
#ifdef __cplusplus

#    include <climits>
#    include <cstddef>
#    include <cstdint>

#    define LAUF_HEADER_START                                                                      \
        extern "C"                                                                                 \
        {
#    define LAUF_HEADER_END }

#else

#    include <limits.h>
#    include <stdbool.h>
#    include <stddef.h>
#    include <stdint.h>

#    define LAUF_HEADER_START
#    define LAUF_HEADER_END

#endif

typedef int64_t  lauf_sint;
typedef uint64_t lauf_uint;

//=== compatibility checks ===//
#if CHAR_BIT != 8
#    error "lauf assumes 8 bit bytes"
#endif

//=== optimizations ===//
#if defined(__GNUC__) || defined(__GNUG__)
#    define LAUF_LIKELY(Cond) __builtin_expect((Cond), 1)
#    define LAUF_UNLIKELY(Cond) __builtin_expect((Cond), 0)
#    if defined(__has_cpp_attribute)
#        if __has_cpp_attribute(clang::musttail)
#            define LAUF_TAIL_CALL [[clang::musttail]]
#            ifndef LAUF_HAS_TAIL_CALL_ELIMINATION
#                define LAUF_HAS_TAIL_CALL_ELIMINATION 1
#            endif
#        elif defined(__clang__)
#            define LAUF_TAIL_CALL [[clang::musttail]]
#        else
#            define LAUF_TAIL_CALL
#        endif
#    endif
#    define LAUF_NOINLINE [[gnu::noinline]]
#    define LAUF_FORCE_INLINE [[gnu::always_inline]] inline
#    define LAUF_UNREACHABLE __builtin_unreachable()
#elif defined(_MSC_VER)
#    define LAUF_LIKELY(Cond) (Cond)
#    define LAUF_UNLIKELY(Cond) (Cond)
#    define LAUF_TAIL_CALL
#    define LAUF_NOINLINE __declspec(noinline)
#    define LAUF_FORCE_INLINE __forceinline
#    define LAUF_UNREACHABLE __assume(0)
#endif

//=== configurations ===//
#ifndef LAUF_CONFIG_DISPATCH_JUMP_TABLE
#    define LAUF_CONFIG_DISPATCH_JUMP_TABLE 1
#endif

#ifndef LAUF_HAS_TAIL_CALL_ELIMINATION
#    define LAUF_HAS_TAIL_CALL_ELIMINATION 0
#endif

//=== warnings ===//
#if !defined(__clang__) && (defined(__GNUC__) || defined(__GNUG__))
#    define LAUF_BITFIELD_CONVERSION(...)                                                          \
        _Pragma("GCC diagnostic push");                                                            \
        _Pragma("GCC diagnostic ignored \"-Wconversion\"");                                        \
        __VA_ARGS__;                                                                               \
        _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
#    define LAUF_BITFIELD_CONVERSION(...)                                                          \
        _Pragma("warning(push)");                                                                  \
        _Pragma("warning(disable : 4267)");                                                        \
        __VA_ARGS__;                                                                               \
        _Pragma("warning(pop)")
#else
#    define LAUF_BITFIELD_CONVERSION(...) __VA_ARGS__
#endif

#endif // LAUF_CONFIG_H_INCLUDED
