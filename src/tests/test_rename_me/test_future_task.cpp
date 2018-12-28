#include <gtest/gtest.h>
#include <rename_me/future_task.h>

using namespace nn;

TEST(FutureTask, Can_Be_Constructed_From_Async)
{
	Scheduler sch;
	Task<short> task = make_task(sch
		, std::async(std::launch::async, []()
	{
		return 2;
	}))
		.on_finish([](const Task<int, std::exception_ptr>& task)
	{
		[&task]
		{
			ASSERT_TRUE(task.is_successful());
			ASSERT_TRUE(task.get().has_value());
		}();
		return short(3 * task.get().value());
	});

	while (task.is_in_progress())
	{
		sch.tick();
	}

	ASSERT_TRUE(task.is_successful());
	ASSERT_TRUE(task.get().has_value());
	ASSERT_EQ(short(3 * 2), task.get().value());
}
