//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

// ReSharper disable CppMemberFunctionMayBeStatic

namespace nova {

//! A vector like container with a capacity defined at compile-time.
/*!
 A StaticVector is a fixed-size array that provides a similar interface to a
 standard vector. It is designed for use cases where the maximum number of
 elements is known at compile-time.

 StaticVector maintains vector-like semantics, and supports familiar operations
 in other STL containers, with the following key differences:
    - No dynamic allocation: Unlike `std::vector`, it can't grow beyond
      `MaxElements`.
    - Static storage: All memory is allocated at compile time.
    - Size tracking: Track its current size separately from capacity.
    - No reverse iterators: Only forward iterators are supported.
    - No random access: Only sequential access is supported.
    - Can only add elements at the back (`push_back` or `emplace_back`) and
      remove elements from the back (`pop_back`).

 @note When constructing or assigning a StaticVector from an initializer list or
       another collection, if the number of input elements exceeds the
       StaticVector's capacity (`MaxElements`), only the first MaxElements
       elements are used and the rest are ignored (truncated). This truncation
       is always allowed and <b>does not throw</b>, but in debug builds an
       assertion will fail if the input size exceeds MaxElements. In release
       builds, truncation is silent.
*/
template <typename T, std::size_t MaxElements> class StaticVector {
  alignas(T) std::byte storage_[sizeof(T) * MaxElements] {};
  std::size_t current_size_ { 0 };

public:
  // ===== Type definitions =====
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = T&;
  using const_reference = const T&;
  using pointer = T*;
  using const_pointer = const T*;
  using iterator = T*;
  using const_iterator = const T*;

  // ===== Constructors / Destructor =====

  constexpr StaticVector() noexcept = default;

  constexpr ~StaticVector() noexcept { clear(); }

  constexpr StaticVector(const size_type count, const T& value)
    : current_size_((std::min)(count, MaxElements))
  {
    assert(count <= MaxElements && "StaticVector: count exceeds maximum size");
    for (size_type i = 0; i < current_size_; ++i) {
      new (storage_ + (i * sizeof(T))) T(value);
    }
  }

  constexpr explicit StaticVector(const size_type count)
    requires std::default_initializable<T>
    : current_size_((std::min)(count, MaxElements))
  {
    assert(count <= MaxElements && "StaticVector: count exceeds maximum size");
    for (size_type i = 0; i < current_size_; ++i) {
      new (storage_ + (i * sizeof(T))) T();
    }
  }

  template <typename InputIt>
    requires std::input_iterator<InputIt>
  constexpr StaticVector(InputIt first, InputIt last)
  {
    const auto input_size = static_cast<size_type>(std::distance(first, last));
    current_size_ = (std::min)(input_size, MaxElements);
    auto it = first;
    for (size_type i = 0; i < current_size_; ++i, ++it) {
      new (storage_ + (i * sizeof(T))) T(*it);
    }
    assert(input_size <= MaxElements
      && "StaticVector: range constructor input exceeds maximum size");
  }

  constexpr StaticVector(std::initializer_list<T> init)
    : current_size_((std::min)(init.size(), MaxElements))
  {
    assert(init.size() <= MaxElements
      && "StaticVector: initializer list size exceeds maximum size");
    auto it = init.begin();
    for (size_type i = 0; i < current_size_; ++i, ++it) {
      new (storage_ + (i * sizeof(T))) T(*it);
    }
  }

  constexpr StaticVector(const StaticVector& other)
    : current_size_(other.current_size_)
  {
    for (size_type i = 0; i < current_size_; ++i) {
      new (storage_ + (i * sizeof(T))) T(other[i]);
    }
  }

  constexpr StaticVector(StaticVector&& other) noexcept
    requires std::move_constructible<T>
    : current_size_(other.current_size_)
  {
    for (size_type i = 0; i < current_size_; ++i) {
      new (storage_ + (i * sizeof(T))) T(std::move(other[i]));
    }
    other.current_size_ = 0;
  }

  constexpr auto operator=(const StaticVector& other) -> StaticVector&
  {
    if (this != &other) {
      clear();
      current_size_ = other.current_size_;
      for (size_type i = 0; i < current_size_; ++i) {
        new (storage_ + (i * sizeof(T))) T(other[i]);
      }
    }
    return *this;
  }

  constexpr auto operator=(StaticVector&& other) noexcept -> StaticVector&
    requires std::move_constructible<T>
  {
    if (this != &other) {
      clear();
      current_size_ = other.current_size_;
      for (size_type i = 0; i < current_size_; ++i) {
        new (storage_ + (i * sizeof(T))) T(std::move(other[i]));
      }
      other.current_size_ = 0;
    }
    return *this;
  }

  constexpr auto operator=(std::initializer_list<T> list) -> StaticVector&
  {
    clear();
    current_size_ = (std::min)(list.size(), MaxElements);
    assert(list.size() <= MaxElements
      && "StaticVector: initializer list size exceeds maximum size");
    auto it = list.begin();
    for (size_type i = 0; i < current_size_; ++i, ++it) {
      new (storage_ + (i * sizeof(T))) T(*it);
    }
    return *this;
  }

  // ===== Element access =====

  [[nodiscard]] constexpr auto at(size_type pos) -> reference
  {
    if (pos >= current_size_) {
      throw std::out_of_range("StaticVector::at: pos (which is "
        + std::to_string(pos) + ") >= this->size() (which is "
        + std::to_string(current_size_) + ")");
    }
    return (*this)[pos];
  }

  [[nodiscard]] constexpr auto at(size_type pos) const -> const_reference
  {
    if (pos >= current_size_) {
      throw std::out_of_range("StaticVector::at: pos (which is "
        + std::to_string(pos) + ") >= this->size() (which is "
        + std::to_string(current_size_) + ")");
    }
    return (*this)[pos];
  }

  [[nodiscard]] constexpr auto operator[](const size_type pos) -> reference
  {
    assert(pos < current_size_ && "StaticVector: out of bounds access");
    return *reinterpret_cast<pointer>(storage_ + (pos * sizeof(T)));
  }

  [[nodiscard]] constexpr auto operator[](const size_type pos) const
    -> const_reference
  {
    assert(pos < current_size_ && "StaticVector: out of bounds access");
    return *reinterpret_cast<const_pointer>(storage_ + (pos * sizeof(T)));
  }

  [[nodiscard]] constexpr auto front() -> reference
  {
    assert(!empty() && "StaticVector: front() called on empty container");
    return (*this)[0];
  }

  [[nodiscard]] constexpr auto front() const -> const_reference
  {
    assert(!empty() && "StaticVector: front() called on empty container");
    return (*this)[0];
  }

  [[nodiscard]] constexpr auto back() -> reference
  {
    assert(!empty() && "StaticVector: back() called on empty container");
    return (*this)[current_size_ - 1];
  }

  [[nodiscard]] constexpr auto back() const -> const_reference
  {
    assert(!empty() && "StaticVector: back() called on empty container");
    return (*this)[current_size_ - 1];
  }

  [[nodiscard]] constexpr auto data() noexcept -> pointer
  {
    return reinterpret_cast<pointer>(storage_);
  }

  [[nodiscard]] constexpr auto data() const noexcept -> const_pointer
  {
    return reinterpret_cast<const_pointer>(storage_);
  }

  // ===== Iterators =====

  [[nodiscard]] constexpr auto begin() noexcept { return data(); }
  [[nodiscard]] constexpr auto begin() const noexcept { return data(); }
  [[nodiscard]] constexpr auto cbegin() const noexcept { return data(); }
  [[nodiscard]] constexpr auto end() noexcept { return data() + current_size_; }
  [[nodiscard]] constexpr auto end() const noexcept
  {
    return data() + current_size_;
  }
  [[nodiscard]] constexpr auto cend() const noexcept
  {
    return data() + current_size_;
  }

  // ===== Capacity =====

  [[nodiscard]] constexpr auto empty() const noexcept
  {
    return current_size_ == 0;
  }
  [[nodiscard]] constexpr auto size() const noexcept { return current_size_; }
  [[nodiscard]] constexpr auto max_size() const noexcept { return MaxElements; }
  [[nodiscard]] constexpr auto capacity() const noexcept { return MaxElements; }

  // ===== Modifiers =====

  constexpr void clear() noexcept
  {
    if constexpr (!std::is_trivially_destructible_v<T>) {
      for (size_type i = 0; i < current_size_; ++i) {
        (*this)[i].~T();
      }
    }
    current_size_ = 0;
  }

  constexpr void push_back(const T& value)
  {
    if (current_size_ >= MaxElements) {
      throw std::length_error(
        "StaticVector::push_back: cannot add element, container is full");
    }
    new (storage_ + (current_size_ * sizeof(T))) T(value);
    ++current_size_;
  }

  constexpr void push_back(T&& value)
  {
    if (current_size_ >= MaxElements) {
      throw std::length_error(
        "StaticVector::push_back: cannot add element, container is full");
    }
    new (storage_ + (current_size_ * sizeof(T))) T(std::move(value));
    ++current_size_;
  }

  template <typename... Args>
  constexpr auto emplace_back(Args&&... args) -> reference
  {
    if (current_size_ >= MaxElements) {
      throw std::length_error(
        "StaticVector::emplace_back: cannot add element, container is full");
    }
    new (storage_ + (current_size_ * sizeof(T))) T(std::forward<Args>(args)...);
    return (*this)[current_size_++];
  }

  constexpr void pop_back()
  {
    assert(!empty() && "StaticVector::pop_back called on empty container");
    if constexpr (!std::is_trivially_destructible_v<T>) {
      back().~T();
    }
    --current_size_;
  }

  constexpr void resize(const size_type new_size)
    requires std::default_initializable<T>
  {
    if (new_size > MaxElements) {
      throw std::length_error(
        "StaticVector::resize: new_size exceeds max_size()");
    }
    if (new_size > current_size_) {
      for (size_type i = current_size_; i < new_size; ++i) {
        new (storage_ + (i * sizeof(T))) T();
      }
    } else if (new_size < current_size_) {
      if constexpr (!std::is_trivially_destructible_v<T>) {
        for (size_type i = new_size; i < current_size_; ++i) {
          (*this)[i].~T();
        }
      }
    }
    current_size_ = new_size;
  }

  constexpr void resize(const size_type new_size, const value_type& value)
  {
    if (new_size > MaxElements) {
      throw std::length_error(
        "StaticVector::resize: new_size exceeds max_size()");
    }
    if (new_size > current_size_) {
      for (size_type i = current_size_; i < new_size; ++i) {
        new (storage_ + (i * sizeof(T))) T(value);
      }
    } else if (new_size < current_size_) {
      if constexpr (!std::is_trivially_destructible_v<T>) {
        for (size_type i = new_size; i < current_size_; ++i) {
          (*this)[i].~T();
        }
      }
    }
    current_size_ = new_size;
  }

  // ===== Comparison operators =====

  constexpr auto operator==(const StaticVector& other) const -> bool
  {
    if (current_size_ != other.current_size_) {
      return false;
    }
    return std::equal(begin(), end(), other.begin());
  }

  constexpr auto operator<=>(const StaticVector& other) const
  {
    return std::lexicographical_compare_three_way(
      begin(), end(), other.begin(), other.end());
  }
};

} // namespace nova
