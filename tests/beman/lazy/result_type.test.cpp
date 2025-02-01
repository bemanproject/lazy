// tests/beman/lazy/result_type.test.cpp                              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <beman/lazy/detail/result_type.hpp>
#include <beman/execution/execution.hpp>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

// ----------------------------------------------------------------------------

namespace {
struct stopped_receiver {
    using receiver_concept = ::beman::execution::receiver_t;
    bool& flag;
    void  set_value(auto&&...) && noexcept { assert(nullptr == "set_value unexpectedly called"); }
    void  set_error(auto&&) && noexcept { assert(nullptr == "set_error unexpectedly called"); }
    void  set_stopped() && noexcept { flag = true; }
};
static_assert(::beman::execution::receiver<stopped_receiver>);

struct value_receiver {
    using receiver_concept = ::beman::execution::receiver_t;
    int& value;
    void set_value(int v) && noexcept { value = v; }
    void set_value(auto&&...) && noexcept { assert(nullptr == "unexpected set_value called"); }
    void set_error(auto&&) && noexcept { assert(nullptr == "set_error unexpectedly called"); }
};
static_assert(::beman::execution::receiver<value_receiver>);

struct void_receiver {
    using receiver_concept = ::beman::execution::receiver_t;
    bool& flag;
    void  set_value() && noexcept { flag = true; }
    void  set_value(auto&&...) && noexcept { assert(nullptr == "unexpected set_value called"); }
    void  set_error(auto&&) && noexcept { assert(nullptr == "set_error unexpectedly called"); }
};
static_assert(::beman::execution::receiver<value_receiver>);

struct error_receiver {
    using receiver_concept = ::beman::execution::receiver_t;
    int& error;
    void set_value(auto&&...) && noexcept { assert(nullptr == "unexpected set_value called"); }
    void set_error(auto&&) && noexcept { assert(nullptr == "unexpected set_error called"); }
    void set_error(int e) && noexcept { error = e; }
};
static_assert(::beman::execution::receiver<error_receiver>);

void test_stopped() {
    beman::lazy::detail::result_type<beman::lazy::detail::stoppable::yes, int> result{};

    bool flag{false};
    result.complete(stopped_receiver{flag});
    assert(flag == true);
}

void test_value() {
    {
        beman::lazy::detail::result_type<beman::lazy::detail::stoppable::no, int, int> result{};
        result.set_value(' ');

        int value{};
        result.complete(value_receiver{value});
        assert(value == ' ');
    }
    {
        beman::lazy::detail::result_type<beman::lazy::detail::stoppable::no, ::beman::lazy::detail::void_type, int>
            result{};
        result.set_value(::beman::lazy::detail::void_type());
        bool flag{false};
        result.complete(void_receiver{flag});
        assert(flag == true);
    }
}

void test_error() {
    struct type {};
    beman::lazy::detail::result_type<beman::lazy::detail::stoppable::no, int, type, int> result{};
    result.set_error(17);

    int error{};
    result.complete(error_receiver{error});
    assert(error == 17);
}

} // namespace

int main() {
    test_stopped();
    test_value();
    test_error();
}
