#pragma once
#include <rename_me/task.h>

#include <future>
#include <exception>
#include <chrono>

namespace nn
{
	namespace detail
	{
		// #TODO: do Task<T, void> if exceptions are disabled
		template<typename T>
		class FutureTask
		{
		public:
			using expected_type = expected<T, std::exception_ptr>;

			explicit FutureTask(std::future<T>&& f)
				: result_()
				, future_(std::move(f))
			{
			}

			Status tick(const ExecutionContext& context)
			{
				// std::future<> can't be canceled
				(void)context.cancel_requested;

				if (!future_.valid())
				{
					result_ = MakeExpectedWithDefaultError<expected_type>();
					return Status::Failed;
				}

				const auto state = future_.wait_for(std::chrono::microseconds(0));
				const bool in_progress = (state != std::future_status::ready);
				if (in_progress)
				{
					return Status::InProgress;
				}

				// Get results
				try
				{
					result_ = future_.get();
					return Status::Successful;
				}
				catch (...)
				{
					SetExpectedWithError(result_, std::current_exception());
					return Status::Failed;
				}
			}

			expected_type& get()
			{
				return result_;
			}

		private:
			expected<T, std::exception_ptr> result_;
			std::future<T> future_;
		};

	} // namespace detail

	// #TODO: support std::future<T&>. Depends on std::expected<T&, ...>.
	// See function_task.h
	template<typename T>
	auto make_task(Scheduler& scheduler, std::future<T>&& future)
	{
		using Task = Task<T, std::exception_ptr>;
		return Task::template make<detail::FutureTask<T>>(
			scheduler, std::move(future));
	}

} // namespace nn
