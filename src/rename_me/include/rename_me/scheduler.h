#pragma once
#include <rename_me/detail/internal_task.h>

#include <vector>
#include <memory>
#include <mutex>

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

		using TaskPtr = std::shared_ptr<detail::TaskBase>;

		void add(TaskPtr task);

	private:
		std::mutex guard_;
		std::vector<TaskPtr> tasks_;
	};

} // namespace nn

