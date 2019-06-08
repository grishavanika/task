#pragma once
#include <rename_me/detail/internal_task.h>

#include <vector>
#include <mutex>
#include <atomic>

namespace nn
{

	class Scheduler
	{
	public:
		explicit Scheduler();
		~Scheduler();
		Scheduler(Scheduler&& rhs) = delete;
		Scheduler& operator=(Scheduler&& rhs) = delete;
		Scheduler(const Scheduler& rhs) = delete;
		Scheduler& operator=(const Scheduler& rhs) = delete;

		std::size_t poll(std::size_t tasks_count = 0);
		std::size_t tasks_count() const;
		bool has_tasks() const;

	private:
		template<typename D>
		friend class BaseTask;

		void post(detail::ErasedTask task);

		std::vector<detail::ErasedTask> get_tasks();
		void add_tasks(std::vector<detail::ErasedTask> tasks);

	private:
		std::mutex guard_;
		std::vector<detail::ErasedTask> tasks_;
		std::atomic<std::size_t> tasks_count_;
		std::atomic<std::size_t> tick_tasks_count_;
	};

} // namespace nn

