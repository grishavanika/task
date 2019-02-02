#include <gtest/gtest.h>
#include <rename_me/forward_task.h>
#include <rename_me/noop_task.h>

#include "test_tools.h"

using namespace nn;

namespace
{

	Task<float, int> CreateTask(Scheduler& sch, int v)
	{
		return make_task<float, int>(success, sch, v * 1.f);
	}

	Task<float, int> CreateFailTask(Scheduler& sch, int v)
	{
		return make_task<int, float>(error, sch, v);
	}

	Task<void, int> CreateVoidTask(Scheduler& sch)
	{
		using Expected = expected<void, int>;
		return make_task(sch, Expected());
	}

	Task<void, int> CreateVoidFailTask(Scheduler& sch, int v)
	{
		using Expected = expected<void, int>;
		using Unexpected = ::nn::unexpected<int>;
		return make_task(sch, Expected(Unexpected(v)));
	}

	Task<int, void> CreateVoidErrorTask(Scheduler& sch, int v)
	{
		return make_task<int, void>(success, sch, v);
	}

	Task<int, void> CreateVoidErrorFailTask(Scheduler& sch)
	{
		return make_task<int>(error, sch);
	}

	template<typename T, typename E, typename F>
	using Invokable = detail::Invokable<T, E, F>;

} // namespace

// Task<float, int>
// F(float) -> expected<int, int> -> Task<int, int>
// F() -> expected<int, int> -> Task<int, int>
TEST(TaskForward, _1)
{
	Scheduler sch;
	
	int calls_count = 0;

	struct CallableAcceptValue
	{
		int* calls_count;
		explicit CallableAcceptValue(int& counter) : calls_count(&counter) {}

		auto operator()(float&& v)
		{
			++(*calls_count);
			using Return = expected<int, int>;
			return Return(static_cast<int>(v) * 2);
		}
	};

	struct CallableDiscardValue
	{
		int* calls_count;
		explicit CallableDiscardValue(int& counter) : calls_count(&counter) {}

		auto operator()()
		{
			++(*calls_count);
			using Return = expected<int, int>;
			return Return(-1);
		}
	};

	using InvokableAccept = Invokable<float, int, CallableAcceptValue>;
	static_assert(InvokableAccept::is_valid(), "");

	calls_count = 0;
	{ // success, f(T/*success*/)
		Task<int, int> task = InvokableAccept::invoke(sch
			, CreateTask(sch, 10) // Task<float, int>
			, CallableAcceptValue(calls_count)); // f(float&&)
		ASSERT_EQ(1, calls_count);
		ASSERT_TRUE(task.is_successful());
		ASSERT_EQ(20, task.get_once().value());
	}

	calls_count = 0;
	{ // error, f(T/*success*/)
		Task<int, int> task = InvokableAccept::invoke(sch
			, CreateFailTask(sch, 10) // Task<float, int>
			, CallableAcceptValue(calls_count)); // f(float&&)
		ASSERT_EQ(0, calls_count);
		ASSERT_TRUE(task.is_failed());
		ASSERT_EQ(10, task.get().error());
	}

	using InvokableDiscard = Invokable<float, int, CallableDiscardValue>;
	static_assert(InvokableDiscard::is_valid(), "");

	calls_count = 0;
	{ // success, f(/*discard*/)
		Task<int, int> task = InvokableDiscard::invoke(sch
			, CreateTask(sch, 10)
			, CallableDiscardValue(calls_count));
		ASSERT_EQ(1, calls_count);
		ASSERT_TRUE(task.is_successful());
		ASSERT_EQ(-1, task.get_once().value());
	}

	calls_count = 0;
	{ // error, f(/*discard*/)
		Task<int, int> task = InvokableDiscard::invoke(sch
			, CreateFailTask(sch, 10)
			, CallableDiscardValue(calls_count));
		ASSERT_EQ(0, calls_count);
		ASSERT_TRUE(task.is_failed());
		ASSERT_EQ(10, task.get().error());
	}
}

// Task<void, int>
// F(float) -> expected<int, int> -> Task<int, int>
// F() -> expected<int, int> -> Task<int, int>
TEST(TaskForward, _2)
{
	Scheduler sch;

	int calls_count = 0;

	struct Callable
	{
		int* calls_count;
		explicit Callable(int& counter) : calls_count(&counter) {}

		auto operator()()
		{
			++(*calls_count);
			using Return = expected<int, int>;
			return Return(3);
		}
	};

	using Invokable = Invokable<void, int, Callable>;
	static_assert(Invokable::is_valid(), "");

	calls_count = 0;
	{ // success, f()
		Task<int, int> task = Invokable::invoke(sch
			, CreateVoidTask(sch) // Task<void, int>
			, Callable(calls_count)); // f()
		ASSERT_EQ(1, calls_count);
		ASSERT_TRUE(task.is_successful());
		ASSERT_EQ(3, task.get_once().value());
	}

	calls_count = 0;
	{ // error, f(T/*success*/)
		Task<int, int> task = Invokable::invoke(sch
			, CreateVoidFailTask(sch, 10) // Task<void, int>
			, Callable(calls_count)); // f()
		ASSERT_EQ(0, calls_count);
		ASSERT_TRUE(task.is_failed());
		ASSERT_EQ(10, task.get().error());
	}
}

// Task<int, void>
// F(int) -> expected<int, void> -> Task<int, void>
// F() -> expected<int, void> -> Task<int, void>
TEST(TaskForward, _3)
{
	Scheduler sch;

	int calls_count = 0;

	struct CallableAcceptValue
	{
		int* calls_count;
		explicit CallableAcceptValue(int& counter) : calls_count(&counter) {}

		auto operator()(int&& v)
		{
			++(*calls_count);
			using Return = expected<int, void>;
			return Return(static_cast<int>(v) * 2);
		}
	};

	struct CallableDiscardValue
	{
		int* calls_count;
		explicit CallableDiscardValue(int& counter) : calls_count(&counter) {}

		auto operator()()
		{
			++(*calls_count);
			using Return = expected<int, void>;
			return Return(-1);
		}
	};

	using InvokableAccept = Invokable<int, void, CallableAcceptValue>;
	static_assert(InvokableAccept::is_valid(), "");

	calls_count = 0;
	{ // success, f(T/*success*/)
		Task<int, void> task = InvokableAccept::invoke(sch
			, CreateVoidErrorTask(sch, 10) // Task<int, void>
			, CallableAcceptValue(calls_count)); // f(int&&)
		ASSERT_EQ(1, calls_count);
		ASSERT_TRUE(task.is_successful());
		ASSERT_EQ(20, task.get_once().value());
	}

	calls_count = 0;
	{ // error, f(T/*success*/)
		Task<int, void> task = InvokableAccept::invoke(sch
			, CreateVoidErrorFailTask(sch) // Task<int, void>
			, CallableAcceptValue(calls_count)); // f(int&&)
		ASSERT_EQ(0, calls_count);
		ASSERT_TRUE(task.is_failed());
		ASSERT_FALSE(task.get().has_value());
	}

	using InvokableDiscard = Invokable<int, void, CallableDiscardValue>;
	static_assert(InvokableDiscard::is_valid(), "");

	calls_count = 0;
	{ // success, f(/*discard*/)
		Task<int, void> task = InvokableDiscard::invoke(sch
			, CreateVoidErrorTask(sch, 10) // Task<int, void>
			, CallableDiscardValue(calls_count));
		ASSERT_EQ(1, calls_count);
		ASSERT_TRUE(task.is_successful());
		ASSERT_EQ(-1, task.get_once().value());
	}

	calls_count = 0;
	{ // error, f(/*discard*/)
		Task<int, void> task = InvokableDiscard::invoke(sch
			, CreateVoidErrorFailTask(sch) // Task<int, void>
			, CallableDiscardValue(calls_count));
		ASSERT_EQ(0, calls_count);
		ASSERT_TRUE(task.is_failed());
		ASSERT_FALSE(task.get().has_value());
	}
}

// Task<void, void>
// F() -> expected<int, void> -> Task<int, void>
TEST(TaskForward, _4)
{
	Scheduler sch;

	int calls_count = 0;

	struct Callable
	{
		int* calls_count;
		explicit Callable(int& counter) : calls_count(&counter) {}

		auto operator()()
		{
			++(*calls_count);
			using Return = expected<int, void>;
			return Return(3);
		}
	};

	using Invokable = Invokable<void, void, Callable>;
	static_assert(Invokable::is_valid(), "");

	calls_count = 0;
	{ // success, f()
		Task<int, void> task = Invokable::invoke(sch
			, make_task(success, sch)
			, Callable(calls_count)); // f()
		ASSERT_EQ(1, calls_count);
		ASSERT_TRUE(task.is_successful());
		ASSERT_EQ(3, task.get_once().value());
	}

	calls_count = 0;
	{ // error, f(T/*success*/)
		Task<int, void> task = Invokable::invoke(sch
			, make_task(error, sch)
			, Callable(calls_count)); // f()
		ASSERT_EQ(0, calls_count);
		ASSERT_TRUE(task.is_failed());
		ASSERT_FALSE(task.get().has_value());
	}
}
