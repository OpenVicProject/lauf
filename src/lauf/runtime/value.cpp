// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/runtime/value.h>

static_assert(sizeof(lauf_runtime_value) == sizeof(uint64_t));

static constexpr uint64_t generation_move = 30;
static constexpr uint64_t offset_move     = 32;

static constexpr uint64_t allocation_mask = (1ull << generation_move) - 1;
static constexpr uint64_t generation_mask = 0x3ull << generation_move;
static constexpr uint64_t offset_mask     = 0xFFFFFFFFull << offset_move;

lauf_runtime_address lauf_runtime_address_from_store(lauf_runtime_address_store addr)
{
    lauf_runtime_address result;
    result.allocation = addr.value & allocation_mask;
    result.generation = static_cast<uint32_t>((addr.value & generation_mask) >> generation_move);
    result.offset     = static_cast<uint32_t>(addr.value >> offset_move);
    return result;
}

lauf_runtime_address_store lauf_runtime_address_to_store(lauf_runtime_address addr)
{
    lauf_runtime_address_store result;
    result.value = addr.allocation;
    result.value |= static_cast<uint64_t>(addr.generation) << generation_move;
    result.value |= static_cast<uint64_t>(addr.offset) << offset_move;
    return result;
}

void lauf_runtime_address_store_set_allocation(lauf_runtime_address_store* store,
                                               uint32_t                    allocation)
{
    store->value &= ~allocation_mask;
    store->value |= allocation & allocation_mask;
}

void lauf_runtime_address_store_set_generation(lauf_runtime_address_store* store,
                                               uint8_t                     generation)
{
    store->value &= ~generation_mask;
    store->value |= static_cast<uint64_t>(generation) << generation_move;
}

void lauf_runtime_address_store_set_offset(lauf_runtime_address_store* store, uint32_t offset)
{
    store->value &= ~static_cast<uint64_t>(offset_mask);
    store->value |= static_cast<uint64_t>(offset) << offset_move;
}