#include <rename_me/future_task.h>
#include <chrono>
#include <cstdio>
#include <cassert>

nn::Task<int> DoWork(nn::Scheduler& scheduler, int input)
{
	auto task = make_task(scheduler
		// Run std::async() and wrap std::future<> into Task<>
		, std::async(std::launch::async, [=]()
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		return (2 * input);
	}))
		// When std::async() completes
		.then([](const nn::Task<int, std::exception_ptr>& task)
	{
		// There was no exception
		assert(task.is_successful());
		return (3 * task.get().value());
	});

	// When our `task` completes. Ignore any values, just print something
	(void)task.then([]
	{
		std::puts("Completed");
	});
	return task;
}

int main()
{
	nn::Scheduler scheduler;
	nn::Task<int> task = DoWork(scheduler, 10);
	while (task.is_in_progress())
	{
		scheduler.poll();
	}
	return task.get().value(); // Returns 10 * 2 * 3 = 60
}
