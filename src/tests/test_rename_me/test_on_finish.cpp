#include <gtest/gtest.h>
#include <rename_me/task.h>
#include <rename_me/function_task.h>

using namespace nn;

TEST(OnFinish, Can_Be_Chained)
{
	Scheduler sch;
	Scheduler sch2;
	Task<int> task1 = make_task(sch
		, [&]
	{
		return 1;
	});

	Task<char> task2 = task1.on_finish(
		[](const Task<int>& task1)
	{
		[&]() { ASSERT_TRUE(task1.is_finished()); }();
		return 'x';
	});

	Task<int> task3 = task2.on_finish(sch2,
		[&sch](const Task<char>& task2)
	{
		[&]() { ASSERT_TRUE(task2.is_finished()); }();
		return make_task(sch
			, []
		{
			return 2;
		});
	});

	while (task3.is_in_progress())
	{
		sch.tick();
		sch2.tick();
	}

	ASSERT_EQ(&sch2, &task3.scheduler());
	ASSERT_TRUE(task1.is_successful());
	ASSERT_TRUE(task2.is_successful());
	ASSERT_TRUE(task3.is_successful());

	ASSERT_EQ(1, task1.get().value());
	ASSERT_EQ('x', task2.get().value());
	ASSERT_EQ(2, task3.get().value());
}
