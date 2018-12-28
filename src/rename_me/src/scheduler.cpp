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
	{
	}

	void Scheduler::add(TaskPtr task)
	{
		assert(task);
		Lock _(guard_);
		tasks_.push_back(std::move(task));
	}

	void Scheduler::tick()
	{
		std::vector<TaskPtr> tasks;
		{
			Lock _(guard_);
			tasks = std::move(tasks_);
		}

		for (auto& task : tasks)
		{
			if (task->tick() != Status::InProgress)
			{
				task = nullptr;
			}
		}

		{
			// Move in-progress tasks back
			auto it = std::remove_if(std::begin(tasks), std::end(tasks)
				, [](const TaskPtr& task) { return !task; });
			tasks.erase(it, std::end(tasks));

			Lock _(guard_);
			tasks_ = std::move(tasks);
		}
	}

} // namespace nn
