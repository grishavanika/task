#pragma once
#include <rename_me/task.h>
#include <rename_me/scheduler.h>
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
		class FunctionTask<T, F, std::tuple<Args...>> : public ICustomTask<T, void>
		{
			enum class State : std::uint8_t
			{
				None,
				Invoked,
				Canceled,
			};

		public:
			explicit FunctionTask(F&& f, std::tuple<Args...>&& args)
				: result_()
				, f_(std::move(f))
				, args_(std::move(args))
				, state_(State::None)
			{
			}

			virtual void tick() override
			{
				if (state_ == State::None)
				{
					state_ = State::Invoked;
					result_ = std::apply(std::move(f_), std::move(args_));
				}
			}

			virtual Status status() const override
			{
				assert(state_ != State::None);
				return (state_ == State::Invoked)
					? Status::Successful
					: Status::Failed;
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


		private:
			// #TODO: aligned_storage for non-defalt-constructiable things
			T result_;
			F f_;
			std::tuple<Args...> args_;
			State state_;
		};

	} // namespace detail

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
