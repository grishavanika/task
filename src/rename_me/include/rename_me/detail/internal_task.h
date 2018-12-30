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
			State state() const;
			bool is_canceled() const;
			expected<T, E>& get();

		private:
			Scheduler& scheduler_;
			std::unique_ptr<ICustomTask<T, E>> task_;
			// #TODO: looks like it's possible to have some other "packed"
			// representation for these flags to have single atomic<>.
			// #TODO: looks like `last_run_` and `canceled_` (e.g., state())
			// should be glued together to guaranty nicer thread-safety/consistency stuff
			std::atomic<Status> last_run_;
			std::atomic_bool canceled_;
			std::atomic_bool try_cancel_;
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
			const bool cancel_requested = try_cancel_;
			try_cancel_ = false;
			const State state = task_->tick(cancel_requested);
			canceled_ = state.canceled;
			last_run_ = state.status;
			return last_run_;
		}

		template<typename T, typename E>
		State InternalTask<T, E>::state() const
		{
			return State(status(), is_canceled());
		}

		template<typename T, typename E>
		Status InternalTask<T, E>::status() const
		{
			return last_run_.load();
		}

		template<typename T, typename E>
		void InternalTask<T, E>::cancel()
		{
			try_cancel_.store(true);
		}

		template<typename T, typename E>
		bool InternalTask<T, E>::is_canceled() const
		{
			return canceled_.load();
		}

		template<typename T, typename E>
		expected<T, E>& InternalTask<T, E>::get()
		{
			assert(status() != Status::InProgress);
			return task_->get();
		}

	} // namespace detail
} // namespace nn
