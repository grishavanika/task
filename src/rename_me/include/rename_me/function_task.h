#pragma once
#include <rename_me/task.h>
#include <rename_me/detail/cpp_20.h>

#include <functional>
#include <tuple>

#include <cassert>
#include <cstdint>

namespace nn
{
	namespace detail
	{
		template<typename R, typename F, typename ArgsTuple>
		class FunctionTask;

		template<typename T, typename F, typename... Args>
		class FunctionTask<T, F, std::tuple<Args...>> final
			: public ICustomTask<T, void>
			, private F
			, private std::tuple<Args...>
			, private expected<T, void>
		{
		public:
			enum class State : std::uint8_t
			{
				None,
				Invoked,
				Canceled,
			};

			explicit FunctionTask(F&& f, std::tuple<Args...>&& args)
				: F(std::move(f))
				, std::tuple<Args...>(std::move(args))
				, expected<T, void>()
				, state_(State::None)
			{
			}

			virtual Status tick() override
			{
				if (state_ == State::None)
				{
					state_ = State::Invoked;
					call_impl(std::is_same<T, void>());
					return Status::Successful;
				}
				return Status::Failed;
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

			virtual expected<T, void>& get() override
			{
				return static_cast<expected<T, void>&>(*this);
			}

			void call_impl(std::true_type/*void*/)
			{
				std::apply(std::move(f()), std::move(args()));
			}

			void call_impl(std::false_type/*NOT void*/)
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
	// #TODO: enable_if only f(args...) is valid expression
	// #TODO: return Task<T, E> if f(args...) returns expected<T, E>
	template<typename F, typename... Args>
	auto make_task(Scheduler& scheduler, F&& f, Args&&... args)
	{
		using Function = detail::remove_cvref_t<F>;
		using ArgsTuple = std::tuple<detail::remove_cvref_t<Args>...>;
		using R = detail::remove_cvref_t<decltype(
			std::apply(std::declval<Function>(), std::declval<ArgsTuple>()))>;
		using Impl = detail::FunctionTask<R, Function, ArgsTuple>;
		
		auto task = std::make_unique<Impl>(std::forward<F>(f)
			, ArgsTuple(std::forward<Args>(args)...));

		return Task<R, void>(scheduler, std::move(task));
	}

} // namespace nn
