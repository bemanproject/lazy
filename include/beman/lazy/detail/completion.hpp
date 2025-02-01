// include/beman/lazy/detail/completion.hpp                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_INCLUDE_BEMAN_LAZY_DETAIL_COMPLETION
#define INCLUDED_INCLUDE_BEMAN_LAZY_DETAIL_COMPLETION

#include <beman/execution/execution.hpp>

// ----------------------------------------------------------------------------

namespace beman::lazy::detail {
template <typename R>
struct completion {
    using type = ::beman::execution::set_value_t(R);
};
template <>
struct completion<void> {
    using type = ::beman::execution::set_value_t();
};

template <typename R>
using completion_t = typename beman::lazy::detail::completion<R>::type;

} // namespace beman::lazy::detail

// ----------------------------------------------------------------------------

#endif
