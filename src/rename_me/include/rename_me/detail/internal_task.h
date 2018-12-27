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
			Status status() const;

		private:
			Scheduler& scheduler_;
			std::unique_ptr<ICustomTask<T, E>> task_;
			Status last_run_;
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
			task_->tick();
			last_run_ = task_->status();
			return last_run_;
		}

		template<typename T, typename E>
		Status InternalTask<T, E>::status() const
		{
			return last_run_;
		}

	} // namespace detail
} // namespace nn
