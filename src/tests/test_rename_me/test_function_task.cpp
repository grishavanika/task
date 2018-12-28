#include <gtest/gtest.h>
#include <rename_me/function_task.h>

using namespace nn;

namespace
{
	using detail::FunctionTask;
		
	using F = struct Empty { void operator()() const {} };
	using Args = std::tuple<>;
	using SmallestFunction = FunctionTask<void, F, Args>;

	struct MinimumFunction : ICustomTask<void, void>
	{
		SmallestFunction::State _;
	};

	static_assert(sizeof(SmallestFunction) == sizeof(MinimumFunction)
		, "detail::FunctionTask<> template should use EBO to have minimum "
		"possible size");
} // namespace

TEST(FunctionTask, Function_Is_Executed_Once)
{
	Scheduler sch;
	int calls_count = 0;

	{
		Task<void> task = make_task(sch
			, [&]
		{
			++calls_count;
		});
		// No call to tick() yet. Nothing was scheduled
		ASSERT_TRUE(task.is_in_progress());
		ASSERT_FALSE(task.is_finished());
		ASSERT_FALSE(task.is_canceled());
		ASSERT_EQ(Status::InProgress, task.status());
		ASSERT_EQ(0, calls_count);

		sch.tick();

		// Function was executed once
		ASSERT_FALSE(task.is_in_progress());
		ASSERT_TRUE(task.is_finished());
		ASSERT_FALSE(task.is_canceled());
		ASSERT_EQ(Status::Successful, task.status());
		ASSERT_EQ(1, calls_count);

		sch.tick();

		// No 2nd call, nothing was changed
		ASSERT_FALSE(task.is_in_progress());
		ASSERT_TRUE(task.is_finished());
		ASSERT_FALSE(task.is_canceled());
		ASSERT_EQ(Status::Successful, task.status());
		ASSERT_EQ(1, calls_count);
	}

	sch.tick();
}

TEST(FunctionTask, Can_Be_Canceled_Before_First_Call_To_Tick)
{
	Scheduler sch;
	int calls_count = 0;
	Task<void> task = make_task(sch, [&] { ++calls_count; });

	task.try_cancel();

	// No call to tick() yet. Nothing was scheduled
	ASSERT_FALSE(task.is_canceled());
	ASSERT_TRUE(task.is_in_progress());
	ASSERT_FALSE(task.is_finished());
	ASSERT_EQ(Status::InProgress, task.status());
	ASSERT_EQ(0, calls_count);

	sch.tick();

	// Function call was canceled
	ASSERT_TRUE(task.is_canceled());
	ASSERT_FALSE(task.is_in_progress());
	ASSERT_TRUE(task.is_finished());
	ASSERT_EQ(Status::Failed, task.status());
	ASSERT_EQ(0, calls_count);
}

TEST(FunctionTask, Cannot_Be_Canceled_After_Call_To_Tick)
{
	Scheduler sch;
	int calls_count = 0;
	Task<void> task = make_task(sch, [&] { ++calls_count; });

	sch.tick();
	task.try_cancel();

	// Function was executed once
	ASSERT_FALSE(task.is_canceled());
	ASSERT_TRUE(task.is_finished());
	ASSERT_EQ(1, calls_count);

	sch.tick();
	task.try_cancel();

	ASSERT_FALSE(task.is_canceled());
	ASSERT_TRUE(task.is_finished());
	ASSERT_EQ(1, calls_count);
}
