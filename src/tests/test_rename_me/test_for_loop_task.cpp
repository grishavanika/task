#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <rename_me/for_loop_task.h>

#include <memory>

#include "test_tools.h"

using namespace nn;

using ::testing::StrictMock;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::ByMove;

TEST(ForLoop, Simplest_Forever_Loop_Creation_With_Cancel)
{
	Scheduler sch;
	Task<> task1 = make_forever_loop_task<void>(sch);
	Task<> task2 = make_forever_loop_task(sch);
	Task<int, void> task3 = make_forever_loop_task<int>({sch, int()});
	Task<std::unique_ptr<int>, void> task4 =
		make_forever_loop_task(
			nn::LoopContext<std::unique_ptr<int>>(sch, std::make_unique<int>()));
	Task<std::unique_ptr<int>, void> task5 =
		make_forever_loop_task(
			nn::make_loop_context(sch, std::make_unique<int>()));
	Task<> task6 = make_forever_loop_task(nn::make_loop_context(sch));

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

TEST(ForLoop, One_Loop_States)
{
	Scheduler sch;

	auto create_task = [&]() -> Task<char>
	{
		return make_task(success, sch, 'y');
	};

	struct Listener
	{
		MOCK_METHOD2(on_before_create
			, bool (int context, std::size_t index));
		MOCK_METHOD2(on_create
			, Task<char> (int context, std::size_t index));
		MOCK_METHOD3(on_single_task_finish
			, bool (int context, std::size_t index, char payload));
		MOCK_METHOD3(on_finish
			, expected<int, void> (int context, std::size_t index, char payload));
	};
	StrictMock<Listener> listener;

	{
		InSequence one_loop_sequence;
		EXPECT_CALL(listener, on_before_create(752, 0u))
			.WillOnce(Return(true));
		EXPECT_CALL(listener, on_create(752, 0u))
			.WillOnce(Return(ByMove(create_task())));
		EXPECT_CALL(listener, on_single_task_finish(752, 0u, 'y'))
			.WillOnce(Return(false));
		EXPECT_CALL(listener, on_finish(752, 1u, 'y'))
			.WillOnce(Return(expected<int, void>(2)));
	}

	Task<int> task = make_for_loop_task(
		make_loop_context(sch, int(752))
		, [&listener](LoopContext<int>& context)
	{
		return listener.on_create(context.data(), context.index());
	}
		, [&listener](LoopContext<int>& context, const Task<char>& task)
	{
		assert(task.is_successful());
		return listener.on_single_task_finish(context.data()
			, context.index(), task.get().value());
	}
		, [&listener](LoopContext<int>& context)
	{
		return listener.on_before_create(context.data(), context.index());
	}
		, [&listener](LoopContext<int>& context, const Task<char>& last_task)
	{
		assert(last_task.is_successful());
		return listener.on_finish(context.data()
			, context.index(), last_task.get().value());
	});

	while (sch.has_tasks())
	{
		(void)sch.poll();
	}

	ASSERT_TRUE(task.is_successful());
	ASSERT_EQ(2, task.get().value());
}
