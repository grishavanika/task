#pragma once
#include <rename_me/expected.h>
#include <rename_me/detail/internal_task.h>
#include <rename_me/detail/config.h>
#include <rename_me/detail/cpp_20.h>
#include <rename_me/detail/lazy_storage.h>
#include <rename_me/detail/noop_task_base.h>

#include <functional>
#include <cassert>
#include <cstdint>

namespace nn
{
	
	class Scheduler;

	template<typename T, typename E>
	class Task;

	namespace detail
	{
		// To minimize amount of template instantiations
		// this helper also contains part of the logic
		// that knows how to tick() and get value nicely

		template<typename R>
		struct FunctionTaskReturnImpl
		{
			using is_expected   = std::bool_constant<false>;
			using is_task       = std::bool_constant<false>;
			using type          = Task<R, void>;
			using expected_type = expected<R, void>;
			using storage       = LazyStorage<expected<R, void>>;
			using is_void       = std::bool_constant<false>;

			static Status tick_results(storage&, bool)
			{
				return Status::Successful;
			}

			static expected_type& get(storage& data)
			{
				return data.get();
			}

			template<typename F, typename... Args>
			static Task<R, void> invoke(Scheduler& scheduler, F&& f, Args&&... args)
			{
				return Task<R, void>::template make<detail::NoopTask<R, void>>(scheduler
					, expected_type(std::forward<F>(f)(std::forward<Args>(args)...)));
			}
		};

		template<>
		struct FunctionTaskReturnImpl<void>
		{
			using is_expected   = std::bool_constant<false>;
			using is_task       = std::bool_constant<false>;
			using type          = Task<void, void>;
			using expected_type = expected<void, void>;
			using storage       = LazyStorage<expected<void, void>>;
			using is_void       = std::bool_constant<true>;

			static Status tick_results(storage&, bool)
			{
				return Status::Successful;
			}

			static expected_type& get(storage& data)
			{
				return data.get();
			}

			template<typename X, typename F, typename... Args>
			static Task<X, X> invoke_incomplete(Scheduler& scheduler, F&& f, Args&&... args)
			{
				(void)std::forward<F>(f)(std::forward<Args>(args)...);
				return Task<X, X>::template make<detail::NoopTask<X, X>>(scheduler, expected_type());
			}

			template<typename F, typename... Args>
			static auto invoke(Scheduler& scheduler, F&& f, Args&&... args)
			{
				// Delay incomplete Task<void, void> instantiation
				return invoke_incomplete<void>(scheduler, std::forward<F>(f), std::forward<Args>(args)...);
			}
		};

		template<typename T, typename E>
		struct FunctionTaskReturnImpl<expected<T, E>>
		{
			using is_expected   = std::bool_constant<true>;
			using is_task       = std::bool_constant<false>;
			using type          = Task<T, E>;
			using expected_type = expected<T, E>;
			using storage       = LazyStorage<expected<T, E>>;
			using is_void       = std::bool_constant<false>;

			static Status tick_results(storage& data, bool)
			{
				return (data.get().has_value() ? Status::Successful : Status::Failed);
			}

			static expected_type& get(storage& data)
			{
				return data.get();
			}

			template<typename F, typename... Args>
			static Task<T, E> invoke(Scheduler& scheduler, F&& f, Args&&... args)
			{
				return Task<T, E>::template make<detail::NoopTask<T, E>>(scheduler
					, std::forward<F>(f)(std::forward<Args>(args)...));
			}
		};

		template<typename T, typename E>
		struct FunctionTaskReturnImpl<Task<T, E>>
		{
			using is_expected   = std::bool_constant<false>;
			using is_task       = std::bool_constant<true>;
			using type          = Task<T, E>;
			using expected_type = expected<T, E>;
			using storage       = LazyStorage<Task<T, E>>;
			using is_void       = std::bool_constant<false>;

			static Status tick_results(storage& data, bool cancel_requested)
			{
				auto& task = data.get();
				if (cancel_requested)
				{
					task.try_cancel();
				}
				return task.status();
			}

			static expected_type& get(storage& data)
			{
				return data.get().get();
			}

			template<typename F, typename... Args>
			static Task<T, E> invoke(Scheduler& scheduler, F&& f, Args&&... args)
			{
				(void)scheduler;
				return std::forward<F>(f)(std::forward<Args>(args)...);
			}
		};

		template<bool, typename F, typename... Args>
		struct FunctionTaskReturnEnable
		{
			using is_valid = std::false_type;
		};

		template<typename F, typename... Args>
		struct FunctionTaskReturnEnable<true, F, Args...>
			: FunctionTaskReturnImpl<remove_cvref_t<
				std::invoke_result_t<F, Args...>>>
		{
			using is_valid = std::true_type;
		};

		template<typename F, typename... Args>
		struct FunctionTaskReturn
			: FunctionTaskReturnEnable<std::is_invocable_v<F, Args...>
				, F, Args...>
		{
		};

		// Help MSVC to out-of-class definition of the on_finish() function
		template<typename F, typename... Args>
		using FunctionTaskReturnT = typename FunctionTaskReturn<F, Args...>::type;

		// `Invoker` is:
		//  (1) `auto invoke()` that returns result of functor invocation.
		//  (2) `bool can_invoke()` that returns true if invoke() call is allowed.
		//    Otherwise task will be marked as canceled.
		//  (3) `bool wait() const` that returns true if invoke() call is delayed.
		template<
			typename Return // FunctionTaskReturn helper
			, typename Invoker
			, typename Storage = typename Return::storage>
		class NN_EBO_CLASS FunctionTask
			: private Invoker
			, private Storage
		{
			using IsTask = typename Return::is_task;
			using IsApplyVoid = typename Return::is_void;
		public:
			explicit FunctionTask(Invoker&& invoker)
				: Invoker(std::move(invoker))
				, Storage()
				, invoked_(false)
			{
			}

			FunctionTask(FunctionTask&&) = delete;
			FunctionTask& operator=(FunctionTask&&) = delete;
			FunctionTask(const FunctionTask&) = delete;
			FunctionTask& operator=(const FunctionTask&) = delete;

			Status tick(bool cancel_requested)
			{
				if (cancel_requested && !invoked_)
				{
					return Status::Canceled;
				}
				if (IsTask() && invoked_)
				{
					return Return::tick_results(*this, cancel_requested);
				}
				if (const_invoker().wait())
				{
					assert(!invoked_);
					return Status::InProgress;
				}
				if (!invoker().can_invoke())
				{
					return Status::Canceled;
				}

				assert(!invoked_);
				call_impl(IsApplyVoid());
				invoked_ = true;
				assert(Storage::has_value());
				return Return::tick_results(*this, cancel_requested);
			}

			typename Return::expected_type& get()
			{
				return Return::get(*this);
			}

			void call_impl(std::true_type/*f(...) is void*/)
			{
				assert(!IsTask()
					&& "void f() should construct expected<void, ...> return");
				invoker().invoke();
				using Expected = typename Return::expected_type;
				Storage::emplace_once(Expected());
			}

			void call_impl(std::false_type/*f(...) is NOT void*/)
			{
				// In case f() returns reference, e.g.: T& f() -
				// move it to our storage since we have API
				// that promises to return expected<T, ...> if function
				// returns T&
				Storage::emplace_once(std::move(invoker().invoke()));
			}

			Invoker& invoker()
			{
				return static_cast<Invoker&>(*this);
			}

			const Invoker& const_invoker()
			{
				return static_cast<const Invoker&>(*this);
			}

		private:
			bool invoked_;
		};

	} // namespace detail
} // namespace nn
