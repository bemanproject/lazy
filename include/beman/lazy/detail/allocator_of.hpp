// include/beman/lazy/detail/allocator_of.hpp                         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_INCLUDE_BEMAN_LAZY_DETAIL_ALLOCATOR_OF
#define INCLUDED_INCLUDE_BEMAN_LAZY_DETAIL_ALLOCATOR_OF

#include <concepts>
#include <memory>

// ----------------------------------------------------------------------------

namespace beman::lazy::detail {
/*!
 * \brief Utility to get an allocator type from a context
 * \headerfile beman/lazy/lazy.hpp <beman/lazy/lazy.hpp>
 * \internal
 */
template <typename>
struct allocator_of {
    using type = std::allocator<std::byte>;
};
template <typename Context>
    requires requires { typename Context::allocator_type; }
struct allocator_of<Context> {
    using type = typename Context::allocator_type;
    static_assert(
        requires(type& a, std::size_t s, std::byte* ptr){
            { a.allocate(s) } -> std::same_as<std::byte*>;
            a.deallocate(ptr, s);
        }, "The allocator_type needs to be an allocator of std::byte");
};
template <typename Context>
using allocator_of_t = typename allocator_of<Context>::type;
}

// ----------------------------------------------------------------------------

#endif
