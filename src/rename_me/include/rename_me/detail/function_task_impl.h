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
			using custom_task = ICustomTask<R, void>;
			using expected_type = expected<R, void>;
		};

		template<typename T, typename E>
		struct FunctionTaskReturnImpl<expected<T, E>>
		{
			using is_expected = std::bool_constant<true>;
			using is_task = std::bool_constant<false>;
			using type = Task<T, E>;
			using custom_task = ICustomTask<T, E>;
			using expected_type = expected<T, E>;
		};

		template<typename T, typename E>
		struct FunctionTaskReturnImpl<Task<T, E>>
		{
			using is_expected = std::bool_constant<false>;
			using is_task = std::bool_constant<true>;
			using type = Task<T, E>;
			using custom_task = ICustomTask<T, E>;
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

		template<
			// FunctionTaskReturn helper
			typename Return
			, typename Invoker
			// Proper ICustomTask<T, E>
			, typename CustomTask = typename Return::custom_task>
		class FunctionTask final
			: public CustomTask
			, private Invoker
		{
		public:
			enum class Step : std::uint8_t
			{
				None,
				Invoked,
				Canceled,
			};
			
			using Value = std::variant<
				typename Return::type // Either Task<> or
				, typename Return::expected_type>; // expected<>

			using IsExpected = typename Return::is_expected;
			using IsTask = typename Return::is_task;
			using IsValueVoid = std::is_same<
				typename Return::type::value_type, void>;
			using IsApplyVoid = std::integral_constant<bool
				, !IsExpected::value && IsValueVoid::value>;
			// Workaround for "broken"/temporary expected<void, void> we have now 
			using IsVoidExpected = std::integral_constant<bool
				, IsExpected::value && IsValueVoid::value>;

			explicit FunctionTask(Invoker&& invoker)
				: Invoker(std::move(invoker))
				, value_()
				, step_(Step::None)
				, value_set_(false)
			{
			}

			virtual ~FunctionTask() override
			{
				if (value_set_)
				{
					value().~Value();
				}
			}

			FunctionTask(FunctionTask&&) = delete;
			FunctionTask& operator=(FunctionTask&&) = delete;
			FunctionTask(const FunctionTask&) = delete;
			FunctionTask& operator=(const FunctionTask&) = delete;

			virtual State tick(bool cancel_requested) override
			{
				if (cancel_requested && (step_ == Step::None))
				{
					step_ = Step::Canceled;
					return State(Status::Failed, true/*canceled*/);
				}
				if (step_ == Step::Canceled)
				{
					return Status::Failed;
				}
				if (Invoker::wait())
				{
					assert(step_ == Step::None);
					return Status::InProgress;
				}

				if (IsTask() && (step_ == Step::Invoked))
				{
					return tick_task(cancel_requested);
				}

				assert(step_ == Step::None);
				step_ = Step::Invoked;
				call_impl(IsApplyVoid());

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

				// Single T was returned, we are done right after call
				return Status::Successful;
			}

			typename Return::type& get_task()
			{
				assert(value_set_);
				auto& v = value();
				assert(v.index() == 0);
				return std::get<0>(v);
			}

			typename Return::expected_type& get_expected()
			{
				assert(value_set_);
				auto& v = value();
				assert(v.index() == 1);
				return std::get<1>(v);
			}

			State tick_task(bool cancel_requested)
			{
				auto& task = get_task();
				if (cancel_requested)
				{
					task.try_cancel();
				}
				return State(task.status(), task.is_canceled());
			}

			virtual typename Return::expected_type& get() override
			{
				return (IsTask() ? get_task().get() : get_expected());
			}

			void call_impl(std::true_type/*f(...) is void*/)
			{
				using Expected = typename Return::expected_type;
				new(static_cast<void*>(std::addressof(value_))) Value(Expected());
				value_set_ = true;
				assert(!IsTask()
					&& "void f() should construct expected<void, ...> return");
				Invoker::invoke();
			}

			void call_impl(std::false_type/*f(...) is NOT void*/)
			{
				// f() returns Task<> or single value T or expected<>.
				// In any case std::variant<> c-tor will resolve to proper active
				// member since Task<> can't be constructed from T or expected.
				new(static_cast<void*>(std::addressof(value_))) Value(Invoker::invoke());
				value_set_ = true;
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

		private:
			using ValueStorage = typename std::aligned_storage<
				sizeof(Value), alignof(Value)>::type;

			ValueStorage value_;
			Step step_;
			bool value_set_;
		};

	} // namespace detail
} // namespace nn
