#pragma once
#include <rename_me/storage.h>
#include <rename_me/detail/internal_task.h>
#include <rename_me/detail/cpp_20.h>

#include <functional>
#include <tuple>
#include <variant>

#include <cassert>
#include <cstdint>

namespace nn
{
	
	template<typename T, typename E>
	class Task;

	namespace detail
	{
		template<typename R>
		struct FunctionTaskReturnImpl
		{
			using is_expected = std::bool_constant<false>;
			using is_task = std::bool_constant<false>;
			using type = Task<R, void>;
			using expected_type = expected<R, void>;
		};

		template<typename T, typename E>
		struct FunctionTaskReturnImpl<expected<T, E>>
		{
			using is_expected = std::bool_constant<true>;
			using is_task = std::bool_constant<false>;
			using type = Task<T, E>;
			using expected_type = expected<T, E>;
		};

		template<typename T, typename E>
		struct FunctionTaskReturnImpl<Task<T, E>>
		{
			using is_expected = std::bool_constant<false>;
			using is_task = std::bool_constant<true>;
			using type = Task<T, E>;
			using expected_type = expected<T, E>;
		};

		template<typename F, typename ArgsTuple
			, typename R = remove_cvref_t<decltype(
				std::apply(std::declval<F>(), std::declval<ArgsTuple>()))>>
		struct FunctionTaskReturn : FunctionTaskReturnImpl<R>
		{
		};

		// Help MSVC to out-of-class definition of the on_finish() function
		template<typename F, typename ArgsTuple>
		using FunctionTaskReturnT = typename FunctionTaskReturn<F, ArgsTuple>::type;

		// `Invoker` is:
		//  (1) `auto invoke()` that returns result of functor invocation.
		//  (2) `bool can_invoke()` that returns true if invoke() call is allowed.
		//    Otherwise task will be marked as canceled.
		//  (3) `bool wait() const` that returns true if invoke() call is delayed.
		template<
			// FunctionTaskReturn helper
			typename Return
			, typename Invoker>
		class FunctionTask
			: private Invoker
		{
		public:
			using Value = std::variant<
				typename Return::type // Either Task<> or
				, typename Return::expected_type>; // expected<>

			using IsExpected = typename Return::is_expected;
			using IsTask = typename Return::is_task;
			using IsValueVoid = std::is_same<
				typename Return::type::value_type, void>;
			using IsApplyVoid = std::integral_constant<bool
				, !IsExpected::value && !IsTask::value && IsValueVoid::value>;
			// Workaround for "broken"/temporary expected<void, void> we have now 
			using IsVoidExpected = std::integral_constant<bool
				, IsExpected::value && IsValueVoid::value>;

			explicit FunctionTask(Invoker&& invoker)
				: Invoker(std::move(invoker))
				, value_()
				, invoked_(false)
			{
			}

			~FunctionTask()
			{
				if (invoked_)
				{
					value().~Value();
				}
			}

			FunctionTask(FunctionTask&&) = delete;
			FunctionTask& operator=(FunctionTask&&) = delete;
			FunctionTask(const FunctionTask&) = delete;
			FunctionTask& operator=(const FunctionTask&) = delete;

			Status tick(bool cancel_requested)
			{
				if (cancel_requested && !invoked_)
				{
					return Status::Canceled;
				}
				if (IsTask() && invoked_)
				{
					return tick_task(cancel_requested);
				}
				if (const_invoker().wait())
				{
					assert(!invoked_);
					return Status::InProgress;
				}
				if (!invoker().can_invoke())
				{
					return Status::Canceled;
				}

				assert(!invoked_);
				call_impl(IsApplyVoid());
				invoked_ = true;

				if (IsExpected())
				{
					return get_expected().has_value()
						? Status::Successful
						: Status::Failed;
				}
				else if (IsTask())
				{
					return tick_task(cancel_requested);
				}

				// Single value was returned, we are done right after call
				return Status::Successful;
			}

			typename Return::type& get_task()
			{
				assert(invoked_);
				auto& v = value();
				assert(v.index() == 0);
				return std::get<0>(v);
			}

			typename Return::expected_type& get_expected()
			{
				assert(invoked_);
				auto& v = value();
				assert(v.index() == 1);
				return std::get<1>(v);
			}

			Status tick_task(bool cancel_requested)
			{
				auto& task = get_task();
				if (cancel_requested)
				{
					task.try_cancel();
				}
				return task.status();
			}

			typename Return::expected_type& get()
			{
				return (IsTask() ? get_task().get() : get_expected());
			}

			void call_impl(std::true_type/*f(...) is void*/)
			{
				using Expected = typename Return::expected_type;
				new(static_cast<void*>(std::addressof(value_))) Value(Expected());
				assert(!IsTask()
					&& "void f() should construct expected<void, ...> return");
				invoker().invoke();
			}

			void call_impl(std::false_type/*f(...) is NOT void*/)
			{
				// f() returns Task<> or single value T or expected<>.
				// In any case std::variant<> c-tor will resolve to proper active
				// member since Task<> can't be constructed from T or expected.
				new(static_cast<void*>(std::addressof(value_))) Value(invoker().invoke());
				assert((!IsTask() && (value().index() == 1))
					|| ( IsTask() && (value().index() == 0)));
			}

			Value& value()
			{
				return *reinterpret_cast<Value*>(std::addressof(value_));
			}

			Invoker& invoker()
			{
				return static_cast<Invoker&>(*this);
			}

			const Invoker& const_invoker()
			{
				return static_cast<const Invoker&>(*this);
			}

		private:
			using ValueStorage = typename std::aligned_storage<
				sizeof(Value), alignof(Value)>::type;

			ValueStorage value_;
			bool invoked_;
		};

	} // namespace detail
} // namespace nn
