#include <rename_me/scheduler.h>

#include <algorithm>

#include <cassert>

namespace nn
{

	/*explicit*/ Scheduler::Scheduler()
		: tasks_()
	{
	}

	void Scheduler::add(TaskPtr task)
	{
		assert(task);
		tasks_.push_back(std::move(task));
	}

	void Scheduler::remove(detail::TaskBase& task)
	{
		removed_.push_back(&task);
	}

	void Scheduler::tick()
	{
		auto tasks = std::move(tasks_);
		auto removed = std::move(removed_);
		std::vector<TaskPtr> finished = std::move(finished_);

		for (auto& task : tasks)
		{
			if (task->tick() == Status::Finished)
			{
				finished.push_back(std::move(task));
				task = nullptr;
			}
		}

		// Delete `removed` from `finished`

		{
			// Move in-progress tasks back
			auto it = std::remove_if(std::begin(tasks), std::end(tasks)
				, [](const TaskPtr& task)
			{
				return !task;
			});
			tasks_.assign(std::make_move_iterator(std::begin(tasks))
				, std::make_move_iterator(it));
		}
		{
			// Move removed tasks back
			auto it = std::remove_if(std::begin(removed), std::end(removed)
				, [](const detail::TaskBase* task)
			{
				return !task;
			});
			removed_.assign(std::begin(removed), it);
		}

		finished_ = std::move(finished);
	}

} // namespace nn
