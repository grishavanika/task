#pragma once
#include <rename_me/noop_task.h>
#include <rename_me/detail/function_task_base.h>

#include <cassert>

namespace nn
{
	namespace detail
	{

		template<typename Return, typename T, typename E>
		typename Return::type InvokeError(Scheduler& scheduler, const Task<T, E>& task)
		{
			using type = typename Return::type;
			using Noop = detail::NoopTask<typename type::value_type, typename type::error_type>;
			using Expected = typename type::value;

			return type::template make<Noop>(scheduler
				, MakeExpectedWithError<Expected>(std::move(task.get().error())));
		}

		template<typename Return, typename T>
		typename Return::type InvokeError(Scheduler& scheduler, const Task<T, void>& task)
		{
			(void)task;

			using type = typename Return::type;
			using Noop = detail::NoopTask<typename type::value_type, void>;
			using Expected = typename type::value;

			return type::template make<Noop>(scheduler
				, MakeExpectedWithDefaultError<Expected>());
		}

		template<typename Return>
		struct CallerAccept
		{
			using type = typename Return::type;

			template<typename T, typename E, typename F>
			static type invoke(Scheduler& scheduler, const Task<T, E>& task, F&& f)
			{
				return Return::template invoke(scheduler
					, std::forward<F>(f), std::move(task.get_once().value()));
			}

			template<typename E, typename F>
			static type invoke(Scheduler& scheduler, const Task<void, E>& task, F&& f)
			{
				return Return::template invoke(scheduler
					, std::forward<F>(f)/*void*/);
			}
		};

		template<typename Return>
		struct CallerDiscard
		{
			using type = typename Return::type;

			template<typename T, typename E, typename F>
			static type invoke(Scheduler& scheduler, const Task<T, E>& task, F&& f)
			{
				(void)task;
				return Return::template invoke(scheduler
					, std::forward<F>(f)/*void*/);
			}
		};

		// using Task = F(T)
		// expects: Task::error_type == E
		template<bool ErrorMatch, typename T, typename E, typename F
			, typename Return, typename Caller>
		struct InvokableErrorMatchImpl
		{
			using is_valid = std::true_type;
			using type = typename Return::type;

			static type invoke(Scheduler& scheduler, const Task<T, E>& task, F&& f)
			{
				assert(task.is_finished());
				if (task.is_successful())
				{
					assert(task.get().has_value());
					return Caller::template invoke(scheduler, task, std::forward<F>(f));
				}

				assert(!task.get().has_value());
				(void)f;
				return InvokeError<Return>(scheduler, task);
			}
		};

		template<typename T, typename E, typename F
			, typename Return, typename Caller>
		struct InvokableErrorMatchImpl<false, T, E, F, Return, Caller>
		{
			// Return type from F(Arg) does not match E
			using is_valid = std::false_type;
		};

		template<bool Valid, typename T, typename E, typename F
			, typename Return, typename Caller>
		struct InvokableImpl : InvokableErrorMatchImpl<
			std::is_same_v<E, typename Return::type::error_type>, T, E, F, Return, Caller>
		{
		};

		template<typename T, typename E, typename F
			, typename Return, typename Caller>
		struct InvokableImpl<false, T, E, F, Return, Caller>
		{
			// F(Arg)/F() is not valid
			using is_valid = std::false_type;
		};

		template<typename T, typename E, typename F
			// Functor (F) discards successful data (T)
			, typename ReturnDiscard    = FunctionTaskReturn<F>
			// Functor (F) accepts successful data (T&&)
			, typename ReturnAccept     = FunctionTaskReturn<F, std::add_rvalue_reference_t<T>>
			, typename InvokableAccept  = InvokableImpl<ReturnAccept::is_valid::value, T, E, F, ReturnAccept, CallerAccept<ReturnAccept>>
			, typename InvokableDiscard = InvokableImpl<ReturnDiscard::is_valid::value, T, E, F, ReturnDiscard, CallerDiscard<ReturnDiscard>>>
		struct Invokable
			: InvokableDiscard
			, InvokableAccept
		{
			static_assert((InvokableDiscard::is_valid::value
				&& InvokableAccept::is_valid::value) == false
				, "Either f() or f(T) should be valid, not both. Remove auto ?");

			using is_valid = std::disjunction<
				typename InvokableDiscard::is_valid
				, typename InvokableAccept::is_valid>;
		};

		template<typename T, typename E, typename F>
		using InvokableT = typename Invokable<T, E, F>::type;

	} // namespace detail

	// #TODO: + Scheduler
	// #TODO: + transform_error ?
	template<typename T, typename E, typename F>
	detail::InvokableT<T, E, F> forward_error(Task<T, E>&& root, F&& f)
	{
		using Invokable = detail::Invokable<T, E, F>;
		Scheduler* scheduler = &root.scheduler();
		return root.then(*scheduler
			, [scheduler, f = std::forward<F>(f)]
				(const Task<T, E>& task) mutable
		{
			return Invokable::invoke(*scheduler, task
				, std::forward<decltype(f)>(f));
		});
	}

} // namespace nn
