#include <gtest/gtest.h>
#include <rename_me/noop_task.h>

#include "test_tools.h"

using namespace nn;

namespace
{

	template<typename ExpectedTask, typename T, typename E = void>
	struct SuccessTaskReturn
	{
		using IsSame = std::is_same<ExpectedTask
			, decltype(make_task<T, E>(success
				, std::declval<Scheduler&>(), std::declval<T>()))>;

		static constexpr bool value = IsSame::value;
	};

	template<typename ExpectedTask, typename E>
	struct SuccessTaskReturn<ExpectedTask, void, E>
	{
		using IsSame = std::is_same<ExpectedTask
			, decltype(make_task<E>(success
				, std::declval<Scheduler&>()))>;

		static constexpr bool value = IsSame::value;
	};

	static_assert(SuccessTaskReturn<Task<int, void>,           int>::value, "");
	static_assert(SuccessTaskReturn<Task<std::unique_ptr<int>, void>, std::unique_ptr<int>&&>::value, "");
	static_assert(SuccessTaskReturn<Task<>,                    void>::value, "");
	static_assert(SuccessTaskReturn<Task<int, int>,            int, int>::value, "");
	static_assert(SuccessTaskReturn<Task<void, int>,           void, int>::value, "");

} // namespace

TEST(Noop, Success_Task_Is_Finished_Immediately)
{
	{
		Scheduler sch;
		Task<int, void> task = make_task(success, sch, 1);
		ASSERT_TRUE(task.is_finished());
		ASSERT_TRUE(task.is_successful());
		ASSERT_TRUE(task.get().has_value());
		ASSERT_EQ(1, task.get().value());
	}

	{
		Scheduler sch;
		Task<int, int> task =
			make_task<int/*succees*/, int/*error*/>(success, sch, 1);
		ASSERT_TRUE(task.is_finished());
		ASSERT_TRUE(task.is_successful());
		ASSERT_TRUE(task.get().has_value());
		ASSERT_EQ(1, task.get().value());
	}
}

TEST(Noop, Error_Task_Is_Finished_Immediately)
{
	{
		Scheduler sch;
		Task<void, int> task = make_task(error, sch, 1);
		ASSERT_TRUE(task.is_finished());
		ASSERT_TRUE(task.is_failed());
		ASSERT_FALSE(task.get().has_value());
		ASSERT_EQ(1, task.get().error());
	}

	{
		Scheduler sch;
		Task<std::unique_ptr<int>, int> task =
			make_task<int/*error*/, std::unique_ptr<int>/*success*/>(error, sch, 1);
		ASSERT_TRUE(task.is_finished());
		ASSERT_TRUE(task.is_failed());
		ASSERT_FALSE(task.get().has_value());
		ASSERT_EQ(1, task.get().error());
	}
}

TEST(Noop, Status_Is_Reflected_From_Expected)
{
	{
		Scheduler sch;
		using Expected = expected<int, int>;
		Task<int, int> task = make_task(sch, Expected(1));
		ASSERT_TRUE(task.is_finished());
		ASSERT_TRUE(task.is_successful());
		ASSERT_TRUE(task.get().has_value());
		ASSERT_EQ(1, task.get().value());
	}

	{
		Scheduler sch;
		using Expected = expected<int, int>;
		Task<int, int> task = make_task(sch, MakeExpectedWithError<Expected>(1));
		ASSERT_TRUE(task.is_finished());
		ASSERT_TRUE(task.is_failed());
		ASSERT_FALSE(task.get().has_value());
		ASSERT_EQ(1, task.get().error());
	}
}

TEST(Noop, Expexted_With_Void_Error)
{
	Scheduler sch;
	Task<void, void> task = make_task(error, sch);
	ASSERT_TRUE(task.is_finished());
	ASSERT_TRUE(task.is_failed());
	ASSERT_FALSE(task.get().has_value());
}

