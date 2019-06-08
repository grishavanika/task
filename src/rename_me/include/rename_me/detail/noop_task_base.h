#pragma once
#include <rename_me/detail/ebo_storage.h>
#include <rename_me/detail/cpp_20.h>
#include <rename_me/data_traits.h>

#include <utility>

#include <cassert>

namespace nn
{
	namespace detail
	{

		template<typename D>
		struct NN_EBO_CLASS NoopTask
			: private detail::EboStorage<D>
		{
			using Storage = detail::EboStorage<D>;
			using Traits = DataTraits<D>;

			// #TODO: disable when T is NoopTask
			template<typename T>
			explicit NoopTask(T&& v)
				: Storage(std::forward<T>(v))
			{
			}

			NoopTask(const NoopTask&) = delete;
			NoopTask(NoopTask&&) = delete;

			Status initial_status() const
			{
				D& data = Storage::get();
				return Traits::has_value(data)
					? Status::Successful : Status::Failed;
			}

			Status tick(const ExecutionContext& context)
			{
				assert(false && "tick() should never be called since "
					"Noop task has immediate non-inprogress state");
				(void)context;
				return Status::Canceled;
			}

			D& get()
			{
				return Storage::get();
			}
		};

	} // namespace detail
} // namespace nn

