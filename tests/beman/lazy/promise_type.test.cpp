// tests/beman/lazy/promise_type.test.cpp                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <beman/lazy/detail/promise_type.hpp>
#include <beman/lazy/detail/any_scheduler.hpp>
#include <beman/execution/execution.hpp>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <stdexcept>
#include <latch>
#include <cassert>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif

namespace ex = beman::execution;

// ----------------------------------------------------------------------------

namespace {
struct thread_pool {

    struct node {
        node*        next;
        virtual void run() = 0;

      protected:
        ~node() = default;
    };

    std::mutex              mutex;
    std::condition_variable condition;
    node*                   stack{};
    bool                    stopped{false};
    std::thread             driver{[this] {
        while (std::optional<node*> n = [this] {
            std::unique_lock cerberus(mutex);
            condition.wait(cerberus, [this] { return stopped || stack; });
            return this->stack ? std::optional<node*>(std::exchange(this->stack, this->stack->next))
                                           : std::optional<node*>();
        }()) {
            (*n)->run();
        }
    }};

    thread_pool() = default;
    ~thread_pool() {
        this->stop();
        this->driver.join();
    }
    void stop() {
        {
            std::lock_guard cerberus(this->mutex);
            stopped = true;
        }
        this->condition.notify_one();
    }

    struct scheduler {
        using scheduler_concept = ex::scheduler_t;
        struct env {
            thread_pool* pool;

            template <typename T>
            scheduler query(const ex::get_completion_scheduler_t<T>&) const noexcept {
                return {this->pool};
            }
        };
        template <typename Receiver>
        struct state final : thread_pool::node {
            using operation_state_concept = ex::operation_state_t;
            std::remove_cvref_t<Receiver> receiver;
            thread_pool*                  pool;

            template <typename R>
            state(R&& r, thread_pool* p) : node{}, receiver(std::forward<R>(r)), pool(p) {}
            void start() & noexcept {
                {
                    std::lock_guard cerberus(this->pool->mutex);
                    this->next = std::exchange(this->pool->stack, this);
                }
                this->pool->condition.notify_one();
            }
            void run() override { ex::set_value(std::move(this->receiver)); }
        };
        struct sender {
            using sender_concept        = ex::sender_t;
            using completion_signatures = ex::completion_signatures<ex::set_value_t()>;
            thread_pool* pool;
            template <typename Receiver>
            state<Receiver> connect(Receiver&& receiver) {
                return state<Receiver>(std::forward<Receiver>(receiver), pool);
            }

            env get_env() const noexcept { return {this->pool}; }
        };
        thread_pool* pool;
        sender       schedule() { return {this->pool}; }
        bool         operator==(const scheduler&) const = default;
    };
    scheduler get_scheduler() { return {this}; }
};

static_assert(ex::scheduler<thread_pool::scheduler>);

struct context {};

struct test_error {
    int value;
};

struct test_lazy : beman::lazy::detail::state_base<context> {

    using promise_type = beman::lazy::detail::promise_type<test_lazy, int, context>;

    beman::lazy::detail::handle<promise_type> handle;
    explicit test_lazy(beman::lazy::detail::handle<promise_type> h) : handle(std::move(h)) {}

    void run() {
        this->handle.start(*this, this);
        this->latch.wait();
    }
    template <beman::execution::receiver Receiver>
    void complete(Receiver&& receiver) {
        this->handle.complete(receiver);
    }

    using stop_source_type = beman::lazy::detail::stop_source_of_t<context>;
    using stop_token_type  = decltype(std::declval<stop_source_type>().get_token());

    std::latch       latch{1u};
    context          ctxt;
    stop_source_type source;

    void            do_complete() override { this->latch.count_down(); }
    stop_token_type do_get_stop_token() override { return this->source.get_token(); }
    context&        do_get_context() override { return this->ctxt; }

    beman::lazy::detail::any_scheduler scheduler{beman::lazy::detail::inline_scheduler{}};
    beman::lazy::detail::any_scheduler query(beman::execution::get_scheduler_t) const noexcept {
        return this->scheduler;
    }
};

struct exception_receiver {
    using receiver_concept = beman::execution::receiver_t;
    bool& flag;

    auto set_value(int) && noexcept { assert(nullptr == "unexcepted set_value"); }
    auto set_stopped() && noexcept { assert(nullptr == "unexcepted set_stopped"); }
    auto set_error(std::exception_ptr ex) && noexcept {
        flag = true;
        try {
            std::rethrow_exception(ex);
        } catch (const test_error& error) {
            assert(error.value == 17);
        } catch (...) {
            assert(nullptr == "unexpected exception");
        }
    }
};

void test_exception() {
    auto coro{[]() -> test_lazy {
        throw test_error{17};
        co_return 0;
    }()};
    coro.run();
    bool flag{};
    coro.complete(exception_receiver{flag});
    assert(flag == true);
}

void test_initial_scheduler() {
    thread_pool     pool;
    std::thread::id id{};
    ex::sync_wait(ex::schedule(pool.get_scheduler()) | ex::then([&id] { id = std::this_thread::get_id(); }));

    auto coro{[](std::thread::id expect) -> test_lazy {
        assert(expect == std::this_thread::get_id());
        co_await ex::just();
        assert(expect == std::this_thread::get_id());
        co_return 0;
    }(id)};
    coro.scheduler = beman::lazy::detail::any_scheduler(pool.get_scheduler());
    coro.run();
}
} // namespace

int main() {
    test_exception();
    test_initial_scheduler();
}
