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
		(void)sch.poll();
	}

	ASSERT_TRUE(task.is_successful());
	ASSERT_TRUE(task.get().has_value());
	ASSERT_EQ(short(3 * 2), task.get().value());
}

TEST(FutureTask, Maintains_Proper_Status_On_Success)
{
	Scheduler sch;
	std::promise<int> p;
	using FutureTask = Task<int, std::exception_ptr>;

	FutureTask task = make_task(sch, p.get_future());
	ASSERT_EQ(std::size_t(0), sch.poll());
	ASSERT_TRUE(task.is_in_progress());
	p.set_value(1);
	ASSERT_EQ(std::size_t(1), sch.poll());
	ASSERT_TRUE(task.is_successful());
	ASSERT_EQ(1, task.get().value());
}

TEST(FutureTask, Maintains_Proper_Status_On_Fail)
{
	Scheduler sch;
	std::promise<int> p;
	using FutureTask = Task<int, std::exception_ptr>;

	FutureTask task = make_task(sch, p.get_future());
	ASSERT_EQ(std::size_t(0), sch.poll());
	ASSERT_TRUE(task.is_in_progress());
	p.set_exception(std::make_exception_ptr(1));
	ASSERT_EQ(std::size_t(1), sch.poll());
	ASSERT_TRUE(task.is_failed());

	try
	{
		std::rethrow_exception(task.get().error());
	}
	catch (int e)
	{
		ASSERT_EQ(1, e);
	}
}
