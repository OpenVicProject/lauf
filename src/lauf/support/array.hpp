// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_SUPPORT_ARRAY_HPP_INCLUDED
#define SRC_LAUF_SUPPORT_ARRAY_HPP_INCLUDED

#include <cassert>
#include <lauf/config.h>
#include <lauf/support/arena.hpp>
#include <type_traits>

namespace lauf
{
/// Essentially a `std::vector<T>` that uses an arena when possible.
template <typename T>
class array
{
    static_assert(std::is_trivially_copyable_v<T>); // For simplicity for now.

public:
    //=== constructors ===//
    array() : _ptr(nullptr), _size(0), _capacity(0), _is_heap(false) {}

    array(const array&)            = delete;
    array& operator=(const array&) = delete;

    ~array()
    {
        if (_is_heap)
            ::operator delete(_ptr);
    }

    //=== access ===//
    bool empty() const
    {
        return _size == 0;
    }

    std::size_t size() const
    {
        return _size;
    }
    std::size_t capacity() const
    {
        return _capacity;
    }

    T& operator[](std::size_t idx)
    {
        assert(idx < _size);
        return _ptr[idx];
    }
    const T& operator[](std::size_t idx) const
    {
        assert(idx < _size);
        return _ptr[idx];
    }

    T& front()
    {
        return _ptr[0];
    }
    const T& front() const
    {
        return _ptr[0];
    }
    T& back()
    {
        return _ptr[_size - 1];
    }
    const T& back() const
    {
        return _ptr[_size - 1];
    }

    T* begin()
    {
        return _ptr;
    }
    T* end()
    {
        return _ptr + _size;
    }

    const T* begin() const
    {
        return _ptr;
    }
    const T* end() const
    {
        return _ptr + _size;
    }

    //=== modifiers ===//
    void clear()
    {
        _size = 0;
        if (!_is_heap)
        {
            // If we aren't using heap memory, reset pointer and capacity as the arena might have
            // been cleared as well. (If we are using heap memory, we don't want to free it).
            _ptr      = nullptr;
            _capacity = 0;
        }
    }

    void reserve(arena_base& arena, std::size_t new_size)
    {
        if (new_size < _capacity)
            return;

        constexpr auto initial_capacity = 64;
        if (_capacity == 0 && new_size <= initial_capacity)
        {
            _ptr      = arena.template allocate<T>(initial_capacity);
            _capacity = initial_capacity;
            return;
        }

        auto new_capacity = 2 * _capacity;
        if (new_capacity < new_size)
            new_capacity = new_size;

        if (!_is_heap && arena.try_expand(_ptr, _capacity, new_capacity))
        {
            _capacity = new_capacity;
        }
        else
        {
            auto new_memory = ::operator new(new_capacity * sizeof(T));
            std::memcpy(new_memory, _ptr, _size * sizeof(T));
            if (_is_heap)
                ::operator delete(_ptr);

            _ptr      = static_cast<T*>(new_memory);
            _capacity = new_capacity;
            _is_heap  = true;
        }
    }

    void push_back_unchecked(const T& obj)
    {
        _ptr[_size] = obj;
        ++_size;
    }
    void push_back(arena_base& arena, const T& obj)
    {
        reserve(arena, _size + 1);
        push_back_unchecked(obj);
    }

    template <typename... Args>
    void emplace_back_unchecked(Args&&... args)
    {
        ::new (&_ptr[_size]) T(static_cast<Args&&>(args)...);
        ++_size;
    }
    template <typename... Args>
    void emplace_back(arena_base& arena, Args&&... args)
    {
        reserve(arena, _size + 1);
        emplace_back_unchecked(static_cast<Args&&>(args)...);
    }

    void shrink(std::size_t new_size)
    {
        assert(new_size <= _size);
        _size = new_size;
    }

    void resize_uninitialized(arena_base& arena, std::size_t new_size)
    {
        if (new_size < _size)
        {
            _size = new_size;
        }
        else
        {
            reserve(arena, new_size);
            _size = new_size;
        }
    }

    void pop_back()
    {
        --_size;
    }

private:
    T*          _ptr;
    std::size_t _size;
    std::size_t _capacity : 63;
    bool        _is_heap : 1;
};
} // namespace lauf

#endif // SRC_LAUF_SUPPORT_ARRAY_HPP_INCLUDED

