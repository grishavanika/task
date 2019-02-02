#pragma once
#include <rename_me/custom_task.h>
#include <rename_me/detail/cpp_20.h>
#include <rename_me/detail/internal_task.h>
#include <rename_me/detail/function_task_base.h>

#include <type_traits>

#include <cassert>

namespace nn
{

	class Scheduler;

	template<typename>
	struct is_task;

	template<typename>
	struct task_from_expected;

	template<typename T = void, typename E = void>
	class Task
	{
	private:
		// Reference counting is needed to handle ownership
		// of task internals for _on_finish()_ implementation.
		// Without on_finish() API, it's enough to have raw pointer
		// with custom management of lifetime thru Scheduler
		// (of course this is true only if Task is move-only.
		// If copy semantic will be enabled, ref. counted-like pointer
		// is needed in any case)
		using InternalTask = detail::RefCountPtr<detail::InternalTask<T, E>>;

		template<typename OtherT, typename OtherE>
		friend class Task;

	public:
		using value_type = T;
		using error_type = E;
		using value = expected<T, E>;

	public:
		template<typename CustomTask, typename... Args>
		static
			typename std::enable_if<IsCustomTask<CustomTask, T, E>::value, Task>::type
				make(Scheduler& scheduler, Args&&... args);
		explicit Task();
		~Task();
		Task(Task&& rhs) noexcept;
		Task& operator=(Task&& rhs) noexcept;
		Task(const Task& rhs) = delete;
		Task& operator=(const Task& rhs) = delete;

		// Because of on_finish() API, we need to accept `const Task& task`
		// to avoid situations when someones std::move(task) to internal storage and,
		// hence, ending up with, possibly, 2 Tasks intances that refer to same
		// InternalTask. This also avoids ability to call task.on_finish() inside
		// on_finish() callback.
		// Because of this decision, getters of values of the task should be const,
		// but return non-const reference so client can get value.
		// 
		// Note: the behavior is undefined if get*():
		//  1. is called before task't finish
		//  2. value from expected<> is moved more than once
		//  3. value from expected<> is read & moved from different threads
		//		(effect of 2nd case)
		expected<T, E>& get() const &;
		expected<T, E>& get() &;
		expected<T, E> get() &&;
		expected<T, E> get_once() const;

		// Thread-safe
		void try_cancel();
		bool is_canceled() const;

		Status status() const;
		bool is_in_progress() const;
		bool is_finished() const;
		bool is_failed() const;
		bool is_successful() const;

		Scheduler& scheduler() const;

		// Let's R = f(*this). If R is:
		//  1. Task<U, O> then returns Task<U, O>.
		//     Returned task will reflect state of returned from functor task.
		//  2. expected<U, O> then returns Task<U, O>.
		//     Returned task will reflect status and value of returned
		//     from functor expected<> (e.g., will be failed if expected contains error).
		//  3. U (some type that is not Task<> or expected<>)
		//     then returns Task<U, void> with immediate success status.
		// 
		// #TODO: accept Args... so caller can pass valid
		// INVOKE()-able expression, like:
		// struct InvokeLike { void call(const Task<>&) {} };
		// InvokeLike callback;
		// task.on_finish(&InvokeLike::call, callback)
		template<typename F>
		detail::FunctionTaskReturnT<F, const Task<T, E>&>
			on_finish(Scheduler& scheduler, F&& f);

		// Same as on_finish(), but functor does not need to accept
		// Task<> instance (e.g., caller discards it and _knows_ that)
		template<typename F>
		detail::FunctionTaskReturnT<F>
			on_finish(Scheduler& scheduler, F&& f);

		// Executes on_finish() with this task's scheduler
		template<typename F>
		auto on_finish(F&& f)
			-> decltype(on_finish(
				std::declval<Scheduler&>(), std::forward<F>(f)));

		// Alias for on_finish()
		template<typename F>
		auto then(Scheduler& scheduler, F&& f)
			-> decltype(on_finish(scheduler, std::forward<F>(f)));

		// Executes then() with this task's scheduler
		template<typename F>
		auto then(F&& f)
			-> decltype(then(
				std::declval<Scheduler&>(), std::forward<F>(f)));

		template<typename F>
		auto on_fail(Scheduler& scheduler, F&& f)
			-> decltype(on_finish(scheduler, std::forward<F>(f)));

		// Executes on_fail() with this task's scheduler
		template<typename F>
		auto on_fail(F&& f)
			-> decltype(on_fail(
				std::declval<Scheduler&>(), std::forward<F>(f)));

		template<typename F>
		auto on_success(Scheduler& scheduler, F&& f)
			-> decltype(on_finish(scheduler, std::forward<F>(f)));

		// Executes on_success() with this task's scheduler
		template<typename F>
		auto on_success(F&& f)
			-> decltype(on_success(
				std::declval<Scheduler&>(), std::forward<F>(f)));

		template<typename F>
		auto on_cancel(Scheduler& scheduler, F&& f)
			-> decltype(on_finish(scheduler, std::forward<F>(f)));

		// Executes on_cancel() with this task's scheduler
		template<typename F>
		auto on_cancel(F&& f)
			-> decltype(on_cancel(
				std::declval<Scheduler&>(), std::forward<F>(f)));

		bool is_valid() const;

	private:
		explicit Task(InternalTask task);

		template<typename F, typename CallPredicate
			, typename ReturnWithTaskArg = detail::FunctionTaskReturn<F, const Task<T, E>&>
			, typename ReturnWithoutTaskArg = detail::FunctionTaskReturn<F>>
		auto on_finish_impl(Scheduler& scheduler, F&& f, CallPredicate p);

		template<typename F>
		static decltype(auto) invoke(std::false_type, F& f, const Task&);
		template<typename F>
		static decltype(auto) invoke(std::true_type, F& f, const Task& self);

		void remove();

	private:
		InternalTask task_;
	};

} // namespace nn

#include <rename_me/scheduler.h>
#include <rename_me/detail/internal_task.h>
#include <rename_me/detail/config.h>
#include <rename_me/detail/ebo_storage.h>

#include <utility>

namespace nn
{

	template<typename T, typename E>
	/*explicit*/ Task<T, E>::Task(InternalTask task)
		: task_(std::move(task))
	{
	}

	template<typename T, typename E>
	template<typename CustomTask, typename... Args>
	/*static*/
		typename std::enable_if<IsCustomTask<CustomTask, T, E>::value, Task<T, E>>::type
			Task<T, E>::make(Scheduler& scheduler, Args&&... args)
	{
		using FullTask = detail::InternalCustomTask<T, E, CustomTask>;
		auto full_task = detail::RefCountPtr<FullTask>::make(
			scheduler, std::forward<Args>(args)...);
		if (full_task->status() == Status::InProgress)
		{
			scheduler.post(full_task.template to_base<detail::TaskBase>());
		}
		return Task(full_task.template to_base<typename InternalTask::type>());
	}

	template<typename T, typename E>
	/*explicit*/ Task<T, E>::Task()
		: task_()
	{
	}

	template<typename T, typename E>
	bool Task<T, E>::is_valid() const
	{
		return task_.operator bool();
	}

	template<typename T, typename E>
	Task<T, E>::~Task()
	{
		remove();
	}

	template<typename T, typename E>
	void Task<T, E>::remove()
	{
		task_ = nullptr;
	}

	template<typename T, typename E>
	Task<T, E>::Task(Task&& rhs) noexcept
		: task_(rhs.task_)
	{
		rhs.task_ = nullptr;
	}

	template<typename T, typename E>
	Task<T, E>& Task<T, E>::operator=(Task&& rhs) noexcept
	{
		if (this != &rhs)
		{
			remove();
			std::swap(task_, rhs.task_);
		}
		return *this;
	}

	template<typename T, typename E>
	Status Task<T, E>::status() const
	{
		assert(task_);
		return task_->status();
	}

	template<typename T, typename E>
	bool Task<T, E>::is_in_progress() const
	{
		return (status() == Status::InProgress);
	}

	template<typename T, typename E>
	bool Task<T, E>::is_finished() const
	{
		return (status() != Status::InProgress);
	}

	template<typename T, typename E>
	bool Task<T, E>::is_canceled() const
	{
		assert(task_);
		return (status() == Status::Canceled);
	}

	template<typename T, typename E>
	bool Task<T, E>::is_failed() const
	{
		const Status s = status();
		return (s == Status::Failed)
			|| (s == Status::Canceled);
	}

	template<typename T, typename E>
	bool Task<T, E>::is_successful() const
	{
		return (status() == Status::Successful);
	}

	template<typename T, typename E>
	Scheduler& Task<T, E>::scheduler() const
	{
		assert(task_);
		return task_->scheduler();
	}

	template<typename T, typename E>
	void Task<T, E>::try_cancel()
	{
		assert(task_);
		task_->cancel();
	}

	template<typename T, typename E>
	expected<T, E>& Task<T, E>::get() const &
	{
		assert(task_);
		return task_->get_data();
	}

	template<typename T, typename E>
	expected<T, E>& Task<T, E>::get() &
	{
		assert(task_);
		return task_->get_data();
	}

	template<typename T, typename E>
	expected<T, E> Task<T, E>::get() &&
	{
		assert(task_);
		return std::move(task_->get_data());
	}

	template<typename T, typename E>
	expected<T, E> Task<T, E>::get_once() const
	{
		assert(task_);
		return std::move(task_->get_data());
	}

	template<typename T, typename E>
	template<typename F>
	/*static*/ decltype(auto) Task<T, E>::invoke(std::false_type, F& f, const Task&)
	{
		return std::move(f)();
	}

	template<typename T, typename E>
	template<typename F>
	/*static*/ decltype(auto) Task<T, E>::invoke(std::true_type, F& f, const Task& self)
	{
		return std::move(f)(self);
	}

	template<typename T, typename E>
	template<typename F, typename CallPredicate
		, typename ReturnWithTaskArg
		, typename ReturnWithoutTaskArg>
	auto Task<T, E>::on_finish_impl(Scheduler& scheduler, F&& f, CallPredicate p)
	{
		using HasTaskArg = typename ReturnWithTaskArg::is_valid;
		using Function = detail::remove_cvref_t<F>;
		using FunctionTaskReturn = std::conditional_t<
			HasTaskArg::value
			, ReturnWithTaskArg
			, ReturnWithoutTaskArg>;
		using ReturnTask = typename FunctionTaskReturn::type;

		struct NN_EBO_CLASS Invoker
			: private detail::EboStorage<Function>
			, private CallPredicate
		{
			using Callable = detail::EboStorage<Function>;

			explicit Invoker(Function f, CallPredicate p, const InternalTask& t)
				: Callable(std::move(f))
				, CallPredicate(std::move(p))
				, task(t)
			{
			}

			decltype(auto) invoke()
			{
				Function& f = static_cast<Callable&>(*this).get();
				return Task::invoke(HasTaskArg(), f, Task(task));
			}

			bool can_invoke()
			{
				auto& call_if = static_cast<CallPredicate&>(*this);
				return std::move(call_if)(Task(task));
			}

			bool wait() const
			{
				return (task->status() == Status::InProgress);
			}

			InternalTask task;
		};

		using FinishTask = detail::FunctionTask<FunctionTaskReturn, Invoker>;

		assert(task_);
		return ReturnTask::template make<FinishTask>(scheduler
			, Invoker(std::forward<F>(f), std::move(p), task_));
	}

	template<typename T, typename E>
	template<typename F>
	detail::FunctionTaskReturnT<F, const Task<T, E>&>
		Task<T, E>::on_finish(Scheduler& scheduler, F&& f)
	{
		return on_finish_impl(scheduler, std::forward<F>(f)
			, [](const Task& self)
		{
			// Invoke callback in any case
			(void)self;
			return true;
		});
	}

	template<typename T, typename E>
	template<typename F>
	detail::FunctionTaskReturnT<F>
		Task<T, E>::on_finish(Scheduler& scheduler, F&& f)
	{
		return on_finish_impl(scheduler, std::forward<F>(f)
			, [](const Task&) { return true; });
	}

	template<typename T, typename E>
	template<typename F>
	auto Task<T, E>::on_finish(F&& f)
		-> decltype(on_finish(
			std::declval<Scheduler&>(), std::forward<F>(f)))
	{
		assert(task_);
		return on_finish(task_->scheduler(), std::forward<F>(f));
	}

	template<typename T, typename E>
	template<typename F>
	auto Task<T, E>::then(Scheduler& scheduler, F&& f)
		-> decltype(on_finish(scheduler, std::forward<F>(f)))
	{
		return on_finish(scheduler, std::forward<F>(f));
	}

	template<typename T, typename E>
	template<typename F>
	auto Task<T, E>::then(F&& f)
		-> decltype(then(
			std::declval<Scheduler&>(), std::forward<F>(f)))
	{
		return then(task_->scheduler(), std::forward<F>(f));
	}

	template<typename T, typename E>
	template<typename F>
	auto Task<T, E>::on_fail(Scheduler& scheduler, F&& f)
		-> decltype(on_finish(scheduler, std::forward<F>(f)))
	{
		return on_finish_impl(scheduler, std::forward<F>(f)
			, [](const Task& self)
		{
			return self.is_failed();
		});
	}

	template<typename T, typename E>
	template<typename F>
	auto Task<T, E>::on_fail(F&& f)
		-> decltype(on_fail(
			std::declval<Scheduler&>(), std::forward<F>(f)))
	{
		assert(task_);
		return on_fail(task_->scheduler(), std::forward<F>(f));
	}

	template<typename T, typename E>
	template<typename F>
	auto Task<T, E>::on_success(Scheduler& scheduler, F&& f)
		-> decltype(on_finish(scheduler, std::forward<F>(f)))
	{
		return on_finish_impl(scheduler, std::forward<F>(f)
			, [](const Task& self)
		{
			return self.is_successful();
		});
	}

	template<typename T, typename E>
	template<typename F>
	auto Task<T, E>::on_success(F&& f)
		-> decltype(on_success(
			std::declval<Scheduler&>(), std::forward<F>(f)))
	{
		assert(task_);
		return on_success(task_->scheduler(), std::forward<F>(f));
	}

	template<typename T, typename E>
	template<typename F>
	auto Task<T, E>::on_cancel(Scheduler& scheduler, F&& f)
		-> decltype(on_finish(scheduler, std::forward<F>(f)))
	{
		return on_finish_impl(scheduler, std::forward<F>(f)
			, [](const Task& self)
		{
			return self.is_canceled();
		});
	}

	template<typename T, typename E>
	template<typename F>
	auto Task<T, E>::on_cancel(F&& f)
		-> decltype(on_cancel(
			std::declval<Scheduler&>(), std::forward<F>(f)))
	{
		assert(task_);
		return on_cancel(task_->scheduler(), std::forward<F>(f));
	}

	template<typename>
	struct is_task : std::false_type { };

	template<typename T, typename E>
	struct is_task<Task<T, E>> : std::true_type { };

	template<typename T, typename E>
	struct task_from_expected<expected<T, E>>
	{
		using type = Task<T, E>;
	};

} // namespace nn
