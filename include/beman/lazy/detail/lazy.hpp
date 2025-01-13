// include/beman/lazy/detail/into_optional.hpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_LAZY_DETAIL_LAZY
#define INCLUDED_BEMAN_LAZY_DETAIL_LAZY

#include <beman/execution26/execution.hpp>
#include <beman/lazy/detail/allocator.hpp>
#include <beman/lazy/detail/any_scheduler.hpp>
#include <beman/lazy/detail/stop_source.hpp>
#include <beman/lazy/detail/inline_scheduler.hpp>
#include <concepts>
#include <coroutine>
#include <optional>
#include <type_traits>

// ----------------------------------------------------------------------------

namespace beman::lazy::detail {
template <std::size_t Start, typename Fun, typename V, std::size_t... I>
void sub_visit_helper(Fun& fun, V& v, std::index_sequence<I...>) {
    using thunk_t = void (*)(Fun&, V&);
    static constexpr thunk_t thunks[]{(+[](Fun& fun, V& v) { fun(std::get<Start + I>(v)); })...};
    thunks[v.index() - Start](fun, v);
}

template <std::size_t Start, typename... T>
void sub_visit(auto&& fun, std::variant<T...>& v) {
    if (v.index() < Start)
        return;
    sub_visit_helper<Start>(fun, v, std::make_index_sequence<sizeof...(T) - Start>{});
}

template <typename Awaiter>
concept awaiter = ::beman::execution26::sender<Awaiter> && requires(Awaiter&& awaiter) {
    { awaiter.await_ready() } -> std::same_as<bool>;
    awaiter.disabled(); // remove this to get an awaiter unfriendly coroutine
};

template <typename E>
struct with_error {
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

struct default_context {};

template <typename R>
struct lazy_completion {
    using type = ::beman::execution26::set_value_t(R);
};
template <>
struct lazy_completion<void> {
    using type = ::beman::execution26::set_value_t();
};

template <typename R>
struct lazy_promise_base {
    using type     = std::remove_cvref_t<R>;
    using result_t = std::variant<std::monostate, type, std::exception_ptr, std::error_code>;
    result_t result;
    template <typename T>
    void return_value(T&& value) {
        this->result.template emplace<type>(std::forward<T>(value));
    }
    template <typename E>
    void return_value(::beman::lazy::detail::with_error<E> with) {
        this->result.template emplace<E>(with.error);
    }
};
template <>
struct lazy_promise_base<void> {
    struct void_t {};
    using result_t = std::variant<std::monostate, void_t, std::exception_ptr, std::error_code>;
    result_t result;
    void     return_void() { this->result.template emplace<void_t>(void_t{}); }
};

template <typename T = void, typename C = default_context>
struct lazy {
    using allocator_type   = ::beman::lazy::detail::allocator_of_t<C>;
    using scheduler_type   = ::beman::lazy::detail::scheduler_of_t<C>;
    using stop_source_type = ::beman::lazy::detail::stop_source_of_t<C>;
    using stop_token_type  = decltype(std::declval<stop_source_type>().get_token());

    using sender_concept = ::beman::execution26::sender_t;
    using completion_signatures =
        ::beman::execution26::completion_signatures<typename lazy_completion<T>::type,
                                                    ::beman::execution26::set_error_t(std::exception_ptr),
                                                    ::beman::execution26::set_error_t(std::error_code),
                                                    ::beman::execution26::set_stopped_t()>;

    struct state_base {
        virtual void            complete(lazy_promise_base<std::remove_cvref_t<T>>::result_t&) = 0;
        virtual stop_token_type get_stop_token()                                               = 0;
        virtual C&              get_context()                                                  = 0;

      protected:
        virtual ~state_base() = default;
    };

    struct promise_type : lazy_promise_base<std::remove_cvref_t<T>> {
        template <typename... A>
        void* operator new(std::size_t size, const A&... a) {
            return ::beman::lazy::detail::coroutine_allocate<C>(size, a...);
        }
        void operator delete(void* ptr, std::size_t size) {
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#pragma GCC diagnostic ignored "-Werror=mismatched-new-delete"
#endif
            return ::beman::lazy::detail::coroutine_deallocate<C>(ptr, size);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
        }

        template <typename... A>
        promise_type(const A&... a) : allocator(::beman::lazy::detail::find_allocator<allocator_type>(a...)) {}

        struct final_awaiter {
            promise_type*         promise;
            static constexpr bool await_ready() noexcept { return false; }
            void        await_suspend(std::coroutine_handle<>) noexcept { promise->state->complete(promise->result); }
            static void await_resume() noexcept {}
        };

        std::suspend_always initial_suspend() noexcept { return {}; }
        final_awaiter       final_suspend() noexcept { return {this}; }
        void unhandled_exception() { this->result.template emplace<std::exception_ptr>(std::current_exception()); }
        lazy get_return_object() { return {std::coroutine_handle<promise_type>::from_promise(*this)}; }

        template <typename E>
        auto await_transform(with_error<E> with) noexcept {
            return std::move(with);
        }
        template <::beman::execution26::sender Sender>
        auto await_transform(Sender&& sender) noexcept {
            if constexpr (std::same_as<::beman::lazy::detail::inline_scheduler, scheduler_type>)
                return ::beman::execution26::as_awaitable(std::forward<Sender>(sender), *this);
            else
                return ::beman::execution26::as_awaitable(
                    ::beman::execution26::continues_on(std::forward<Sender>(sender), *(this->scheduler)), *this);
        }
        template <::beman::lazy::detail::awaiter Awaiter>
        auto await_transform(Awaiter&&) noexcept = delete;

        template <typename E>
        final_awaiter yield_value(with_error<E> with) noexcept {
            this->result.template emplace<E>(with.error);
            return {this};
        }

        [[no_unique_address]] allocator_type allocator;
        std::optional<scheduler_type>        scheduler{};
        state_base*                          state{};

        std::coroutine_handle<> unhandled_stopped() {
            this->state->complete(this->result);
            return std::noop_coroutine();
        }

        struct env {
            const promise_type* promise;

            scheduler_type  query(::beman::execution26::get_scheduler_t) const noexcept { return *promise->scheduler; }
            allocator_type  query(::beman::execution26::get_allocator_t) const noexcept { return promise->allocator; }
            stop_token_type query(::beman::execution26::get_stop_token_t) const noexcept {
                return promise->state->get_stop_token();
            }
            template <typename Q, typename... A>
                requires requires(const C& c, Q q, A&&... a) { q(c, std::forward<A>(a)...); }
            auto query(Q q, A&&... a) const noexcept {
                return q(promise->state->get_context(), std::forward<A>(a)...);
            }
        };

        env get_env() const noexcept { return {this}; }
    };

    template <typename Receiver>
    struct state_rep {
        std::remove_cvref_t<Receiver> receiver;
        C                             context;
        template <typename R>
        state_rep(R&& r) : receiver(std::forward<R>(r)), context() {}
    };
    template <typename Receiver>
        requires requires { C(::beman::execution26::get_env(std::declval<std::remove_cvref_t<Receiver>&>())); } &&
                 (not requires(const Receiver& receiver) {
                     typename C::template env_type<decltype(::beman::execution26::get_env(receiver))>;
                 })
    struct state_rep<Receiver> {
        std::remove_cvref_t<Receiver> receiver;
        C                             context;
        template <typename R>
        state_rep(R&& r) : receiver(std::forward<R>(r)), context(::beman::execution26::get_env(this->receiver)) {}
    };
    template <typename Receiver>
        requires requires(const Receiver& receiver) {
            typename C::template env_type<decltype(::beman::execution26::get_env(receiver))>;
        }
    struct state_rep<Receiver> {
        using upstream_env = decltype(::beman::execution26::get_env(std::declval<std::remove_cvref_t<Receiver>&>()));
        std::remove_cvref_t<Receiver>               receiver;
        typename C::template env_type<upstream_env> own_env;
        C                                           context;
        template <typename R>
        state_rep(R&& r)
            : receiver(std::forward<R>(r)),
              own_env(::beman::execution26::get_env(this->receiver)),
              context(this->own_env) {}
    };

    template <typename Receiver>
    struct state : state_base, state_rep<Receiver> {
        using operation_state_concept = ::beman::execution26::operation_state_t;
        using stop_token_t =
            decltype(::beman::execution26::get_stop_token(::beman::execution26::get_env(std::declval<Receiver>())));
        struct stop_link {
            stop_source_type& source;
            void              operator()() const noexcept { source.request_stop(); }
        };
        using stop_callback_t = ::beman::execution26::stop_callback_for_t<stop_token_t, stop_link>;
        template <typename R, typename H>
        state(R&& r, H h) : state_rep<Receiver>(std::forward<R>(r)), handle(std::move(h)) {}
        ~state() {
            if (this->handle) {
                this->handle.destroy();
            }
        }
        std::coroutine_handle<promise_type> handle;
        stop_source_type                    source;
        std::optional<stop_callback_t>      stop_callback;

        void start() & noexcept {
            handle.promise().scheduler.emplace(
                ::beman::execution26::get_scheduler(::beman::execution26::get_env(this->receiver)));
            handle.promise().state = this;
            handle.resume();
        }
        void complete(lazy_promise_base<std::remove_cvref_t<T>>::result_t& result) override {
            switch (result.index()) {
            case 0: // set_stopped
                this->reset_handle();
                ::beman::execution26::set_stopped(std::move(this->receiver));
                break;
            case 1: // set_value
                if constexpr (std::same_as<void, T>) {
                    reset_handle();
                    ::beman::execution26::set_value(std::move(this->receiver));
                } else {
                    auto r(std::move(std::get<1>(result)));
                    this->reset_handle();
                    ::beman::execution26::set_value(std::move(this->receiver), std::move(r));
                }
                break;
            default:
                sub_visit<2>(
                    [this](auto&& r) {
                        this->reset_handle();
                        ::beman::execution26::set_error(std::move(this->receiver), std::move(r));
                    },
                    result);
                break;
            }
        }
        stop_token_type get_stop_token() override {
            if (this->source.stop_possible() && not this->stop_callback) {
                this->stop_callback.emplace(
                    ::beman::execution26::get_stop_token(::beman::execution26::get_env(this->receiver)),
                    stop_link{this->source});
            }
            return this->source.get_token();
        }
        C&   get_context() override { return this->context; }
        void reset_handle() {
            this->handle.destroy();
            this->handle = {};
        }
    };

    std::coroutine_handle<promise_type> handle;

  private:
    lazy(std::coroutine_handle<promise_type> h) : handle(std::move(h)) {}

  public:
    lazy(const lazy& other) = delete;
    lazy(lazy&& other) : handle(std::exchange(other.handle, {})) {}
    ~lazy() {
        if (this->handle) {
            this->handle.destroy();
        }
    }
    lazy& operator=(const lazy&) = delete;
    lazy& operator=(lazy&&)      = delete;

    template <typename Receiver>
    state<Receiver> connect(Receiver receiver) {
        return state<Receiver>(std::forward<Receiver>(receiver), std::exchange(this->handle, {}));
    }
};
} // namespace beman::lazy::detail

// ----------------------------------------------------------------------------

#endif
