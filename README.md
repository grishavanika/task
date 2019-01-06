# RenameMe

[![Build status (Windows)](https://ci.appveyor.com/api/projects/status/xfla7b9s7nkf1ix0?svg=true)](https://ci.appveyor.com/project/grishavanika/task)
[![Build Status (Linux)](https://travis-ci.org/grishavanika/task.svg)](https://travis-ci.org/grishavanika/task)

expected<T, E>-like asynchronous tasks with .on_finish() support
and ability to adapt third-party tasks API.

# Introduction

```
#include <rename_me/future_task.h>
#include <chrono>
#include <cstdio>
#include <cassert>

nn::Task<int> DoWork(nn::Scheduler& scheduler, int input)
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
}

int main()
{
	nn::Scheduler scheduler;
	nn::Task<int> task = DoWork(scheduler, 10);
	while (task.is_in_progress())
	{
		scheduler.poll();
	}
	return task.get().value(); // Returns 10 * 2 * 3 = 60
}
```

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
11.

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
