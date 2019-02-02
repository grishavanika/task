# RenameMe

[![Build status (Windows)](https://ci.appveyor.com/api/projects/status/xfla7b9s7nkf1ix0?svg=true)](https://ci.appveyor.com/project/grishavanika/task)
[![Build Status (Linux)](https://travis-ci.org/grishavanika/task.svg)](https://travis-ci.org/grishavanika/task)

expected<T, E>-like asynchronous tasks with .on_finish() support
and ability to adapt third-party tasks API.

# Introduction

``` cpp
#include <rename_me/future_task.h>
#include <chrono>
#include <cstdio>
#include <cassert>

int main()
{
	auto do_work = [](nn::Scheduler& scheduler, int input) -> nn::Task<int>
	{
		auto task = make_task(scheduler
			// Run std::async() and wrap std::future<> into Task<>
			, std::async(std::launch::async, [=]()
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			return (2 * input);
		}))
			// When std::async() completes
			.then([](const nn::Task<int, std::exception_ptr>& task)
		{
			// There was no exception
			assert(task.is_successful());
			return (3 * task.get().value());
		});

		// When our `task` completes. Ignore any values, just print something
		(void)task.then([]
		{
			std::puts("Completed");
		});
		return task;
	};

	nn::Scheduler scheduler;
	nn::Task<int> task = do_work(scheduler, 10);
	while (task.is_in_progress())
	{
		scheduler.poll();
	}
	return task.get().value(); // Returns 10 * 2 * 3 = 60
}
```

See [simple_then example](task/blob/master/src/examples/example_simple_then/main.cpp).

# TODO:

1. Add support of custom allocators.
   One allocator for scheduler ? Or one allocator per task ?
   (allocator per task will be hard to add/implement with
   existing interface).
2. How Scheduler can be customized ?
4. Unify nn::expected<> API to be consistent.
5. Add supporting API (like, `when_all(tasks...).on_finish(...)`).
6. Tests: ensure thread-safe stuff.
8. Polish memory layout for internal tasks.
9. Default (thread-local ?) Scheduler's ?
10. Compare to continuable: https://github.com/Naios/continuable
11. [*can't be done without some kind of RTTI*] Attach `Info` to the task.
    This allows to have:
      - NoInfo (in Retail, for example)
          - configurable with NN_DISABLE_INFO
      - DebugInfo (were started and so on)
      - ExecutionInfo (parent/child relationships, see transwrap library)
          - configurable with NN_ENABLE_TASKS_GRAPH
      - CustomInfo - allows to attach custom information by the client

```
struct NoInfo {};
struct DebugInfo : NoInfo { const char* const file, unsigned line; /**/ };
struct ExecutionInfo : DebugInfo { TaskHandle parent; /**/ };

#if (NN_DISABLE_INFO)
  using TaskInfo = NoInfo;
#else
  #if (NN_ENABLE_TASKS_GRAPH)
    using TaskInfo = ExecutionInfo;
  #else
    using TaskInfo = DebugInfo;
  #endif
#endif

struct CustomInfo : TaskInfo { int cookie; /**/ };
```

12. Because of Cancel, we need to require Error to be default-constructible
    to be able to create expected<..., Error> in any case, when cancel() happens
    in runtime.
    
    This mostly needed to ensure that no matter what - client can always
    get expected<...> from the Task<...> even if it was canceled.
    It's custom's task responsibility to handle cancel nicely.
    This also allows to make nice API that unwraps Task<T, E>
    to `on_success([](T&&) {})` and `on_fail([](E&&, bool canceled))`, for example.
    
    Looks like this allows also remove LazyStorage<> since we always can build
    default expected with error state.

13. Make generalization: let client provides customization of our traits
    that will tell how to get/set success and error values to some type T.
    We can do something like `template<typename Data> BasicTask` and Task be
    `using template<typename T, typename E> Task = BasicTask<expected<T, E>>`.
    
    This allows to optimize storage even more for cases like `expected<void, void>`.
```
template<typename Data>
struct DataTraits;

template<typename T, typename E>
struct DataTraits<expected<T, E>>
{
	using value_type = T;
	using error_type = E;
	using type = expected<T, E>;

	value_type& get_value(expected<T, E>&) {}
	error_type& get_error(expected<T, E>&) {}
	type build_value(...) {}
	type build_error(...) {}
};

// Special case for DataTraits<void>
```

14. Pass Scheduler instance to CustomTask's tick().
    E.g., extend CustomTask to have either tick(Scheduler&, bool) or tick(bool).
    Needed to fix function_task.

# Compilers

TODO: some C++ 17 stuff is used, can be back-ported to C++ 11

# How-to build

```
mkdir build && cd build
cmake -G "Visual Studio 15 2017 Win64" ..
# cmake ..
cmake --build . --config Release
```

# Notes for Windows and non-MSVC compilers

Clang integration into MSVC 2017 on latest Clang (8.0).

- go to and install LLVM *with* "LLVM Compiler Toolchain Visual Studio extension"
- run CMake with "-T LLVM" toolset (case sensetive)
