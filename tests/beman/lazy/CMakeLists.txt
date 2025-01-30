# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

list(
    APPEND
    lazy_tests
    handle
    final_awaiter
    allocator_of
    allocator_support
    any_scheduler
    find_allocator
    inline_scheduler
    lazy
    poly
    scheduler_of
)

foreach(test ${lazy_tests})
    add_executable(beman.lazy.tests.${test})
    target_sources(beman.lazy.tests.${test} PRIVATE ${test}.test.cpp)
    target_link_libraries(beman.lazy.tests.${test} PRIVATE beman::lazy)
    add_test(
        NAME beman.lazy.tests.${test}
        COMMAND $<TARGET_FILE:beman.lazy.tests.${test}>
    )
endforeach()
