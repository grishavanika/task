#pragma once
#include <rename_me/task.h>
#include <rename_me/storage.h>
#include <rename_me/detail/cpp_20.h>

#include <functional>
#include <tuple>

#include <cassert>
#include <cstdint>

namespace nn
{
	namespace detail
	{
		template<typename R>
		struct FunctionTaskReturnImpl
		{
			using is_expected = std::bool_constant<false>;
			using type = Task<R, void>;
			using custom_task = ICustomTask<R, void>;
			using value = expected<R, void>;
		};

		template<typename T, typename E>
		struct FunctionTaskReturnImpl<expected<T, E>>
		{
			using is_expected = std::bool_constant<true>;
			using type = Task<T, E>;
			using custom_task = ICustomTask<T, E>;
			using value = expected<T, E>;
		};

		template<typename F, typename ArgsTuple
			, typename R = remove_cvref_t<decltype(
				std::apply(std::declval<F>(), std::declval<ArgsTuple>()))>>
		struct FunctionTaskReturn : FunctionTaskReturnImpl<R>
		{
		};

		template<typename Return // FunctionTaskReturn helper
			, typename CustomTask // Proper ICustomTask<T, E>
			, typename ExpectedReturn // Proper expected<T, E>
			, typename F
			, typename ArgsTuple>
		class FunctionTask;

		template<typename Return
			, typename CustomTask
			, typename ExpectedReturn
			, typename F
			, typename... Args>
		class FunctionTask<Return, CustomTask, ExpectedReturn, F, std::tuple<Args...>> final
			: public CustomTask
			, private F // Enable EBO if possible
			, private std::tuple<Args...>
			, private ExpectedReturn
		{
		public:
			enum class State : std::uint8_t
			{
				None,
				Invoked,
				Canceled,
			};
			
			using ArgsBase = std::tuple<Args...>;
			using IsExpected = typename Return::is_expected;
			using IsValueVoid = std::is_same<
				typename Return::type::value_type, void>;
			using IsApplyVoid = std::integral_constant<bool
				, !IsExpected::value && IsValueVoid::value>;
			using IsVoidExpected = std::integral_constant<bool
				, IsExpected::value && IsValueVoid::value>;

			explicit FunctionTask(F&& f, std::tuple<Args...>&& args)
				: F(std::move(f))
				, ArgsBase(std::move(args))
				, ExpectedReturn()
				, state_(State::None)
			{
			}

			virtual Status tick() override
			{
				if (state_ != State::None)
				{
					return Status::Failed;
				}

				state_ = State::Invoked;
				call_impl(IsApplyVoid());
				if (IsExpected())
				{
					return (get().has_value() ? Status::Successful : Status::Failed);
				}
				return Status::Successful;
			}

			virtual bool cancel() override
			{
				if (state_ == State::None)
				{
					// Canceled before tick()
					state_ = State::Canceled;
					return true;
				}
				return false;
			}

			virtual ExpectedReturn& get() override
			{
				return static_cast<ExpectedReturn&>(*this);
			}

			void call_impl(std::true_type/*f(...) is void*/)
			{
				std::apply(std::move(f()), std::move(args()));
			}

			void call_impl(std::false_type/*f(...) is NOT void*/)
			{
				get() = std::apply(std::move(f()), std::move(args()));
			}

			F& f()
			{
				return static_cast<F&>(*this);
			}

			std::tuple<Args...>& args()
			{
				return static_cast<std::tuple<Args...>&>(*this);
			}

		private:
			State state_;
			char alignment_[3];
		};

	} // namespace detail

	// #TODO: probably, if function returns T&, reference should not be discarded.
	// Looks like it depends on std::expected: if it supports references - they can 
	// be added easily.
	template<typename F, typename... Args>
	typename detail::FunctionTaskReturn<F
		, std::tuple<detail::remove_cvref_t<Args>...>>::type
			make_task(Scheduler& scheduler, F&& f, Args&&... args)
	{
		using Function = detail::remove_cvref_t<F>;
		using ArgsTuple = std::tuple<detail::remove_cvref_t<Args>...>;
		using FunctionTaskReturn = detail::FunctionTaskReturn<F, ArgsTuple>;
		using ReturnTask = typename FunctionTaskReturn::type;

		using Impl = detail::FunctionTask<
			FunctionTaskReturn
			, typename FunctionTaskReturn::custom_task
			, typename FunctionTaskReturn::value
			, Function
			, ArgsTuple>;
		
		auto task = std::make_unique<Impl>(std::forward<F>(f)
			, ArgsTuple(std::forward<Args>(args)...));

		return ReturnTask(scheduler, std::move(task));
	}

} // namespace nn
