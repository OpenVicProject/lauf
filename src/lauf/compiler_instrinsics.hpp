// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_COMPILER_INTRINSICS_HPP_INCLUDED
#define SRC_LAUF_COMPILER_INTRINSICS_HPP_INCLUDED

#include <lauf/config.h>

#if !defined(_MSC_VER)
template <typename IntT, typename T>
LAUF_FORCE_INLINE static bool lauf_add_overflow(IntT a, IntT b, T* out) noexcept
{
    return __builtin_add_overflow(a, b, out);
}

template <typename IntT, typename T>
LAUF_FORCE_INLINE static bool lauf_sub_overflow(IntT a, IntT b, T* out) noexcept
{
    return __builtin_sub_overflow(a, b, out);
}

template <typename IntT, typename T>
LAUF_FORCE_INLINE static bool lauf_mul_overflow(IntT a, IntT b, T* out) noexcept
{
    return __builtin_mul_overflow(a, b, out);
}

template <typename T>
LAUF_FORCE_INLINE static int lauf_countr_zero(T a) noexcept
{
    return __builtin_ctzll(a);
}

template <class T>
LAUF_FORCE_INLINE static constexpr int lauf_countr_zero_constexpr(const T val) noexcept
{
    return __builtin_ctzll(val);
}

#else
#    include <intrin.h>
#    include <type_traits>

template <typename IntT, typename T>
LAUF_FORCE_INLINE static bool lauf_add_overflow(IntT a, IntT b, T* out)
{
#    if defined(_M_IX86) || defined(_M_X64)
    if constexpr (std::is_unsigned_v<IntT>)
    {
        return _addcarry_u64(0, a, b, out);
    }
    else
#    endif
    {
        *out = a + b;
        return ((a ^ *out) & (b ^ *out)) < 0;
    }
}

template <typename IntT, typename T>
LAUF_FORCE_INLINE static bool lauf_sub_overflow(IntT a, IntT b, T* out)
{
#    if defined(_M_IX86) || defined(_M_X64)
    if constexpr (std::is_unsigned_v<IntT>)
    {
        return _subborrow_u64(0, a, b, out);
    }
    else
#    endif
    {
        *out = a - b;
        return ((a ^ b) < 0) && ((a ^ *out) < 0);
    }
}

extern "C"
{
    extern int __isa_available;
}

template <class T>
constexpr int lauf_digits = sizeof(T) * CHAR_BIT;

template <class T>
constexpr int lauf_countl_zero_fallback(T val) noexcept
{
    T yx = 0;

    unsigned int nx = lauf_digits<T>;
    unsigned int cx = lauf_digits<T> / 2;
    do
    {
        yx = static_cast<T>(val >> cx);
        if (yx != 0)
        {
            nx -= cx;
            val = yx;
        }
        cx >>= 1;
    } while (cx != 0);
    return static_cast<int>(nx) - static_cast<int>(val);
}

#    if (defined(_M_IX86) && !defined(_M_HYBRID_X86_ARM64))                                        \
        || (defined(_M_X64) && !defined(_M_ARM64EC))
template <class T>
int lauf_countl_zero_lzcnt(const T val) noexcept
{
    constexpr int digits = lauf_digits<T>;

    if constexpr (digits <= 16)
    {
        return static_cast<int>(__lzcnt16(val) - (16 - digits));
    }
    else if constexpr (digits == 32)
    {
        return static_cast<int>(__lzcnt(val));
    }
    else
    {
#        ifdef _M_IX86
        const unsigned int high = val >> 32;
        const auto         low  = static_cast<unsigned int>(val);
        if (high == 0)
        {
            return 32 + lauf_countl_zero_lzcnt(low);
        }
        else
        {
            return lauf_countl_zero_lzcnt(high);
        }
#        else  // ^^^ defined(_M_IX86) / !defined(_M_IX86) vvv
        return static_cast<int>(__lzcnt64(val));
#        endif // ^^^ !defined(_M_IX86) ^^^
    }
}

template <class T>
int lauf_countl_zero_bsr(const T val) noexcept
{
    constexpr int digits = lauf_digits<T>;

    unsigned long result;
    if constexpr (digits <= 32)
    {
        if (!_BitScanReverse(&result, val))
        {
            return digits;
        }
    }
    else
    {
#        ifdef _M_IX86
        const unsigned int high = val >> 32;
        if (_BitScanReverse(&result, high))
        {
            return static_cast<int>(31 - result);
        }

        const auto low = static_cast<unsigned int>(val);
        if (!_BitScanReverse(&result, low))
        {
            return digits;
        }
#        else  // ^^^ defined(_M_IX86) / !defined(_M_IX86) vvv
        if (!_BitScanReverse64(&result, val))
        {
            return digits;
        }
#        endif // ^^^ !defined(_M_IX86) ^^^
    }
    return static_cast<int>(digits - 1 - result);
}

template <class T>
int lauf_checked_x86_x64_countl_zero(const T val) noexcept
{
#        ifdef __AVX2__
    return lauf_countl_zero_lzcnt(val);
#        else  // ^^^ defined(__AVX2__) / !defined(__AVX2__) vvv
    constexpr int _isa_available_avx2 = 5;
    const bool    has_lzcnt           = __isa_available >= _isa_available_avx2;
    if (has_lzcnt)
    {
        return lauf_countl_zero_lzcnt(val);
    }
    else
    {
        return lauf_countl_zero_bsr(val);
    }
#        endif // ^^^ !defined(__AVX2__) ^^^
}
#    endif     // (defined(_M_IX86) && !defined(_M_HYBRID_X86_ARM64)) || (defined(_M_X64) &&
               // !defined(_M_ARM64EC))

#    if defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC) || defined(_M_HYBRID_X86_ARM64)
template <class T>
int lauf_checked_arm_arm64_countl_zero(const T val) noexcept
{
    constexpr int digits = lauf_digits<T>;
    if (val == 0)
    {
        return digits;
    }

    if constexpr (digits <= 32)
    {
        return static_cast<int>(_CountLeadingZeros(val)) - (lauf_digits<unsigned long> - digits);
    }
    else
    {
        return static_cast<int>(_CountLeadingZeros64(val));
    }
}
#    endif // defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC) ||
           // defined(_M_HYBRID_X86_ARM64)

template <typename T>
constexpr int lauf_countl_zero(const T val) noexcept
{
#    if (defined(_M_IX86) && !defined(_M_HYBRID_X86_ARM64))                                        \
        || (defined(_M_X64) && !defined(_M_ARM64EC))
    return lauf_checked_x86_x64_countl_zero(val);
#    elif defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC)                              \
        || defined(_M_HYBRID_X86_ARM64)
    return lauf_checked_arm_arm64_countl_zero(val);
#    endif // defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC) ||
           // defined(_M_HYBRID_X86_ARM64)

    return lauf_countl_zero_fallback(val);
}

template <typename IntT, typename T>
LAUF_FORCE_INLINE static bool lauf_mul_overflow(IntT a, IntT b, T* out)
{
    *out = a * b;
    // This test isnt exact, but avoids doing integer division
    return ((lauf_countl_zero(a) + lauf_countl_zero(b)) < 64);
}

template <class T>
constexpr int lauf_countr_zero_fallback(const T val) noexcept
{
    constexpr int digits = lauf_digits<T>;
    return digits
           - lauf_countl_zero_fallback(
               static_cast<T>(static_cast<T>(~val) & static_cast<T>(val - 1)));
}

#    if ((defined(_M_IX86) && !defined(_M_HYBRID_X86_ARM64))                                       \
         || (defined(_M_X64) && !defined(_M_ARM64EC)))                                             \
        && !defined(_M_CEE_PURE) && !defined(__CUDACC__)
#        define _LAUF_HAS_TZCNT_BSF_INTRINSICS 1
#    else // ^^^ intrinsics available / intrinsics unavailable vvv
#        define _LAUF_HAS_TZCNT_BSF_INTRINSICS 0
#    endif // ^^^ intrinsics unavailable ^^^

#    if _LAUF_HAS_TZCNT_BSF_INTRINSICS
template <class T>
int lauf_countr_zero_tzcnt(const T val) noexcept
{
    constexpr int digits = lauf_digits<T>;
    constexpr T   max    = static_cast<T>(-1); // equal to (numeric_limits<T>::max)()

    if constexpr (digits <= 32)
    {
        // Intended widening to int. This operation means that a narrow 0 will widen
        // to 0xFFFF....FFFF0... instead of 0. We need this to avoid counting all the zeros
        // of the wider type.
        return static_cast<int>(_tzcnt_u32(static_cast<unsigned int>(~max | val)));
    }
    else
    {
#        ifdef _M_IX86
        const auto low = static_cast<unsigned int>(val);
        if (low == 0)
        {
            const unsigned int high = val >> 32;
            return static_cast<int>(32 + _tzcnt_u32(high));
        }
        else
        {
            return static_cast<int>(_tzcnt_u32(low));
        }
#        else  // ^^^ defined(_M_IX86) / !defined(_M_IX86) vvv
        return static_cast<int>(_tzcnt_u64(val));
#        endif // ^^^ !defined(_M_IX86) ^^^
    }
}

template <class T>
int lauf_countr_zero_bsf(const T val) noexcept
{
    constexpr int digits = lauf_digits<T>;
    constexpr T   max    = static_cast<T>(-1); // equal to (numeric_limits<T>::max)()

    unsigned long result;
    if constexpr (digits <= 32)
    {
        // Intended widening to int. This operation means that a narrow 0 will widen
        // to 0xFFFF....FFFF0... instead of 0. We need this to avoid counting all the zeros
        // of the wider type.
        if (!_BitScanForward(&result, static_cast<unsigned int>(~max | val)))
        {
            return digits;
        }
    }
    else
    {
#        ifdef _M_IX86
        const auto low = static_cast<unsigned int>(val);
        if (_BitScanForward(&result, low))
        {
            return static_cast<int>(result);
        }

        const unsigned int high = val >> 32;
        if (!_BitScanForward(&result, high))
        {
            return digits;
        }
        else
        {
            return static_cast<int>(result + 32);
        }
#        else  // ^^^ defined(_M_IX86) / !defined(_M_IX86) vvv
        if (!_BitScanForward64(&result, val))
        {
            return digits;
        }
#        endif // ^^^ !defined(_M_IX86) ^^^
    }
    return static_cast<int>(result);
}

template <class T>
int lauf_checked_x86_x64_countr_zero(const T val) noexcept
{
#        ifdef __AVX2__
    return lauf_countr_zero_tzcnt(val);
#        else  // ^^^ defined(__AVX2__) / !defined(__AVX2__) vvv
    constexpr int _isa_available_avx2 = 5;
    const bool    has_tzcnt           = __isa_available >= _isa_available_avx2;
    if (has_tzcnt)
    {
        return lauf_countr_zero_tzcnt(val);
    }
    else
    {
        return lauf_countr_zero_bsf(val);
    }
#        endif // ^^^ !defined(__AVX2__) ^^^
}

#    endif // _LAUF_HAS_TZCNT_BSF_INTRINSICS

template <class T>
LAUF_FORCE_INLINE static int lauf_countr_zero(const T val) noexcept
{
#    if _HAS_TZCNT_BSF_INTRINSICS
    return lauf_checked_x86_x64_countr_zero(val);
#    endif // _HAS_TZCNT_BSF_INTRINSICS
    return lauf_countr_zero_fallback(val);
}

template <class T>
LAUF_FORCE_INLINE static constexpr int lauf_countr_zero_constexpr(const T val) noexcept
{
    return lauf_countr_zero_fallback(val);
}

#endif

#endif // SRC_LAUF_COMPILER_INTRINSICS_HPP_INCLUDED