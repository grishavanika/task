#pragma once
#include <rename_me/expected.h>

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

	struct ExecutionContext
	{
		Scheduler& scheduler;
		bool cancel_requested = false;
	};

	// CustomTask<T, E> interface
#if (0)
	template<typename T, typename E>
	struct CustomTask
	{
		// Invoked on Scheduler's thread.
		Status tick(const ExecutionContext& context);
		// To be thread-safe, get() value needs to be set before
		// finish status returned from tick().
		// 
		// It's expected for get() to return expected with
		// (1) has_value() == true when last tick was _Successful_
		// (2) has_value() == false (e.g, error is valid) when _Failed OR Canceled_
		expected<T, E>& get();
		// Optional.
		// If initial_status() is not InProgress,
		// task is not posted to scheduler and is
		// immediately finished. In this case tick() will not
		// be invoked.
		// Can be useful for creating no-op (ready) tasks.
		Status initial_status() const;
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
	//  (2) expected<T, E>& get()
	// member functions
	template<typename CustomTask, typename T, typename E>
	using CustomTaskInterface = std::void_t<
			typename detail::VoidifySame<
				Status, decltype(std::declval<CustomTask&>()
					.tick(std::declval<const ExecutionContext&>()))>::type
			, typename detail::VoidifySame<
				expected<T, E>&, decltype(std::declval<CustomTask&>()
					.get())>::type>;

	template<typename CustomTask, typename T, typename E
		, typename = void>
	struct IsCustomTask
		: std::false_type
	{
	};

	template<typename CustomTask, typename T, typename E>
	struct IsCustomTask<CustomTask, T, E
		, CustomTaskInterface<CustomTask, T, E>>
		: std::true_type
	{
	};

	template<typename CustomTask>
	using InitialStatusInterface = std::void_t<
			typename detail::VoidifySame<
				Status, decltype(std::declval<const CustomTask&>()
					.initial_status())>::type>;

	template<typename CustomTask, typename = void>
	struct HasInitialStatus
		: std::false_type
	{
	};

	template<typename CustomTask>
	struct HasInitialStatus<CustomTask
		, InitialStatusInterface<CustomTask>>
		: std::true_type
	{
	};

} // namespace nn
