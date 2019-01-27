#pragma once
#include <rename_me/task.h>
#include <rename_me/detail/ebo_storage.h>
#include <rename_me/detail/cpp_20.h>

#include <cassert>

namespace nn
{

	namespace detail
	{

		template<typename T, typename E>
		struct NN_EBO_CLASS NoopTask
			: private detail::EboStorage<expected<T, E>>
		{
			using Storage = detail::EboStorage<expected<T, E>>;

			template<typename Expected>
			explicit NoopTask(Expected&& v)
				: Storage(std::forward<Expected>(v))
			{
			}

			NoopTask(const NoopTask&) = delete;
			NoopTask(NoopTask&&) = delete;

			Status initial_status() const
			{
				return Storage::get().has_value()
					? Status::Successful : Status::Failed;
			}

			Status tick(bool cancel_requested)
			{
				assert(false);
				(void)cancel_requested;
				return Status::Canceled;
			}

			expected<T, E>& get()
			{
				return Storage::get();
			}
		};

	} // namespace detail

	struct SuccessTag {};
	constexpr SuccessTag success{};

	struct ErrorTag {};
	constexpr ErrorTag error{};

	template<typename T, typename E>
	Task<T, E> make_task(Scheduler& scheduler, expected<T, E>&& v)
	{
		return Task<T, E>::template make<detail::NoopTask<T, E>>(scheduler, std::move(v));
	}

	template<typename T, typename E>
	Task<T, E> make_task(Scheduler& scheduler, const expected<T, E>& v)
	{
		return Task<T, E>::template make<detail::NoopTask<T, E>>(scheduler, v);
	}

	template<typename T, typename E = void>
	auto make_task(SuccessTag, Scheduler& scheduler, T&& v)
	{
		static_assert(std::is_same_v<E, detail::remove_cvref_t<E>>,
			"Error should not be reference/const/volatile");

		using DerivedTask = Task<detail::remove_cvref_t<T>, E>;
		using Noop = detail::NoopTask<typename DerivedTask::value_type, E>;
		using Expected = typename DerivedTask::value;
		
		return DerivedTask::template make<Noop>(scheduler
			, Expected(std::forward<T>(v)));
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
	auto make_task(ErrorTag, Scheduler& scheduler, E&& v)
	{
		static_assert(std::is_same_v<T, detail::remove_cvref_t<T>>,
			"Value should not be reference/const/volatile");

		using DerivedTask = Task<T, detail::remove_cvref_t<E>>;
		using Noop = detail::NoopTask<T, typename DerivedTask::error_type>;
		using Expected = typename DerivedTask::value;
		using Unexpected = nn::unexpected<typename DerivedTask::error_type>;

		return DerivedTask::template make<Noop>(scheduler
			, Expected(Unexpected(std::forward<E>(v))));
	}

	template<typename T = void>
	auto make_task(ErrorTag, Scheduler& scheduler)
	{
		static_assert(std::is_same_v<T, detail::remove_cvref_t<T>>,
			"Value should not be reference/const/volatile");

		using DerivedTask = Task<T, void>;
		using Noop = detail::NoopTask<T, void>;
		using Expected = typename DerivedTask::value;
		using Unexpected = nn::unexpected<void>;

		return DerivedTask::template make<Noop>(scheduler
			, Expected(Unexpected()));
	}
} // namespace nn
