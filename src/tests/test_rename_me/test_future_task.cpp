#include <gtest/gtest.h>
#include <rename_me/future_task.h>

using namespace nn;

TEST(FutureTask, Can_Be_Constructed_From_Async)
{
	Scheduler sch;
	Task<short> task = make_task(sch
		, std::async(std::launch::async, []()
	{
		return 1;
	}))
		.on_finish([](const Task<int, std::exception_ptr>&)
	{
		return short(2);
	});

	while (task.is_in_progress())
	{
		sch.tick();
	}
}
