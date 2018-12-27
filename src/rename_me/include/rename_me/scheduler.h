#pragma once
#include <rename_me/detail/internal_task.h>

#include <vector>
#include <memory>

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

		void tick();

	private:
		template<typename T, typename E>
		friend class Task;

		using TaskPtr = std::unique_ptr<detail::TaskBase>;

		void add(TaskPtr task);
		void remove(detail::TaskBase& task);

	private:
		std::vector<TaskPtr> tasks_;
		std::vector<TaskPtr> finished_;
		std::vector<const detail::TaskBase*> removed_;
	};

} // namespace nn

