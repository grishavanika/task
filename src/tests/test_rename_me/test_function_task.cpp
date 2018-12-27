#include <gtest/gtest.h>
#include <rename_me/function_task.h>

using namespace nn;

TEST(FunctionTask, XXX)
{
	Scheduler sch;
	int calls_count = 0;

	{
		Task<int, void> task = make_task(sch
			, [&]
		{
			++calls_count;
			return 1;
		});

		ASSERT_TRUE(task.is_in_progress());
		ASSERT_FALSE(task.is_finished());
		ASSERT_EQ(Status::InProgress, task.status());
		ASSERT_EQ(0, calls_count);

		sch.tick();

		ASSERT_FALSE(task.is_in_progress());
		ASSERT_TRUE(task.is_finished());
		ASSERT_EQ(Status::Finished, task.status());
		ASSERT_EQ(1, calls_count);

		sch.tick();

		ASSERT_FALSE(task.is_in_progress());
		ASSERT_TRUE(task.is_finished());
		ASSERT_EQ(Status::Finished, task.status());
		ASSERT_EQ(1, calls_count);
	}

	sch.tick();
}
