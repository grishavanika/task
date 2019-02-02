#include <gtest/gtest.h>
#include <rename_me/for_loop_task.h>

#include <memory>

#include "test_tools.h"

using namespace nn;

TEST(ForLoop, Simplest_Forever_Loop_Creation_With_Cancel)
{
	Scheduler sch;
	Task<> task1 = make_forever_loop_task<void>(sch);
	Task<> task2 = make_forever_loop_task(sch);
	Task<int, void> task3 = make_forever_loop_task<int>({sch, int()});
	Task<std::unique_ptr<int>, void> task4 =
		make_forever_loop_task(nn::Context<std::unique_ptr<int>>(sch, std::make_unique<int>()));
	Task<std::unique_ptr<int>, void> task5 =
		make_forever_loop_task(nn::make_context(sch, std::make_unique<int>()));
	Task<> task6 = make_forever_loop_task(nn::make_context(sch));

	(void)sch.poll();
	task1.try_cancel();
	task2.try_cancel();
	task3.try_cancel();
	task4.try_cancel();
	task5.try_cancel();
	task6.try_cancel();

	while (sch.has_tasks())
	{
		(void)sch.poll();
	}
}
