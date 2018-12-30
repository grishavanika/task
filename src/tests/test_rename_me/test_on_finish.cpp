#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>
#include <rename_me/task.h>
#include <rename_me/function_task.h>

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

using namespace nn;

namespace
{

	// Check that returned type from task.on_finish()
	// has proper Task<> depending on what functor returns
	template<typename R, typename ExpectedTask, typename RootTask = Task<void, void>>
	struct ReturnCheck
	{
		static R functor(const RootTask&);

		using IsSameWithScheduler = std::is_same<ExpectedTask
			, decltype(std::declval<RootTask&>().on_finish(
				std::declval<Scheduler&>(), &ReturnCheck::functor))>;

		using IsSameWithoutScheduler = std::is_same<ExpectedTask
			, decltype(std::declval<RootTask&>().on_finish(
				&ReturnCheck::functor))>;

		static constexpr bool value =
			IsSameWithScheduler::value
				&& IsSameWithoutScheduler::value;
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

} // namespace

TEST(OnFinish, Can_Be_Chained)
{
	Scheduler sch;
	Scheduler sch2;
	std::vector<int> order;

	Task<int> task1 = make_task(sch
		, [&]
	{
		order.push_back(1);
		return 1;
	});

	Task<char> task2 = task1.on_finish(
		[&](const Task<int>& task1)
	{
		order.push_back(2);
		[&]() { ASSERT_TRUE(task1.is_finished()); }();
		return 'x';
	});

	Task<int> task3 = task2.on_finish(sch2,
		[&](const Task<char>& task2)
	{
		order.push_back(3);
		[&]() { ASSERT_TRUE(task2.is_finished()); }();
		return make_task(sch
			, [&]
		{
			order.push_back(4);
			return 2;
		});
	});

	while (task3.is_in_progress())
	{
		sch.tick();
		sch2.tick();
	}

	ASSERT_THAT(order, ElementsAre(1, 2, 3, 4));

	ASSERT_EQ(&sch2, &task3.scheduler());
	ASSERT_TRUE(task1.is_successful());
	ASSERT_TRUE(task2.is_successful());
	ASSERT_TRUE(task3.is_successful());

	ASSERT_EQ(1, task1.get().value());
	ASSERT_EQ('x', task2.get().value());
	ASSERT_EQ(2, task3.get().value());
}

TEST(OnFinish, Can_Be_Called_Multiple_Times_On_Same_Task)
{
	Scheduler sch;
	Task<void> task = make_task(sch, [&] { });
	std::vector<int> calls;
	auto task1 = task.on_finish([&](const Task<void>&) { calls.push_back(1); });
	auto task2 = task.on_finish([&](const Task<void>&) { calls.push_back(2); });
	auto task3 = task.on_finish([&](const Task<void>&) { calls.push_back(3); });

	while (sch.has_tasks())
	{
		sch.tick();
	}

	ASSERT_THAT(calls, UnorderedElementsAre(1, 2, 3));
}
