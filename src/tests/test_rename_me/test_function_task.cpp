#include <gtest/gtest.h>
#include <rename_me/function_task.h>

using namespace nn;

namespace
{

	// Check that returned type has proper Task<> depending on what
	// functor returns
	template<typename T, typename F>
	struct ReturnCheck : std::is_same<T
		, decltype(make_task(
			std::declval<Scheduler&>(), std::declval<F>()))>
	{
	};

	static_assert(ReturnCheck<
		Task<void, void>
		, void (*)()>::value
		, "Task<void> should be returned when using void f() callback");

	static_assert(ReturnCheck<
		Task<int, void>
		, int (*)()>::value
		, "Task<int> should be returned when using int f() callback");

	static_assert(ReturnCheck<
		Task<int, int>
		, expected<int, int> (*)()>::value
		, "Task<int, int> should be returned when using expected<int, int> f() callback");

	static_assert(ReturnCheck<
		Task<char, char>
		, Task<char, char> (*)()>::value
		, "Task<char, charint> should be returned when using Task<char, char> f() callback");

	// Reference returns are decay-ed. At least now
	static_assert(ReturnCheck<
		Task<int>
		, int& (*)()>::value
		, "Task<int> should be returned when using int& f() callback");

	static_assert(ReturnCheck<
		Task<int, int>
		, expected<int, int>& (*)()>::value
		, "Task<int, int> should be returned when using expected<int, int>& f() callback");

	static_assert(ReturnCheck<
		Task<char, char>
		, Task<char, char>& (*)()>::value
		, "Task<char, charint> should be returned when using Task<char, char>& f() callback");

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

TEST(FunctionTask, Move_Only_Return)
{
	Scheduler sch;
	Task<std::unique_ptr<int>> task = make_task(sch
		, []
	{
		return std::make_unique<int>(11);
	});
	sch.tick();
	ASSERT_TRUE(task.is_successful());
	ASSERT_TRUE(task.get().has_value());
	ASSERT_NE(nullptr, task.get().value());
	ASSERT_EQ(11, *task.get_once().value());
}

TEST(FunctionTask, Failed_Expected_Return_Fails_Task)
{
	Scheduler sch;
	Task<int, int> task = make_task(sch, 
		[]
	{
		// Fail task
		using error = ::nn::unexpected<int>;
		return expected<int, int>(error(1));
	});

	sch.tick();
	ASSERT_TRUE(task.is_failed());
	ASSERT_FALSE(task.get().has_value());
	ASSERT_EQ(1, task.get().error());
}

TEST(FunctionTask, Successful_Expected_Returns_Task_With_Success)
{
	Scheduler sch;
	Task<int, int> task = make_task(sch,
		[]
	{
		// Ok
		return expected<int, int>(2);
	});

	sch.tick();
	ASSERT_TRUE(task.is_successful());
	ASSERT_TRUE(task.get().has_value());
	ASSERT_EQ(2, task.get().value());
}

TEST(FunctionTask, Forwards_Args_To_The_Functor)
{
	Scheduler sch;
	Task<std::unique_ptr<int>> task = make_task(sch,
		[](std::unique_ptr<int> p)
	{
		return p;
	}
		, std::make_unique<int>(42));

	sch.tick();
	ASSERT_TRUE(task.is_successful());
	ASSERT_TRUE(task.get().has_value());
	ASSERT_NE(nullptr, task.get().value());
	ASSERT_EQ(42, *task.get().value());
}

TEST(FunctionTask, Failed_Task_Return_Fails_Task)
{
	Scheduler sch;
	Task<int, int> task = make_task(sch
		, [&sch]
	{
		return make_task(sch, []
		{
			// Fail task
			using error = ::nn::unexpected<int>;
			return expected<int, int>(error(1));
		});
	});

	sch.tick();
	sch.tick();
	ASSERT_TRUE(task.is_failed());
	ASSERT_FALSE(task.get().has_value());
	ASSERT_EQ(1, task.get().error());
}

TEST(FunctionTask, Successful_Task_Returns_Task_With_Success)
{
	Scheduler sch;
	Task<int, int> task = make_task(sch,
		[&sch]
	{
		return make_task(sch
			, []
		{
			// Ok
			return expected<int, int>(2);
		});
	});

	sch.tick();
	sch.tick();
	ASSERT_TRUE(task.is_successful());
	ASSERT_TRUE(task.get().has_value());
	ASSERT_EQ(2, task.get().value());
}
