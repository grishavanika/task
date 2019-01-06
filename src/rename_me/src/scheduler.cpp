#include <rename_me/scheduler.h>

#include <algorithm>

#include <cassert>

namespace nn
{
	namespace
	{
		using Lock = std::lock_guard<std::mutex>;
	} // namespace

	/*explicit*/ Scheduler::Scheduler()
		: tasks_()
		, tasks_count_(0)
		, tick_tasks_count_(0)
	{
	}

	void Scheduler::add(detail::ErasedTask task)
	{
		assert(task);
		Lock _(guard_);
		tasks_.push_back(std::move(task));
		tasks_count_ = tasks_.size();
	}

	std::size_t Scheduler::tasks_count() const
	{
		return (tasks_count_ + tick_tasks_count_);
	}

	bool Scheduler::has_tasks() const
	{
		return (tasks_count() > 0);
	}

	std::size_t Scheduler::poll(std::size_t tasks_count /*= 0*/)
	{
		std::size_t finished = 0;
		const bool has_limit = (tasks_count != 0);
		auto tasks = get_tasks();
		for (auto& task : tasks)
		{
			if (task->update() == Status::InProgress)
			{
				continue;
			}

			task = nullptr;
			++finished;
			if (has_limit && (finished == tasks_count))
			{
				break;
			}
		}
		add_tasks(std::move(tasks));
		return finished;
	}

	std::vector<detail::ErasedTask> Scheduler::get_tasks()
	{
		std::vector<detail::ErasedTask> tasks;
		Lock _(guard_);
		tasks = std::move(tasks_);
		tick_tasks_count_ = tasks.size();
		tasks_count_ = 0;
		return tasks;
	}

	void Scheduler::add_tasks(std::vector<detail::ErasedTask> tasks)
	{
		// Move in-progress tasks back
		auto it = std::remove_if(std::begin(tasks), std::end(tasks)
			, [](const detail::ErasedTask& task) { return !task; });

		Lock _(guard_);
		tasks_.reserve(tasks_.size() + tasks.size());
		tasks_.insert(std::end(tasks_)
			, std::make_move_iterator(std::begin(tasks))
			, std::make_move_iterator(it));
		tick_tasks_count_ = 0;
		tasks_count_ = tasks_.size();
	}

} // namespace nn
