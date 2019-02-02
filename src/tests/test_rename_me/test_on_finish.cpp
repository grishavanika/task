#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>
#include <rename_me/task.h>
#include <rename_me/function_task.h>

#include "test_tools.h"

using ::testing::ElementsAreArray;
using ::testing::UnorderedElementsAreArray;

using namespace nn;

namespace
{

	// Check that returned type from task.on_finish()
	// has proper Task<> depending on what functor returns
	template<typename R, typename ExpectedTask, typename RootTask = Task<void, void>>
	struct ReturnCheck
	{
		static R functor(const RootTask&);
		static R functor_no_task();

		// on_finish() with/without Scheduler and with/without callable
		// that accepts Task<>
		using f_1 = std::is_same<ExpectedTask
			, decltype(std::declval<RootTask&>().on_finish(
				std::declval<Scheduler&>(), &ReturnCheck::functor))>;

		using f_2 = std::is_same<ExpectedTask
			, decltype(std::declval<RootTask&>().on_finish(
				&ReturnCheck::functor))>;

		using f_3 = std::is_same<ExpectedTask
			, decltype(std::declval<RootTask&>().on_finish(
				std::declval<Scheduler&>(), &ReturnCheck::functor_no_task))>;

		using f_4 = std::is_same<ExpectedTask
			, decltype(std::declval<RootTask&>().on_finish(
				&ReturnCheck::functor_no_task))>;

		// on_success()
		using s_1 = std::is_same<ExpectedTask
			, decltype(std::declval<RootTask&>().on_success(
				std::declval<Scheduler&>(), &ReturnCheck::functor))>;

		using s_2 = std::is_same<ExpectedTask
			, decltype(std::declval<RootTask&>().on_success(
				&ReturnCheck::functor))>;

		using s_3 = std::is_same<ExpectedTask
			, decltype(std::declval<RootTask&>().on_success(
				std::declval<Scheduler&>(), &ReturnCheck::functor_no_task))>;

		using s_4 = std::is_same<ExpectedTask
			, decltype(std::declval<RootTask&>().on_success(
				&ReturnCheck::functor_no_task))>;

		// on_fail()
		using e_1 = std::is_same<ExpectedTask
			, decltype(std::declval<RootTask&>().on_fail(
				std::declval<Scheduler&>(), &ReturnCheck::functor))>;

		using e_2 = std::is_same<ExpectedTask
			, decltype(std::declval<RootTask&>().on_fail(
				&ReturnCheck::functor))>;

		using e_3 = std::is_same<ExpectedTask
			, decltype(std::declval<RootTask&>().on_fail(
				std::declval<Scheduler&>(), &ReturnCheck::functor_no_task))>;

		using e_4 = std::is_same<ExpectedTask
			, decltype(std::declval<RootTask&>().on_fail(
				&ReturnCheck::functor_no_task))>;

		// on_cancel()
		using c_1 = std::is_same<ExpectedTask
			, decltype(std::declval<RootTask&>().on_cancel(
				std::declval<Scheduler&>(), &ReturnCheck::functor))>;

		using c_2 = std::is_same<ExpectedTask
			, decltype(std::declval<RootTask&>().on_cancel(
				&ReturnCheck::functor))>;

		using c_3 = std::is_same<ExpectedTask
			, decltype(std::declval<RootTask&>().on_cancel(
				std::declval<Scheduler&>(), &ReturnCheck::functor_no_task))>;

		using c_4 = std::is_same<ExpectedTask
			, decltype(std::declval<RootTask&>().on_cancel(
				&ReturnCheck::functor_no_task))>;

		// then()
		using t_1 = std::is_same<ExpectedTask
			, decltype(std::declval<RootTask&>().then(
				std::declval<Scheduler&>(), &ReturnCheck::functor))>;

		using t_2 = std::is_same<ExpectedTask
			, decltype(std::declval<RootTask&>().then(
				&ReturnCheck::functor))>;

		using t_3 = std::is_same<ExpectedTask
			, decltype(std::declval<RootTask&>().then(
				std::declval<Scheduler&>(), &ReturnCheck::functor_no_task))>;

		using t_4 = std::is_same<ExpectedTask
			, decltype(std::declval<RootTask&>().then(
				&ReturnCheck::functor_no_task))>;

		static constexpr bool value = std::conjunction_v<
			  f_1, f_2, f_3, f_4
			, s_1, s_2, s_3, s_4
			, e_1, e_2, e_3, e_4
			, c_1, c_2, c_3, c_4
			, t_1, t_2, t_3, t_4
			>;
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
		, "Task<char, char> should be returned when using Task<char, char>& f() callback");

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
		(void)sch.poll();
		(void)sch2.poll();
	}

	ASSERT_THAT(order, ElementsAreArray({1, 2, 3, 4}));

	ASSERT_EQ(&sch2, &task3.scheduler());
	ASSERT_TRUE(task1.is_successful());
	ASSERT_TRUE(task2.is_successful());
	ASSERT_TRUE(task3.is_successful());

	ASSERT_EQ(1, task1.get().value());
	ASSERT_EQ('x', task2.get().value());
	ASSERT_EQ(2, task3.get().value());
}

TEST(OnFinish, Then_Is_Alias_For_On_Finish)
{
	Scheduler sch;
	Task<> task = make_task(sch, [] { });
	Task<> then = task.then([](const Task<>&) {});
	Task<> on_finish = task.on_finish([](const Task<>&) {});
	(void)sch.poll();
	ASSERT_FALSE(sch.has_tasks());
	ASSERT_EQ(then.status(), on_finish.status());
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
		(void)sch.poll();
	}

	ASSERT_THAT(calls, UnorderedElementsAreArray({1, 2, 3}));
}

TEST(OnFinish, On_Fail_Is_Failed_And_Canceled_When_Task_Finish_With_Success)
{
	Scheduler sch;
	Task<void> success = make_task(sch, [] { });

	bool fail_invoked = false;
	Task<void> on_fail = success.on_fail([&](const Task<void>&)
	{
		fail_invoked = true;
	});

	(void)sch.poll();
	ASSERT_TRUE(success.is_successful());
	ASSERT_TRUE(on_fail.is_canceled());
	ASSERT_TRUE(on_fail.is_failed());
	ASSERT_FALSE(fail_invoked);
}

TEST(OnFinish, On_Fail_Is_Successful_When_Task_Finish_With_Fail)
{
	Scheduler sch;
	Task<int, int> failed = make_task(sch
		, []
	{
		return MakeExpectedWithError<expected<int, int>>(1);
	});

	bool fail_invoked = false;
	Task<void> on_fail = failed.on_fail([&](const Task<int, int>&)
	{
		fail_invoked = true;
	});

	(void)sch.poll();
	ASSERT_TRUE(failed.is_failed());
	ASSERT_TRUE(on_fail.is_successful());
	ASSERT_FALSE(on_fail.is_canceled());
	ASSERT_TRUE(fail_invoked);
}

TEST(OnFinish, On_Success_Is_Successful_When_Task_Finish_With_Success)
{
	Scheduler sch;
	Task<void> success = make_task(sch, [] {});

	bool success_invoked = false;
	Task<void> on_success = success.on_success([&](const Task<void>&)
	{
		success_invoked = true;
	});

	(void)sch.poll();
	ASSERT_TRUE(success.is_successful());
	ASSERT_TRUE(on_success.is_successful());
	ASSERT_FALSE(on_success.is_canceled());
	ASSERT_TRUE(success_invoked);
}

TEST(OnFinish, On_Success_Is_Failed_And_Canceled_When_Task_Finish_With_Fail)
{
	Scheduler sch;
	Task<int, int> failed = make_task(sch
		, []
	{
		return MakeExpectedWithError<expected<int, int>>(1);
	});

	bool success_invoked = false;
	Task<void> on_success = failed.on_success([&](const Task<int, int>&)
	{
		success_invoked = true;
	});

	(void)sch.poll();
	ASSERT_TRUE(failed.is_failed());
	ASSERT_TRUE(on_success.is_canceled());
	ASSERT_TRUE(on_success.is_failed());
	ASSERT_FALSE(success_invoked);
}

TEST(OnFinish, On_Cancel_Is_Successful_When_Task_Is_Canceled)
{
	Scheduler sch;
	Task<void> task = make_task(sch, [&] { });
	task.try_cancel();

	bool cancel_invoked = false;
	Task<void> on_cancel = task.on_cancel([&](const Task<void>&)
	{
		cancel_invoked = true;
	});

	(void)sch.poll();

	ASSERT_TRUE(task.is_canceled());
	ASSERT_TRUE(on_cancel.is_successful());
	ASSERT_FALSE(on_cancel.is_canceled());
	ASSERT_TRUE(cancel_invoked);
}

TEST(OnFinish, On_Cancel_Is_Failed_And_Canceled_When_Task_Is_NOT_Canceled)
{
	Scheduler sch;
	Task<void> task = make_task(sch, [&] {});

	bool cancel_invoked = false;
	Task<void> on_cancel = task.on_cancel([&](const Task<void>&)
	{
		cancel_invoked = true;
	});

	(void)sch.poll();

	ASSERT_FALSE(task.is_canceled());
	ASSERT_TRUE(on_cancel.is_failed());
	ASSERT_TRUE(on_cancel.is_canceled());
	ASSERT_FALSE(cancel_invoked);
}
