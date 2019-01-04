#pragma once
#include <rename_me/detail/internal_task.h>

#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

namespace nn
{

	namespace detail
	{
		class TaskBase;
	} // namespace detail

	class Scheduler
	{
	public:
		explicit Scheduler();
		Scheduler(Scheduler&& rhs) = delete;
		Scheduler& operator=(Scheduler&& rhs) = delete;
		Scheduler(const Scheduler& rhs) = delete;
		Scheduler& operator=(const Scheduler& rhs) = delete;

		std::size_t poll(std::size_t tasks_count = 0);
		std::size_t tasks_count() const;
		bool has_tasks() const;

	private:
		template<typename T, typename E>
		friend class Task;

		using TaskPtr = std::shared_ptr<detail::TaskBase>;

		void add(TaskPtr task);

		std::vector<TaskPtr> get_tasks();
		void add_tasks(std::vector<TaskPtr> tasks);

	private:
		std::mutex guard_;
		std::vector<TaskPtr> tasks_;
		std::atomic<std::size_t> tasks_count_;
		std::atomic<std::size_t> tick_tasks_count_;
	};

} // namespace nn

