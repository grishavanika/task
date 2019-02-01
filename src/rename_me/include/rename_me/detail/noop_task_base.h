#pragma once
#include <rename_me/detail/ebo_storage.h>
#include <rename_me/detail/cpp_20.h>

#include <utility>

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
} // namespace nn

