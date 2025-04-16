// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_ASM_INSTRUCTION_HPP_INCLUDED
#define SRC_LAUF_ASM_INSTRUCTION_HPP_INCLUDED

#include <lauf/config.h>

#include <cassert>
#include <lauf/support/align.hpp>
#include <type_traits>

// The ASM instructions are also the bytecode for the VM.
// As such, there are many specializations and optimizations.
// It is also not designed to support edits; use the IR for that.

namespace lauf
{
enum class asm_op : std::uint8_t
{
#define LAUF_ASM_INST(Name, Type) Name,
#include "instruction.def.hpp"
#undef LAUF_ASM_INST
    count,
};

constexpr const char* to_string(asm_op op)
{
    switch (op)
    {
#define LAUF_ASM_INST(Name, Type)                                                                  \
case asm_op::Name:                                                                                 \
    return #Name;
#include "instruction.def.hpp"
#undef LAUF_ASM_INST

    case asm_op::count:
        assert(false);
        return nullptr;
    }
}
} // namespace lauf

namespace lauf
{
struct asm_inst_none
{
    asm_op op;
};

#ifdef _MSC_VER
struct SignedTrait
{
    using bit8_t          = std::int8_t;
    using bit16_t         = std::int16_t;
    using bit32_t         = std::int32_t;
    using inverse_bit32_t = std::uint32_t;
};
struct UnsignedTrait
{
    using bit8_t          = std::uint8_t;
    using bit16_t         = std::uint16_t;
    using bit32_t         = std::uint32_t;
    using inverse_bit32_t = std::int32_t;
};
// MSVC tends not to respect bit-fields for packing
#    pragma pack(push, 1)
template <typename SignTrait>
struct _24bitint
{
    typename SignTrait::bit8_t  value_b1 : 8;
    typename SignTrait::bit16_t value_b2 : 16;

    LAUF_FORCE_INLINE constexpr _24bitint() = default;
    LAUF_FORCE_INLINE constexpr _24bitint(typename SignTrait::bit32_t value)
    {
        *this = value;
    }

    struct int_converter
    {
        typename SignTrait::bit8_t  value_b1{};
        typename SignTrait::bit16_t value_b2{};
        typename SignTrait::bit8_t  value_b3{};
    };

    LAUF_FORCE_INLINE constexpr _24bitint& operator=(typename SignTrait::bit32_t lhs)
    {
        int_converter converter = __builtin_bit_cast(decltype(converter), lhs);
        value_b1                = converter.value_b1;
        value_b2                = converter.value_b2;
        return *this;
    }

    LAUF_FORCE_INLINE constexpr operator typename SignTrait::bit32_t() const
    {
        int_converter converter{};
        converter.value_b1 = value_b1;
        converter.value_b2 = value_b2;
        if constexpr (!std::is_unsigned_v<typename SignTrait::bit8_t>)
        {
            converter.value_b3 = value_b2 >> 15;
        }
        return __builtin_bit_cast(typename SignTrait::bit32_t, converter);
    }

    LAUF_FORCE_INLINE constexpr explicit operator typename SignTrait::inverse_bit32_t() const
    {
        int_converter converter{};
        converter.value_b1 = value_b1;
        converter.value_b2 = value_b2;
        return __builtin_bit_cast(typename SignTrait::bit32_t, converter);
    }

    template <typename T>
    LAUF_FORCE_INLINE constexpr explicit operator T() const
    {
        if constexpr (std::is_unsigned_v<T> == std::is_unsigned_v<typename SignTrait::bit32_t>)
        {
            return static_cast<T>(static_cast<typename SignTrait::bit32_t>(*this));
        }
        else
        {
            return static_cast<T>(static_cast<typename SignTrait::inverse_bit32_t>(*this));
        }
    }

    LAUF_FORCE_INLINE constexpr _24bitint operator+() const
    {
        return *this;
    }

    LAUF_FORCE_INLINE constexpr _24bitint operator-() const
    {
        return _24bitint{-static_cast<typename SignTrait::bit32_t>(*this)};
    }

    LAUF_FORCE_INLINE constexpr _24bitint operator~() const
    {
        return _24bitint{~static_cast<typename SignTrait::bit32_t>(*this)};
    }

    LAUF_FORCE_INLINE _24bitint& operator++()
    {
        return *this = *this + 1;
    }

    LAUF_FORCE_INLINE _24bitint operator++(int)
    {
        _24bitint result{*this};
        ++(*this);
        return result;
    }

    LAUF_FORCE_INLINE _24bitint& operator--()
    {
        return *this = *this - 1;
    }

    LAUF_FORCE_INLINE _24bitint operator--(int)
    {
        _24bitint result(*this);
        --(*this);
        return result;
    }
};
#    pragma pack(pop)
using int24_t  = _24bitint<SignedTrait>;
using uint24_t = _24bitint<UnsignedTrait>;
#endif

struct asm_inst_offset
{
    asm_op op : 8;
#ifdef _MSC_VER
    lauf::int24_t offset;
#else
    std::int32_t offset : 24;
#endif
};

template <typename CurType, typename DestType>
std::ptrdiff_t compress_pointer_offset(CurType* _cur, DestType* _dest)
{
    auto cur  = (char*)(_cur);
    auto dest = (char*)(_dest);
    return _dest ? dest - cur : 0;
}

template <typename DestType, typename CurType>
const DestType* uncompress_pointer_offset(CurType* cur, std::ptrdiff_t offset)
{
    return (DestType*)(reinterpret_cast<const char*>(cur) + offset);
}

struct asm_inst_signature
{
    asm_op       op;
    std::uint8_t input_count;
    std::uint8_t output_count;
    std::uint8_t flags;
};

struct asm_inst_layout
{
    asm_op        op;
    std::uint8_t  alignment_log2;
    std::uint16_t size;

    constexpr std::size_t alignment() const
    {
        return std::size_t(1) << alignment_log2;
    }
};

struct asm_inst_value
{
    asm_op op : 8;
#ifdef _MSC_VER
    lauf::uint24_t value;
#else
    std::uint32_t value : 24;
#endif
};

struct asm_inst_stack_idx
{
    asm_op        op;
    std::uint16_t idx;
};

struct asm_inst_local_addr
{
    asm_op        op;
    std::uint8_t  index;
    std::uint16_t offset;
};
} // namespace lauf

union lauf_asm_inst
{
#define LAUF_ASM_INST(Name, Type) lauf::Type Name;
#include "instruction.def.hpp"
#undef LAUF_ASM_INST

    constexpr lauf_asm_inst() : nop{lauf::asm_op::nop}
    {
        static_assert(sizeof(lauf_asm_inst) == sizeof(std::uint32_t));
    }

    constexpr lauf::asm_op op() const
    {
        return nop.op;
    }
};

#endif // SRC_LAUF_ASM_INSTRUCTION_HPP_INCLUDED
