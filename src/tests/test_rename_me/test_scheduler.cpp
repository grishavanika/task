#include <gtest/gtest.h>
#include <rename_me/scheduler.h>
#include <rename_me/function_task.h>

#include <thread>

using namespace nn;

TEST(Scheduler, Default_Constructed_Has_No_Tasks)
{
	Scheduler sch;
	ASSERT_EQ(0, sch.tasks_count());
	ASSERT_FALSE(sch.has_tasks());

	sch.tick();
	ASSERT_EQ(0, sch.tasks_count());
	ASSERT_FALSE(sch.has_tasks());
}

TEST(Scheduler, Single_Task_Adds_One_Task_To_Scheduler)
{
	Scheduler sch;
	(void)make_task(sch, [] {});
	ASSERT_EQ(1, sch.tasks_count());
	ASSERT_TRUE(sch.has_tasks());

	sch.tick();
	ASSERT_EQ(0, sch.tasks_count());
	ASSERT_FALSE(sch.has_tasks());
}

TEST(Scheduler, Task_On_Finish_Adds_One_Task_To_Scheduler)
{
	Scheduler sch;
	auto task = make_task(sch, [] {});
	sch.tick();
	ASSERT_EQ(0, sch.tasks_count());
	ASSERT_FALSE(sch.has_tasks());

	(void)task.on_finish([](const Task<>&) {});
	ASSERT_EQ(1, sch.tasks_count());
	ASSERT_TRUE(sch.has_tasks());
	sch.tick();
	ASSERT_EQ(0, sch.tasks_count());
	ASSERT_FALSE(sch.has_tasks());
}

TEST(Scheduler, Executes_Task_On_Schedulers_Thread)
{
	Scheduler sch;

	Task<std::thread::id> task = make_task(sch
		, [] { return std::this_thread::get_id(); });

	std::thread worker(&Scheduler::tick, &sch);
	while (task.is_in_progress());
	ASSERT_EQ(worker.get_id(), task.get().value());
	worker.join();
}

TEST(Scheduler, Executes_On_Finish_On_Given_Scheduler)
{
	Scheduler sch;
	Scheduler finish_sch;

	Task<std::thread::id> task = make_task(sch
		, []
	{
		return std::this_thread::get_id();
	});

	Task<std::thread::id> finish_task = task.on_finish(finish_sch
		, [](const Task<std::thread::id>&)
	{
		return std::this_thread::get_id();
	});

	std::thread worker1(&Scheduler::tick, &sch);
	while (task.is_in_progress());
	ASSERT_EQ(worker1.get_id(), task.get().value());
	worker1.join();

	ASSERT_EQ(1, finish_sch.tasks_count());
	std::thread worker2(&Scheduler::tick, &finish_sch);
	while (finish_task.is_in_progress());
	ASSERT_EQ(worker2.get_id(), finish_task.get().value());
	worker2.join();
}
