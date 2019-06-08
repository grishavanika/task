#pragma once
#include <rename_me/base_task.h>
#include <rename_me/detail/cpp_20.h>
#include <rename_me/detail/noop_task_base.h>

namespace nn
{

	struct SuccessTag {};
	constexpr SuccessTag success{};

	struct ErrorTag {};
	constexpr ErrorTag error{};

	template<typename D>
	BaseTask<D> make_task(Scheduler& scheduler, D&& v)
	{
		using Unref = detail::remove_cvref<D>
		return BaseTask<D>::template make<detail::NoopTask<Unref>>(
			scheduler, std::forward<D>(v));
	}

	template<typename T, typename E = void>
	auto make_task(SuccessTag, Scheduler& scheduler, T v)
	{
		static_assert(std::is_same_v<E, detail::remove_cvref_t<E>>,
			"Error should not be reference/const/volatile");

		using DerivedTask = Task<detail::remove_cvref_t<T>, E>;
		using Noop = detail::NoopTask<typename DerivedTask::value_type, E>;
		using Expected = typename DerivedTask::value;
		
		return DerivedTask::template make<Noop>(scheduler
			, Expected(std::move(v)));
	}

	template<typename E = void>
	auto make_task(SuccessTag, Scheduler& scheduler)
	{
		static_assert(std::is_same_v<E, detail::remove_cvref_t<E>>,
			"Error should not be reference/const/volatile");

		using DerivedTask = Task<void, E>;
		using Noop = detail::NoopTask<void, E>;
		using Expected = typename DerivedTask::value;

		return DerivedTask::template make<Noop>(scheduler
			, Expected());
	}

	template<typename E, typename T = void>
	auto make_task(ErrorTag, Scheduler& scheduler, E v)
	{
		static_assert(std::is_same_v<T, detail::remove_cvref_t<T>>,
			"Value should not be reference/const/volatile");

		using DerivedTask = Task<T, detail::remove_cvref_t<E>>;
		using Noop = detail::NoopTask<T, typename DerivedTask::error_type>;
		using Expected = typename DerivedTask::value;

		return DerivedTask::template make<Noop>(scheduler
			, MakeExpectedWithError<Expected>(std::move(v)));
	}

	template<typename T = void>
	auto make_task(ErrorTag, Scheduler& scheduler)
	{
		static_assert(std::is_same_v<T, detail::remove_cvref_t<T>>,
			"Value should not be reference/const/volatile");

		using DerivedTask = Task<T, void>;
		using Noop = detail::NoopTask<T, void>;
		using Expected = typename DerivedTask::value;

		return DerivedTask::template make<Noop>(scheduler
			, Expected());
	}
	
	template<>
	inline auto make_task<void>(ErrorTag, Scheduler& scheduler)
	{
		using DerivedTask = Task<void, void>;
		using Noop = detail::NoopTask<void, void>;
		using Expected = typename DerivedTask::value;

		return DerivedTask::template make<Noop>(scheduler
			, MakeExpectedWithDefaultError<Expected>());
	}

} // namespace nn
