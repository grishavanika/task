#pragma once
#include <rename_me/task.h>
#include <rename_me/detail/function_task_impl.h>

#include <tuple>

namespace nn
{
	// See Task<>::on_finish() for proper documentation of returned type.
	// 
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

		struct NN_EBO_CLASS Invoker
			: private detail::EBOFunctor<Function>
			, private ArgsTuple
		{
			using Callable = detail::EBOFunctor<Function>;

			explicit Invoker(Function f, ArgsTuple&& args)
				: Callable(std::move(f))
				, ArgsTuple(std::move(args))
			{
			}

			auto invoke()
			{
				Function& f = static_cast<Callable&>(*this).get();
				auto& args = static_cast<ArgsTuple&>(*this);
				return std::apply(std::move(f), std::move(args));
			}

			bool can_invoke() const
			{
				return true;
			}

			bool wait() const
			{
				return false;
			}
		};

		using FunctionTask = detail::FunctionTask<FunctionTaskReturn, Invoker>;
		return ReturnTask::template make<FunctionTask>(scheduler
			, Invoker(std::forward<F>(f), ArgsTuple(std::forward<Args>(args)...)));
	}

} // namespace nn
