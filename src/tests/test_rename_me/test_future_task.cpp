#include <gtest/gtest.h>
#include <rename_me/future_task.h>

using namespace nn;

TEST(FutureTask, Can_Be_Constructed_From_Async)
{
	Scheduler sch;
	Task<int, std::exception_ptr> task = make_task(sch
		, std::async(std::launch::async, []()
	{
		return 1;
	}));

	while (task.is_in_progress())
	{
		sch.tick();
	}
}
