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

/* #if !defined(__clang__)
#    error "lauf currently requires clang"
#endif */

//=== optimizations ===//
#define LAUF_LIKELY(Cond) Cond   /*__builtin_expect((Cond), 1)*/
#define LAUF_UNLIKELY(Cond) Cond /*__builtin_expect((Cond), 0)*/
#if defined(__has_cpp_attribute)
#    if __has_cpp_attribute(gnu::musttail)
#        define LAUF_TAIL_CALL [[gnu::musttail]]
#    elif __has_cpp_attribute(clang::musttail)
#        define LAUF_TAIL_CALL [[clang::musttail]]
#    else
#        define LAUF_TAIL_CALL
#    endif
#elif defined(__clang__)
#    define LAUF_TAIL_CALL [[clang::musttail]]
#else
#    define LAUF_TAIL_CALL
#endif
#if defined(_MSC_VER)
#    define LAUF_NOINLINE __declspec(noinline)
#    define LAUF_FORCE_INLINE __forceinline
#    define LAUF_UNREACHABLE __assume(0)
#else
#    define LAUF_NOINLINE [[gnu::noinline]]
#    define LAUF_FORCE_INLINE [[gnu::always_inline]] inline
#    define LAUF_UNREACHABLE __builtin_unreachable()
#endif

//=== configurations ===//
#ifndef LAUF_CONFIG_DISPATCH_JUMP_TABLE
#    define LAUF_CONFIG_DISPATCH_JUMP_TABLE 1
#endif

#if !defined(__clang__) && (defined(__GNUC__) || defined(__GNUG__))
#    define LAUF_IGNORE_BITFIELD_WARNING(...)                                                      \
        _Pragma("GCC diagnostic push");                                                            \
        _Pragma("GCC diagnostic ignored \"-Wconversion\"");                                        \
        __VA_ARGS__;                                                                               \
        _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
#    define LAUF_IGNORE_BITFIELD_WARNING(...)                                                      \
        _Pragma("warning(push)");                                                                  \
        _Pragma("warning(disable : 4267)");                                                        \
        __VA_ARGS__;                                                                               \
        _Pragma("warning(pop)")
#else
#    define LAUF_IGNORE_BITFIELD_WARNING(...) __VA_ARGS__
#endif

#if !defined(__clang__) && (defined(__GNUC__) || defined(__GNUG__))
#    define LAUF_IGNORE_CONV_WARNING(...)                                                          \
        _Pragma("GCC diagnostic push");                                                            \
        _Pragma("GCC diagnostic ignored \"-Wconversion\"");                                        \
        __VA_ARGS__;                                                                               \
        _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
#    define LAUF_IGNORE_CONV_WARNING(...)                                                          \
        _Pragma("warning(push)");                                                                  \
        _Pragma("warning(disable : 4267)");                                                        \
        __VA_ARGS__;                                                                               \
        _Pragma("warning(pop)")
#else
#    define LAUF_IGNORE_CONV_WARNING(...) __VA_ARGS__
#endif

#if !defined(__clang__) && (defined(__GNUC__) || defined(__GNUG__))
#    define LAUF_IGNORE_SIGN_WARNING(...)                                                          \
        _Pragma("GCC diagnostic push");                                                            \
        _Pragma("GCC diagnostic ignored \"-Wsign-conversion\"");                                   \
        __VA_ARGS__;                                                                               \
        _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
#    define LAUF_IGNORE_SIGN_WARNING(...)                                                          \
        _Pragma("warning(push)");                                                                  \
        _Pragma("warning(disable : 4267)");                                                        \
        __VA_ARGS__;                                                                               \
        _Pragma("warning(pop)")
#else
#    define LAUF_IGNORE_SIGN_WARNING(...) __VA_ARGS__
#endif

#if defined(__GNUC__) && (__GNUC__ == 12 && __GNUC_MINOR__ < 3)
#    define LAUF_WORKAROUND_GCC_BOGUS_MEMCPY 1
#else
#    define LAUF_WORKAROUND_GCC_BOGUS_MEMCPY 0
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#    include <bit>
template <typename T>
static LAUF_FORCE_INLINE bool __builtin_add_overflow(uint64_t a, uint64_t b, T* out)
{
    unsigned long long tmp;
#    if defined(_M_IX86) || defined(_M_X64)
    auto carry = _addcarry_u64(0, a, b, &tmp);
#    else
    tmp                       = a + b;
    unsigned long long vector = (a & b) ^ ((a ^ b) & ~tmp);
    auto               carry  = vector >> 63;
#    endif
    *out = tmp;
    return carry;
}

template <typename T>
static LAUF_FORCE_INLINE bool __builtin_sub_overflow(uint64_t a, uint64_t b, T* out)
{
    unsigned long long tmp;
#    if defined(_M_IX86) || defined(_M_X64)
    auto borrow = _subborrow_u64(0, a, b, &tmp);
#    else
    tmp                       = a - b;
    unsigned long long vector = ((a ^ b) & a);
    auto               borrow = vector >> 63;
#    endif
    *out = tmp;
    return borrow;
}

template <typename T>
static LAUF_FORCE_INLINE bool __builtin_mul_overflow(uint64_t a, uint64_t b, T* out)
{
    *out = a * b;
    // This test isnt exact, but avoids doing integer division
    return ((std::countl_zero(a) + std::countl_zero(b)) < 64);
}
#endif

#endif // LAUF_CONFIG_H_INCLUDED
