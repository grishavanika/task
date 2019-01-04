#include <gtest/gtest.h>
#include <rename_me/function_task.h>

using namespace nn;

namespace
{

	// Check that returned type has proper Task<> depending on what
	// functor returns
	template<typename R, typename ExpectedTask>
	struct ReturnCheck
	{
		static R functor();

		using IsSame = std::is_same<ExpectedTask
			, decltype(make_task(
				std::declval<Scheduler&>(), &ReturnCheck::functor))>;

		static constexpr bool value = IsSame::value;
	};

	static_assert(ReturnCheck<void, Task<void, void>>::value
		, "Task<void> should be returned when using void f() callback");

	static_assert(ReturnCheck<int, Task<int, void>>::value
		, "Task<int> should be returned when using int f() callback");

	static_assert(ReturnCheck<expected<int, int>, Task<int, int>>::value
		, "Task<int, int> should be returned when using expected<int, int> f() callback");

	static_assert(ReturnCheck<Task<char, char>, Task<char, char>>::value
		, "Task<char, char> should be returned when using Task<char, char> f() callback");

	// Reference returns are decay-ed. At least now
	static_assert(ReturnCheck<int&, Task<int>>::value
		, "Task<int> should be returned when using int& f() callback");

	static_assert(ReturnCheck<expected<int, int>&, Task<int, int>>::value
		, "Task<int, int> should be returned when using expected<int, int>& f() callback");

	static_assert(ReturnCheck<Task<char, char>&, Task<char, char>>::value
		, "Task<char, charint> should be returned when using Task<char, char>& f() callback");

	struct TestTask : ICustomTask<void, void>
	{
		explicit TestTask(Status& status, bool& canceled)
			: status_(status)
			, canceled_(canceled)
		{
		}

		virtual State tick(bool cancel_requested) override
		{
			if (cancel_requested)
			{
				canceled_ = true;
				return State(Status::Failed, canceled_);
			}
			return status_;
		}

		virtual expected<void, void>& get() override
		{
			return dummy_;
		}

		Status& status_;
		bool& canceled_;
		expected<void, void> dummy_;
	};

	Task<> make_test_task(Scheduler& sch, Status& status, bool& canceled)
	{
		return Task<>::template make<TestTask>(sch, status, canceled);
	}

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

		ASSERT_EQ(std::size_t(1), sch.poll());

		// Function was executed once
		ASSERT_FALSE(task.is_in_progress());
		ASSERT_TRUE(task.is_finished());
		ASSERT_FALSE(task.is_canceled());
		ASSERT_EQ(Status::Successful, task.status());
		ASSERT_EQ(1, calls_count);

		ASSERT_EQ(std::size_t(0), sch.poll());

		// No 2nd call, nothing was changed
		ASSERT_FALSE(task.is_in_progress());
		ASSERT_TRUE(task.is_finished());
		ASSERT_FALSE(task.is_canceled());
		ASSERT_EQ(Status::Successful, task.status());
		ASSERT_EQ(1, calls_count);
	}

	ASSERT_EQ(std::size_t(0), sch.poll());
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

	ASSERT_EQ(std::size_t(1), sch.poll());

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

	ASSERT_EQ(std::size_t(1), sch.poll());
	task.try_cancel();

	// Function was executed once
	ASSERT_FALSE(task.is_canceled());
	ASSERT_TRUE(task.is_finished());
	ASSERT_EQ(1, calls_count);

	ASSERT_EQ(std::size_t(0), sch.poll());
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
	ASSERT_EQ(std::size_t(1), sch.poll());
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

	ASSERT_EQ(std::size_t(1), sch.poll());
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

	ASSERT_EQ(std::size_t(1), sch.poll());
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

	ASSERT_EQ(std::size_t(1), sch.poll());
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

	ASSERT_EQ(std::size_t(0), sch.poll());
	ASSERT_EQ(std::size_t(2), sch.poll());
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

	ASSERT_EQ(std::size_t(0), sch.poll());
	ASSERT_EQ(std::size_t(2), sch.poll());
	ASSERT_TRUE(task.is_successful());
	ASSERT_TRUE(task.get().has_value());
	ASSERT_EQ(2, task.get().value());
}

TEST(FunctionTask, Cancel_Requested_For_Inner_Task)
{
	Scheduler sch;

	Status status = Status::InProgress;
	bool canceled = false;
	bool invoked = false;
	Task<> task = make_task(sch
		, [&]
	{
		invoked = true;
		return make_test_task(sch, status, canceled);
	});

	ASSERT_FALSE(invoked);
	ASSERT_EQ(std::size_t(0), sch.poll());
	ASSERT_TRUE(invoked);
	ASSERT_EQ(status, task.status());

	task.try_cancel();
	ASSERT_EQ(std::size_t(0), sch.poll());
	ASSERT_EQ(std::size_t(2), sch.poll());
	ASSERT_EQ(Status::Failed, task.status());
	ASSERT_TRUE(canceled);
	ASSERT_TRUE(task.is_canceled());
}
