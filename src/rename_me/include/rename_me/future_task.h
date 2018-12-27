#pragma once
#include <rename_me/task.h>

#include <future>
#include <exception>
#include <chrono>

namespace nn
{
	namespace detail
	{
		template<typename T>
		class FutureTask : public ICustomTask<T, std::exception_ptr>
		{
		public:
			explicit FutureTask(std::future<T>&& f)
				: result_()
				, future_(std::move(f))
				, status_(Status::InProgress)
			{
			}

			virtual void tick() override
			{
				if (!future_.valid())
				{
					status_ = Status::Failed;
					return;
				}

				const auto state = future_.wait_for(std::chrono::microseconds(0));
				const bool in_progress = (state != std::future_status::ready);
				if (in_progress)
				{
					status_ = Status::InProgress;
					return;
				}

				// Get results
				try
				{
					result_ = future_.get();
					status_ = Status::Successful;
				}
				catch (...)
				{
					error_ = std::current_exception();
					status_ = Status::Failed;
				}
			}

			virtual Status status() const override
			{
				return status_;
			}

			virtual bool cancel() override
			{
				return false;
			}

		private:
			T result_;
			std::exception_ptr error_;
			std::future<T> future_;
			Status status_;
		};

	} // namespace detail

	// #TODO: support std::future<T&>. Depends on std::expected<T&, ...>.
	// See function_task.h
	template<typename T>
	auto make_task(Scheduler& scheduler, std::future<T>&& future)
	{
		auto task = std::make_unique<detail::FutureTask<T>>(std::move(future));
		return Task<T, std::exception_ptr>(scheduler, std::move(task));
	}

} // namespace nn
