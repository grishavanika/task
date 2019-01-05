#include <gtest/gtest.h>
#include <rename_me/function_task.h>
#include <rename_me/detail/config.h>

#include "test_tools.h"

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

	// Check minimum possible size of internal FunctionTask
	struct EBOFunction { void operator()() const { } };
	using EBOArgs = std::tuple<>;
	using EBOFunctionTaskReturn = detail::FunctionTaskReturn<EBOFunction, EBOArgs>;
	struct NN_EBO_CLASS EBOInvoker
		: private detail::EBOFunctor<EBOFunction>
		, private EBOArgs
	{
		void invoke() { }
		bool can_invoke() const { return true; }
		bool wait() const { return false; }
	};
	using EBOFunctionTask = detail::FunctionTask<EBOFunctionTaskReturn, EBOInvoker>;
	static_assert(sizeof(EBOFunctionTask) == sizeof(bool)
		, "detail::FunctionTask<> uses only bool as data member. "
		"Everything else should be EBO-enabled");

	// Set/check minimum possible size of internal CustomTask.
	// May change in the future
	struct EBOTask : expected<void, void>
	{
		Status tick(bool) { return Status::Successful; }
		expected<void, void>& get() { return *this; }
	};
	static_assert(std::is_empty<EBOTask>::value, "");
	static_assert(sizeof(detail::InternalCustomTask<void, void, EBOTask>)
		== 3 * sizeof(void*), "");

	// Test-controlled task
	struct TestTask
		: expected<void, void>
	{
		explicit TestTask(Status& status)
			: status_(status)
		{
		}

		Status tick(bool cancel_requested)
		{
			if (cancel_requested)
			{
				return Status::Canceled;
			}
			return status_;
		}

		expected<void, void>& get()
		{
			return *this;
		}

		Status& status_;
	};

	static_assert(IsCustomTask<TestTask, void, void>::value, "");

	Task<> make_test_task(Scheduler& sch, Status& status)
	{
		return Task<>::template make<TestTask>(sch, status);
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
	ASSERT_EQ(Status::Canceled, task.status());
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
	bool invoked = false;
	Task<> task = make_task(sch
		, [&]
	{
		invoked = true;
		return make_test_task(sch, status);
	});

	ASSERT_FALSE(invoked);
	ASSERT_EQ(std::size_t(0), sch.poll());
	ASSERT_TRUE(invoked);
	ASSERT_EQ(status, task.status());

	task.try_cancel();
	ASSERT_EQ(std::size_t(0), sch.poll());
	ASSERT_EQ(std::size_t(2), sch.poll());
	ASSERT_EQ(Status::Canceled, task.status());
	ASSERT_TRUE(task.is_canceled());
}

TEST(FunctionTask, Returns_Non_Default_Constructiable_Value)
{
	struct Value
	{
		int data;
		explicit Value(int v)
			: data(v)
		{
		}
	};
	Scheduler sch;
	Task<Value> task = make_task(sch, [&] { return Value(1); });
	(void)sch.poll();
	ASSERT_EQ(1, task.get().value().data);
}

TEST(FunctionTask, Can_Be_Invoked_From_EBO_Functor)
{
	Scheduler sch;

	struct Functor
	{
		int operator()(int v) const
		{
			return v;
		}

		int call(int v) const
		{
			return operator()(v);
		}

		int operator()(std::unique_ptr<int>&& v) const
		{
			return *v * 3;
		}
	};
	{
		Task<int> task = make_task(sch, Functor(), 10);
		(void)sch.poll();
		ASSERT_EQ(10, task.get().value());
	}
	{
		Task<int> task = make_task(sch, Functor(), std::make_unique<int>(10));
		(void)sch.poll();
		ASSERT_EQ(30, task.get().value());
	}
	{
		Functor f;
		Task<int> task = make_task(sch, &Functor::call, &f, 10);
		(void)sch.poll();
		ASSERT_EQ(10, task.get().value());
	}
}

TEST(FunctionTask, Can_Be_Invoked_From_Functor_With_Data)
{
	Scheduler sch;

	struct Functor
	{
		std::unique_ptr<int> data = std::make_unique<int>(2);

		int operator()(int v) const
		{
			return *data * v;
		}

		int call(int v) const
		{
			return operator()(v);
		}

		int operator()(std::unique_ptr<int>&& v) const
		{
			return *data * *v * 3;
		}
	};
	{
		Task<int> task = make_task(sch, Functor(), 10);
		(void)sch.poll();
		ASSERT_EQ(2 * 10, task.get().value());
	}
	{
		Task<int> task = make_task(sch, Functor(), std::make_unique<int>(10));
		(void)sch.poll();
		ASSERT_EQ(2 * 30, task.get().value());
	}
	{
		Functor f;
		Task<int> task = make_task(sch, &Functor::call, &f, 10);
		(void)sch.poll();
		ASSERT_EQ(2 * 10, task.get().value());
	}
}

TEST(FunctionTask, Can_Be_Invoked_From_Function_Pointer)
{
	Scheduler sch;

	struct Function
	{
		static int call(int v)
		{
			return v;
		}

		static int call_unique(std::unique_ptr<int>&& v)
		{
			return *v * 3;
		}
	};
	{
		Task<int> task = make_task(sch, &Function::call, 10);
		(void)sch.poll();
		ASSERT_EQ(10, task.get().value());
	}
	{
		Task<int> task = make_task(sch, &Function::call_unique, std::make_unique<int>(10));
		(void)sch.poll();
		ASSERT_EQ(30, task.get().value());
	}
}

TEST(FunctionTask, Can_Be_Invoked_From_L_Value_Functor)
{
	Scheduler sch;

	struct Functor
	{
		int operator()(int v) const
		{
			return v;
		}
	};
	{
		Functor f;
		Task<int> task = make_task(sch, f, 10);
		(void)sch.poll();
		ASSERT_EQ(10, task.get().value());
	}
}

TEST(FunctionTask, Can_Be_Invoked_From_Move_Only_Functor)
{
	Scheduler sch;

	struct Functor
	{
		Functor() = default;
		Functor(Functor&&) = default;
		Functor& operator=(Functor&&) = delete;
		Functor(const Functor&) = delete;
		Functor& operator=(const Functor&) = delete;

		int operator()(int v) const
		{
			return v;
		}
	};
	{
		Task<int> task = make_task(sch, Functor(), 10);
		(void)sch.poll();
		ASSERT_EQ(10, task.get().value());
	}
}
