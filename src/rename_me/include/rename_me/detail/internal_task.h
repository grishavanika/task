#pragma once
#include <rename_me/custom_task.h>

#include <memory>

#include <cassert>

namespace nn
{

	class Scheduler;

	namespace detail
	{

		class TaskBase
		{
		public:
			virtual ~TaskBase() = default;

			virtual Status tick() = 0;
		};

		template<typename T, typename E>
		class InternalTask : public TaskBase
		{
		public:
			explicit InternalTask(Scheduler& scheduler
				, std::unique_ptr<ICustomTask<T, E>> task);

			Scheduler& scheduler();

			virtual Status tick() override;
			void cancel();
			Status status() const;
			bool is_canceled() const;
			expected<T, E>& get();

		private:
			Scheduler& scheduler_;
			std::unique_ptr<ICustomTask<T, E>> task_;
			Status last_run_;
			bool canceled_;
			bool try_cancel_;
		};

	} // namespace detail
} // namespace nn


namespace nn
{
	namespace detail
	{

		template<typename T, typename E>
		/*explicit*/ InternalTask<T, E>::InternalTask(Scheduler& scheduler
			, std::unique_ptr<ICustomTask<T, E>> task)
			: scheduler_(scheduler)
			, task_(std::move(task))
			, last_run_(Status::InProgress)
			, canceled_(false)
			, try_cancel_(false)
		{
			assert(task_);
		}

		template<typename T, typename E>
		Scheduler& InternalTask<T, E>::scheduler()
		{
			return scheduler_;
		}

		template<typename T, typename E>
		Status InternalTask<T, E>::tick()
		{
			assert(last_run_ == Status::InProgress);
			if (!canceled_ && try_cancel_)
			{
				canceled_ = task_->cancel();
			}
			try_cancel_ = false;
			last_run_ = task_->tick();
			return last_run_;
		}

		template<typename T, typename E>
		Status InternalTask<T, E>::status() const
		{
			return last_run_;
		}

		template<typename T, typename E>
		void InternalTask<T, E>::cancel()
		{
			try_cancel_ = true;
		}

		template<typename T, typename E>
		bool InternalTask<T, E>::is_canceled() const
		{
			return canceled_;
		}

		template<typename T, typename E>
		expected<T, E>& InternalTask<T, E>::get()
		{
			assert(status() != Status::InProgress);
			return task_->get();
		}

	} // namespace detail
} // namespace nn
