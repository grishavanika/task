#pragma once
#include <rename_me/custom_task.h>
#include <rename_me/detail/cpp_20.h>
#include <rename_me/detail/on_finish_task.h>

#include <memory>
#include <cassert>

namespace nn
{

	class Scheduler;

	template<typename T, typename E>
	class Task;

	namespace detail
	{
		template<typename T, typename E>
		class InternalTask;

		template<typename R>
		struct OnFinishReturnImpl
		{
			using is_task = std::bool_constant<false>;
			using type = Task<R, void>;
		};

		template<typename T, typename E>
		struct OnFinishReturnImpl<Task<T, E>>
		{
			using is_task = std::bool_constant<true>;
			using type = Task<T, E>;
		};

		template<typename F, typename T
			, typename R = remove_cvref_t<
				decltype(std::declval<F>()(std::declval<const T&>()))>>
		struct OnFinishReturn : public OnFinishReturnImpl<R>
		{
		};

		// Help MSVC to handle trailing return type in case
		// of out-of-class definition of the on_finish() function
		template<typename F, typename T>
		using OnFinishReturnT = typename OnFinishReturn<F, T>::type;

	} // namespace detail

	template<typename T = void, typename E = void>
	class Task
	{
	private:
		using InternalTaskPtr = std::shared_ptr<detail::InternalTask<T, E>>;

		template<typename OtherT, typename OtherE>
		friend class Task;

	public:
		using value_type = T;
		using error_type = E;

	public:
		explicit Task(Scheduler& scheduler, std::unique_ptr<ICustomTask<T, E>> task);
		~Task();
		Task(Task&& rhs) noexcept;
		Task& operator=(Task&& rhs) noexcept;
		Task(const Task& rhs) = delete;
		Task& operator=(const Task& rhs) = delete;

		// Because of on_finish() API, we need to accept `const Task& task`
		// to avoid situations when someones move(task) to internal storage and,
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
		expected<T, E>&& get() &&;
		expected<T, E>&& get_once() &&;

		// Thread-safe
		void try_cancel();
		bool is_canceled() const;

		Status status() const;
		bool is_in_progress() const;
		bool is_finished() const;
		bool is_failed() const;
		bool is_successful() const;

		Scheduler& scheduler() const;

		// Let's R = f(*this). If R is not Task<>, than
		// returns Task<R, void>, otherwise R (e.g., task returned from callback).
		// #TODO: return Task<T, E> when R is expected<T, E>, see comment on function_task.h.
		template<typename F>
		detail::OnFinishReturnT<F, Task>
			on_finish(Scheduler& scheduler, F&& f);

		// Executes on_finish() with this task's scheduler
		template<typename F>
		auto on_finish(F&& f)
			-> decltype(on_finish(
				std::declval<Scheduler&>(), std::forward<F>(f)));

	private:
		explicit Task(InternalTaskPtr task);

		static InternalTaskPtr make_task(
			Scheduler& scheduler, std::unique_ptr<ICustomTask<T, E>> task);

		template<typename F, typename Return, typename Result>
		static void on_finish_invoker(void* erased_f, void* erased_self, Result* result);

		template<typename F, typename Return, typename Result>
		static void invoke_finish(F& f, const Task& self, Result* result
			, std::true_type/*task*/);

		template<typename F, typename Return, typename Result>
		static void invoke_finish(F& f, const Task& self, Result* result
			, std::false_type/*NOT task*/);

		void remove();

	private:
		InternalTaskPtr task_;
	};

} // namespace nn

#include <rename_me/scheduler.h>
#include <rename_me/detail/internal_task.h>

#include <utility>

namespace nn
{
	template<typename T, typename E>
	/*static*/ typename  Task<T, E>::InternalTaskPtr Task<T, E>::make_task(
		Scheduler& scheduler, std::unique_ptr<ICustomTask<T, E>> task)
	{
		auto impl = std::make_shared<detail::InternalTask<T, E>>(
			scheduler, std::move(task));
		scheduler.add(impl);
		return impl;
	}

	template<typename T, typename E>
	/*explicit*/ Task<T, E>::Task(InternalTaskPtr task)
		: task_(std::move(task))
	{
	}

	template<typename T, typename E>
	/*explicit*/ Task<T, E>::Task(Scheduler& scheduler, std::unique_ptr<ICustomTask<T, E>> task)
		: task_(make_task(scheduler, std::move(task)))
	{
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
		return !is_in_progress();
	}

	template<typename T, typename E>
	bool Task<T, E>::is_canceled() const
	{
		assert(task_);
		return task_->is_canceled();
	}

	template<typename T, typename E>
	bool Task<T, E>::is_failed() const
	{
		return (status() == Status::Failed);
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
	template<typename F, typename Return, typename Result>
	/*static*/ void Task<T, E>::on_finish_invoker(
		void* erased_f, void* erased_self, Result* result)
	{
		F& f = *static_cast<F*>(erased_f);
		InternalTaskPtr& self = *static_cast<InternalTaskPtr*>(erased_self);
		using IsTask = typename Return::is_task;

		invoke_finish<F, Return, Result>(f, Task(self), result, IsTask());
		result->invoked = true;
	}

	template<typename T, typename E>
	template<typename F, typename Return, typename Result>
	/*static*/ void Task<T, E>::invoke_finish(F& f, const Task& self, Result* result
		, std::true_type/*is_task*/)
	{
		auto task = std::move(f)(self);
		result->invoked_task = task.task_;
	}

	template<typename T, typename E>
	template<typename F, typename Return, typename Result>
	/*static*/ void Task<T, E>::invoke_finish(F& f, const Task& self, Result* result
		, std::false_type/*is_task*/)
	{
		result->value = std::move(f)(self);
	}

	template<typename T, typename E>
	expected<T, E>& Task<T, E>::get() const &
	{
		assert(task_);
		return task_->get();
	}

	template<typename T, typename E>
	expected<T, E>& Task<T, E>::get() &
	{
		assert(task_);
		return task_->get();
	}

	template<typename T, typename E>
	expected<T, E>&& Task<T, E>::get() &&
	{
		assert(task_);
		return std::move(task_->get());
	}

	template<typename T, typename E>
	expected<T, E>&& Task<T, E>::get_once() &&
	{
		assert(task_);
		return std::move(task_->get());
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
	detail::OnFinishReturnT<F, Task<T, E>>
		Task<T, E>::on_finish(Scheduler& scheduler, F&& f)
	{
		using FinishReturn = detail::OnFinishReturn<F, Task>;
		using ReturnTask = typename FinishReturn::type;
		using IsTask = typename FinishReturn::is_task;
		using Function = detail::remove_cvref_t<F>;
		using FinishTask = detail::OnFinishTask<Task, ReturnTask, Function, IsTask>;
		using InvokeResult = typename FinishTask::InvokeResult;

		assert(task_);
		auto invoker = &Task::on_finish_invoker<Function, FinishReturn, InvokeResult>;
		auto task = std::make_unique<FinishTask>(
			task_, std::forward<F>(f), invoker);
		return ReturnTask(scheduler, std::move(task));
	}

} // namespace nn
