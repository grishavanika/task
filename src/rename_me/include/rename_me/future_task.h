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
			explicit FutureTask(std::future<T>&& f)
				: result_()
				, future_(std::move(f))
			{
			}

			Status tick(bool cancel_requested)
			{
				// std::future<> can't be canceled
				(void)cancel_requested;

				if (!future_.valid())
				{
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
					result_ = unexpected<std::exception_ptr>(std::current_exception());
					return Status::Failed;
				}
			}

			expected<T, std::exception_ptr>& get()
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
