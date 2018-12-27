#pragma once
#include <rename_me/custom_task.h>

#include <memory>

namespace nn
{

	class Scheduler;

	namespace detail
	{
		template<typename T, typename E>
		class InternalTask;
	} // namespace detail

	template<typename T = void, typename E = void>
	class Task
	{
	public:
		using value_type = T;
		using error_type = E;

	public:
		explicit Task(Scheduler& scheduler, std::unique_ptr<ICustomTask<T, E>> task);
		~Task();
		Task(Task&& rhs);
		Task& operator=(Task&& rhs);
		Task(const Task& rhs) = delete;
		Task& operator=(const Task& rhs) = delete;

		void try_cancel();

		Status status() const;
		bool is_in_progress() const;
		bool is_finished() const;
		bool is_canceled() const;

	private:
		detail::InternalTask<T, E>* make_task(Scheduler& scheduler
			, std::unique_ptr<ICustomTask<T, E>> task);

		void remove();

	private:
		detail::InternalTask<T, E>* task_;
	};

} // namespace nn

#include <rename_me/scheduler.h>
#include <rename_me/detail/internal_task.h>

#include <utility>

namespace nn
{
	template<typename T, typename E>
	detail::InternalTask<T, E>* Task<T, E>::make_task(Scheduler& scheduler
		, std::unique_ptr<ICustomTask<T, E>> task)
	{
		auto impl = std::make_unique<detail::InternalTask<T, E>>(scheduler, std::move(task));
		auto* naked = impl.get();
		scheduler.add(std::move(impl));
		return naked;
	}

	template<typename T, typename E>
	/*explicit*/ Task<T, E>::Task(Scheduler& scheduler, std::unique_ptr<ICustomTask<T, E>> task)
		: task_(make_task(scheduler, std::move(task)))
	{
	}

	template<typename T, typename E>
	Task<T, E>::~Task()
	{
		remove();
	}

	template<typename T, typename E>
	void Task<T, E>::remove()
	{
		if (task_)
		{
			auto& scheduler = task_->scheduler();
			scheduler.remove(*task_);
			task_ = nullptr;
		}
	}

	template<typename T, typename E>
	Task<T, E>::Task(Task&& rhs)
		: task_(rhs.task_)
	{
		rhs.task_ = nullptr;
	}

	template<typename T, typename E>
	Task<T, E>& Task<T, E>::operator=(Task&& rhs)
	{
		if (this != &rhs)
		{
			remove();
			std::swap(task_, rhs.task_);
		}
		return *this;
	}

	template<typename T, typename E>
	Status Task<T, E>::status() const
	{
		return (task_ ? task_->status() : Status::InProgress);
	}

	template<typename T, typename E>
	bool Task<T, E>::is_in_progress() const
	{
		return (status() == Status::InProgress);
	}

	template<typename T, typename E>
	bool Task<T, E>::is_finished() const
	{
		return (status() == Status::Finished);
	}

	template<typename T, typename E>
	bool Task<T, E>::is_canceled() const
	{
		return (task_ ? task_->is_canceled() : false);
	}

	template<typename T, typename E>
	void Task<T, E>::try_cancel()
	{
		if (task_)
		{
			task_->cancel();
		}
	}
} // namespace nn
