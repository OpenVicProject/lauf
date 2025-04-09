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

#pragma pack(1)
struct asm_inst_offset
{
    asm_op op : 8;

    constexpr std::int32_t offset() const
    {
#if _MSC_VER
        return (static_cast<int32_t>(static_cast<int8_t>(_offset[2])) << 16) | (_offset[1] << 8)
               | _offset[0];

#else
        return _offset;
#endif
    }
    constexpr std::int32_t offset(std::int32_t value)
    {
#if _MSC_VER
        _offset[0] = static_cast<uint8_t>(value & 0xFF);
        _offset[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        _offset[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
        return value;
#else
        LAUF_BITFIELD_CONVERSION(return _offset = value);
#endif
    }

    asm_inst_offset() = default;
    constexpr asm_inst_offset(asm_op op, std::int32_t offset) : op(op), _offset{}
    {
        this->offset(offset);
    }

private:
#if _MSC_VER
    std::uint8_t _offset[3];
#else
    std::int32_t _offset : 24;
#endif
};
#pragma pack()

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

#pragma pack(1)
struct asm_inst_value
{
    asm_op op : 8;

    constexpr std::uint32_t value() const
    {
#if _MSC_VER
        if (__builtin_is_constant_evaluated())
        {
            std::uint8_t array[sizeof(std::uint32_t)]{_value[0], _value[1], _value[2]};
            return __builtin_bit_cast(std::uint32_t, array);
        }
        else
        {
            return *reinterpret_cast<std::uint8_t const*>(_value)
                   | (*reinterpret_cast<std::uint16_t const*>(_value + 1) << 8);
        }
#else
        return _value;
#endif
    }
    constexpr std::uint32_t value(std::uint32_t value)
    {
#if _MSC_VER
        if (__builtin_is_constant_evaluated())
        {
            struct array_type
            {
                std::uint8_t array[sizeof(value)];
            };
            auto array = __builtin_bit_cast(array_type, value);
            _value[0]  = array.array[0];
            _value[1]  = array.array[1];
            _value[2]  = array.array[2];
            return value;
        }
        else
        {
            return *reinterpret_cast<std::uint32_t*>(_value) = value;
        }
#else
        LAUF_BITFIELD_CONVERSION(return _value = value);
#endif
    }

    asm_inst_value() = default;
    constexpr asm_inst_value(asm_op op, std::uint32_t value) : op(op), _value{}
    {
        this->value(value);
    }

private:
#if _MSC_VER
    std::uint8_t _value[3];
#else
    std::uint32_t _value : 24;
#endif
};
#pragma pack()

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
