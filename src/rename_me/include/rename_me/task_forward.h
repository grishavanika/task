#pragma once
#include <rename_me/noop_task.h>
#include <rename_me/detail/function_task_base.h>

#include <cassert>

namespace nn
{
	namespace detail
	{

		template<bool ErrorMatch, typename T, typename E, typename F, typename Return>
		struct InvokableErrorMatchImpl
		{
			using is_valid = std::true_type;
			using type = typename Return::type;

		};

		template<typename T, typename E, typename F, typename Return>
		struct InvokableErrorMatchImpl<false, T, E, F, Return>
		{
			using is_valid = std::false_type;
		};

		template<bool Valid, typename T, typename E, typename F, typename Return>
		struct InvokableImpl : InvokableErrorMatchImpl<
			std::is_same_v<E, typename Return::type::error_type>, T, E, F, Return>
		{
		};

		template<typename T, typename F, typename E, typename Return>
		struct InvokableImpl<false, T, F, E, Return>
		{
		};

		template<typename T, typename E, typename F
			, typename ReturnDiscard    = FunctionTaskReturn<F>
			, typename ReturnAccept     = FunctionTaskReturn<F, std::add_rvalue_reference_t<T>>
			, typename InvokableDiscard = InvokableImpl<ReturnDiscard::is_valid::value, T, E, F, ReturnDiscard>
			, typename InvokableAccept  = InvokableImpl<ReturnAccept::is_valid::value, T, E, F, ReturnAccept>>
		struct Invokable
			: InvokableDiscard
			, InvokableAccept
		{
#if (1)     // Disabling to minimize compile-time a bit.
			// Can be enabled any time
			static_assert((ReturnDiscard::is_valid::value
				&& ReturnAccept::is_valid::value) == false
				, "Either f() or f(T) should be valid, not both. Remove auto ?");

			using is_valid = std::disjunction<
				typename ReturnDiscard::is_valid
				, typename ReturnAccept::is_valid>;
#endif
		};

		template<typename T, typename E, typename F>
		using InvokableT = typename Invokable<T, E, F>::type;

	} // namespace detail

	// auto result = f()
	// auto result = f(T&&)
	//				 result can be: (1) void (2) Task<...> (3) expected<...>
	//				T can be void
	// using Error = typaneme result::error_type
	// assert(is_same<Error, E>
	// 

	// + Scheduler
	template<typename T, typename E, typename F>
	detail::InvokableT<T, E, F> forward_error(Task<T, E>&& root, F&& f)
	{
		return root.then([f = std::forward<F>(f)](const Task<T, E>& task)
		{
			if (task.is_failed())
			{
				assert(!task.get().has_value());
				return make_task(task.scheduler(), task.get_once());
			}
			return f();
		});
	}

} // namespace nn
