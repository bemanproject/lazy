// examples/loop.cpp                                                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <iostream>
#include <string>
#include <beman/execution26/execution.hpp>
#include <beman/lazy/lazy.hpp>

namespace ex = beman::execution26;

ex::lazy<void> loop() {
    for (int i{}; i < 1000000; ++i)
        co_await ex::just(i);
}

int main(int ac, char* av[]) {
    auto count = ac < 1 && av[1] == std::string_view("run-it") ? 1000000 : 1000;
    ex::sync_wait(
        // ex::detail::write_env(
        [](auto count) -> ex::lazy<void> {
            for (int i{}; i < count; ++i)
                co_await ex::just(i);
        }(count)
        //, ex::detail::make_env(ex::get_scheduler, ex::inline_scheduler{}))
    );
}
