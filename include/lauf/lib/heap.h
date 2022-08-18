// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_LIB_HEAP_H_INCLUDED
#define LAUF_LIB_HEAP_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_runtime_builtin         lauf_runtime_builtin;
typedef struct lauf_runtime_builtin_library lauf_runtime_builtin_library;

extern const lauf_runtime_builtin_library lauf_lib_heap;

/// Allocates new heap memory using the allocator of the VM.
///
/// Memory that hasn't been freed will be automatically freed when VM execution finishes.
///
/// Signature: alignment:uint size:uint => ptr:address
extern const lauf_runtime_builtin lauf_lib_heap_alloc;

/// Allocates new heap memory for an array using the allocator of the VM.
///
/// It behaves like `lauf_lib_heap_alloc` but takes the count of the array as additional parameter.
///
/// Signature: alignment:uint size:uint count:uint => ptr:address
extern const lauf_runtime_builtin lauf_lib_heap_alloc_array;

/// Frees previously allocated heap memory.
///
/// Signature: ptr:address => _
extern const lauf_runtime_builtin lauf_lib_heap_free;

/// Marks heap memory as freed without it being actually freed.
///
/// This prevents code from ever accessing it again.
/// It also allows heap memory to live after VM execution has finished.
///
/// Signature: ptr:address => _
extern const lauf_runtime_builtin lauf_lib_heap_leak;

/// Transfers a local variable to the heap by memcpy'ing it.
///
/// If the address points to heap or global memory, returns it unchanged.
/// Otherwise, allocates new heap memory of the same size and default alignment,
/// and uses `memcpy()` to copy the bytes over.
///
/// This can be used when a local variable escapes from the function and needs to be heap allocated.
///
/// Signature: ptr:address => heap_ptr:address
extern const lauf_runtime_builtin lauf_lib_heap_transfer_local;

/// Frees all heap allocated memory that is not reachable.
///
/// It uses a conservative tracing algorithm that assumes anything that could be a valid address is
/// one. Addresses with invalid offsets do not keep the allocation alive.
///
/// Signature: _ => total_bytes_freed:uint
extern const lauf_runtime_builtin lauf_lib_heap_gc;

/// Marks a heap allocation as reachable for the purposes of garbage collection.
///
/// Signature: ptr:address => _
extern const lauf_runtime_builtin lauf_lib_heap_declare_reachable;

/// Unmarks a heap allocation as reachable for the purposes of garbage collection.
///
/// Signature: ptr:address => _
extern const lauf_runtime_builtin lauf_lib_heap_undeclare_reachable;

/// Marks an (arbitrary) allocation as weak for the purposes of garbage collection.
///
/// When determening whether an allocation is reachable, any addresses inside weak allocations are
/// not considered.
///
/// Signature: ptr:address => _
extern const lauf_runtime_builtin lauf_lib_heap_declare_weak;

/// Undeclares an allocation as weak.
///
/// Signature: ptr:address => _
extern const lauf_runtime_builtin lauf_lib_heap_undeclare_weak;

LAUF_HEADER_END

#endif // LAUF_LIB_HEAP_H_INCLUDED

