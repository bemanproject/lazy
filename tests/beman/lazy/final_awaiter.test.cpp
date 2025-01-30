// tests/beman/lazy/final_awaiter.test.cpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <beman/lazy/detail/final_awaiter.hpp>
#include <beman/lazy/detail/handle.hpp>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

// ----------------------------------------------------------------------------

namespace {
struct tester {
    template <typename P>
    using handle = beman::lazy::detail::handle<P>;

    struct promise_type {
        bool& called;
        promise_type(bool& c) : called(c) {}
        beman::lazy::detail::final_awaiter initial_suspend() { return {}; }
        std::suspend_always                final_suspend() noexcept { return {}; }
        void                               unhandled_exception() {}
        tester                             get_return_object() { return {handle<promise_type>(this)}; }
        void                               return_void() {}
        void                               complete() { called = true; }
    };

    handle<promise_type> h;
};

} // namespace

int main() {
    bool called{false};
    [](bool&) -> tester { co_return; }(called);
    assert(called);
}