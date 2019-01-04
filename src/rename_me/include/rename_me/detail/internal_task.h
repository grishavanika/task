#pragma once
#include <rename_me/custom_task.h>

#include <memory>
#include <atomic>

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

			virtual Status update() = 0;
		};

		template<typename T, typename E>
		class InternalTask : public TaskBase
		{
		public:
			virtual Scheduler& scheduler() = 0;
			virtual void cancel() = 0;
			virtual Status status() const = 0;
			virtual expected<T, E>& get_data() = 0;
		};

		template<typename T, typename E, typename CustomTask>
		class InternalCustomTask final
			: public InternalTask<T, E>
			, private CustomTask
		{
			static_assert(IsCustomTask<CustomTask, T, E>::value
				, "CustomTask should satisfy CustomTask<T, E> interface");
		public:
			template<typename... Args>
			explicit InternalCustomTask(Scheduler& scheduler, Args&&... args);

			CustomTask& task();

			virtual Scheduler& scheduler() override;
			virtual Status update() override;
			virtual void cancel() override;
			virtual Status status() const override;
			virtual expected<T, E>& get_data() override;

		private:
			Scheduler& scheduler_;
			// #TODO: looks like it's possible to have some other "packed"
			// representation for these flags to have single atomic<>.
			std::atomic<Status> last_run_;
			std::atomic_bool try_cancel_;
		};

	} // namespace detail
} // namespace nn


namespace nn
{
	namespace detail
	{

		template<typename T, typename E, typename CustomTask>
		template<typename... Args>
		/*explicit*/ InternalCustomTask<T, E, CustomTask>::InternalCustomTask(
			Scheduler& scheduler, Args&&... args)
			: CustomTask(std::forward<Args>(args)...)
			, scheduler_(scheduler)
			, last_run_(Status::InProgress)
			, try_cancel_(false)
		{
		}

		template<typename T, typename E, typename CustomTask>
		CustomTask& InternalCustomTask<T, E, CustomTask>::task()
		{
			return static_cast<CustomTask&>(*this);
		}

		template<typename T, typename E, typename CustomTask>
		Scheduler& InternalCustomTask<T, E, CustomTask>::scheduler()
		{
			return scheduler_;
		}

		template<typename T, typename E, typename CustomTask>
		Status InternalCustomTask<T, E, CustomTask>::update()
		{
			assert(last_run_ == Status::InProgress);
			const bool cancel_requested = try_cancel_;
			const Status status = task().tick(cancel_requested);
			try_cancel_ = false;
			last_run_ = status;
			return status;
		}

		template<typename T, typename E, typename CustomTask>
		Status InternalCustomTask<T, E, CustomTask>::status() const
		{
			return last_run_;
		}

		template<typename T, typename E, typename CustomTask>
		void InternalCustomTask<T, E, CustomTask>::cancel()
		{
			try_cancel_.store(true);
		}

		template<typename T, typename E, typename CustomTask>
		expected<T, E>& InternalCustomTask<T, E, CustomTask>::get_data()
		{
			assert(last_run_ != Status::InProgress);
			return task().get();
		}

	} // namespace detail
} // namespace nn
