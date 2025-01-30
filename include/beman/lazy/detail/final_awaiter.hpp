// include/beman/lazy/detail/final_awaiter.hpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_INCLUDE_BEMAN_LAZY_DETAIL_FINAL_AWAITER
#define INCLUDED_INCLUDE_BEMAN_LAZY_DETAIL_FINAL_AWAITER

#include <coroutine>

// ----------------------------------------------------------------------------

namespace beman::lazy::detail {

struct final_awaiter {
    static constexpr bool await_ready() noexcept { return false; }
    template <typename Promise>
    static void await_suspend(std::coroutine_handle<Promise> handle) noexcept {
        handle.promise().complete();
    }
    static constexpr void await_resume() noexcept {}
};

} // namespace beman::lazy::detail

// ----------------------------------------------------------------------------

#endif
