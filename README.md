# RenameMe

[![Build status (Windows)](https://ci.appveyor.com/api/projects/status/xfla7b9s7nkf1ix0?svg=true)](https://ci.appveyor.com/project/grishavanika/task)
[![Build Status (Linux)](https://travis-ci.org/grishavanika/task.svg)](https://travis-ci.org/grishavanika/task)

expected<T, E>-like asynchronous tasks with .on_finish() support
and ability to adapt third-party tasks API.

# Introduction

```
#include <rename_me/future_task.h>

nn::Task<int> DoWork(nn::Scheduler& scheduler, int input)
{
	return make_task(scheduler
		, std::async(std::launch::async, [=]()
	{
		return (2 * input);
	}))
		.on_finish([](const nn::Task<int, std::exception_ptr>& task)
	{
		return (3 * task.get().value());
	});
}

int main()
{
	nn::Scheduler scheduler;
	nn::Task<int> task = DoWork(scheduler, 10);
	while (task.is_in_progress())
	{
		scheduler.tick();
	}
	return task.get().value();
}
```

# TODO:

1. Reduce memory usage. Think about custom allocators.
2. How Scheduler can be customized ?
3. Add tasks_count() to Scheduler's API.
4. Unify nn::expected<> API to be consistent.
5. Add supporting API (like, `when_all(tasks...).on_finis(...)`).
6. More tests. Ensure thread-safe stuff.
7. Polish & extend function_task.h (see comments).
8. Polish memory layout for internal tasks.
9. Default (thread-local ?) Scheduler's ?
10. *Important*. Propagate cancel() (with is_canceled()) to function's task continuation. E.g.:
```
	Scheduler sch;
	Task<int> task = make_task(sch
		, [&sch]
	{
		// Inner task
		return make_task(sch
			, []
		{
			return 1;
		});
	});
	sch.tick();
	// ...
	task.try_cancel();
```

if .try_cancel() was called when "Inner task" is in progress, it should
invoke .try_cancel() on it. And, if successful (e.g., inner task is_canceled() == true),
task.is_canceled() should also be true.

11. Add task.on_success(), task.on_fail() and task.on_cancel() API similar to task.on_finish().
on_success() callback should be invoked only if task is successful. In case it's not,
returned from on_success() Task<> should be failed and canceled (since we can't set
proper user-defined error).
```
	Task<int> success = task.on_success([](const Task<int, int>& root_task)
	{
		return 1;
	});
```
In case root_task is failed, callback should not be called and next should be hold:
```
assert(success.is_finished());
assert(success.is_failed());
assert(success.is_canceled());
expected<int, void>& data = success.get();
assert(data does not hold value or error, e.g., was not constructed);
```

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
