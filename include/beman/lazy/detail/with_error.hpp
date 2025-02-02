// include/beman/lazy/detail/with_error.hpp                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_INCLUDE_BEMAN_LAZY_DETAIL_WITH_ERROR
#define INCLUDED_INCLUDE_BEMAN_LAZY_DETAIL_WITH_ERROR

#include <coroutine>
#include <utility>

// ----------------------------------------------------------------------------
/*
 * \brief Tag type used to indicate an error is produced.
 * \headerfile beman/lazy/lazy.hpp <beman/lazy/lazy.hpp>
 * \internal
 */
namespace beman::lazy::detail {
template <typename E>
struct with_error {
    using type = E;
    E error;

    // the members below are only need for co_await with_error{...}
    static constexpr bool await_ready() noexcept { return false; }
    template <typename Promise>
        requires requires(Promise p, E e) {
            p.result.template emplace<E>(std::move(e));
            p.state->complete(p.result);
        }
    void await_suspend(std::coroutine_handle<Promise> handle) noexcept(
        noexcept(handle.promise().result.template emplace<E>(std::move(this->error)))) {
        handle.promise().result.template emplace<E>(std::move(this->error));
        handle.promise().state->complete(handle.promise().result);
    }
    static constexpr void await_resume() noexcept {}
};
template <typename E>
with_error(E&&) -> with_error<std::remove_cvref_t<E>>;

} // namespace beman::lazy::detail

// ----------------------------------------------------------------------------

#endif
