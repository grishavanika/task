#pragma once
#include <rename_me/data_traits.h>

#include <utility>
#include <type_traits>

#include <cstdint>

namespace nn
{
	class Scheduler;

	enum class Status : std::uint8_t
	{
		InProgress,
		Successful,
		Failed,
		Canceled,
	};

	// #TODO: template<typename D>
	struct ExecutionContext
	{
		Scheduler& scheduler;
		bool cancel_requested = false;
	};

	// CustomTask<D> interface
#if (0)
	template<typename D>
	struct CustomTask
	{
		// DataTraits<D> specialization exists

		// Invoked on Scheduler's thread.
		Status tick(const ExecutionContext& context);

		// Optional.
		// If initial_status() is not InProgress,
		// task is not posted to scheduler and is
		// immediately finished. In this case tick() will not
		// be invoked.
		// Can be useful for creating no-op (ready) tasks.
		Status initial_status() const;

		// #TODO: optional.
		// Initializes task's data to provided value
		// instead of default DataTraits<T>::make()
		// T initial_value();
	};
#endif

	namespace detail
	{
		template<typename T, typename U>
		struct VoidifySame { };

		template<typename T>
		struct VoidifySame<T, T> { using type = void; };
	} // namespace detail

	// CustomTask<T, E> is any class with:
	//  (1) Status tick(const ExecutionContext&)
	// member function(s)
	template<typename CustomTask, typename D>
	using CustomTaskInterface = std::void_t<
		typename DataTraits<D>::type	
		, typename detail::VoidifySame<
			Status, decltype(std::declval<CustomTask&>()
				.tick(std::declval<const ExecutionContext&>()))>::type>;

	template<typename CustomTask, typename D, typename = void>
	struct IsCustomTask
		: std::false_type { };

	template<typename CustomTask, typename D>
	struct IsCustomTask<CustomTask, D, CustomTaskInterface<CustomTask, D>>
		: std::true_type { };

	template<typename CustomTask>
	using InitialStatusInterface = std::void_t<
			typename detail::VoidifySame<
				Status, decltype(std::declval<const CustomTask&>()
					.initial_status())>::type>;

	template<typename CustomTask, typename = void>
	struct HasInitialStatus
		: std::false_type { };

	template<typename CustomTask>
	struct HasInitialStatus<CustomTask, InitialStatusInterface<CustomTask>>
		: std::true_type { };

} // namespace nn
