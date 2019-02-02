#pragma once
#include <rename_me/noop_task.h>
#include <rename_me/detail/ebo_storage.h>

#include <utility>
#include <type_traits>

namespace nn
{

	template<typename UserContext>
	struct LoopContext
	{
		LoopContext(Scheduler& scheduler)
			: scheduler_(scheduler)
			, data_()
			, index_(0)
		{
		}

		template<typename... Args>
		LoopContext(Scheduler& scheduler, Args&&... args)
			: scheduler_(scheduler)
			, data_(std::forward<Args>(args)...)
			, index_(0)
		{
		}

		Scheduler& scheduler()            { return scheduler_; }
		UserContext& data()               { return data_;  }
		std::size_t index() const         { return index_; }
		void set_index(std::size_t index) { index_ = index; }

	private:
		Scheduler& scheduler_;
		UserContext data_;
		std::size_t index_;
	};

	template<>
	struct LoopContext<void>
	{
		LoopContext(Scheduler& scheduler)
			: scheduler_(scheduler)
			, index_(0)
		{
		}

		Scheduler& scheduler()            { return scheduler_; }
		std::size_t index() const         { return index_; }
		void set_index(std::size_t index) { index_ = index; }

	private:
		Scheduler& scheduler_;
		std::size_t index_;
	};

	template<typename UserData>
	auto make_loop_context(Scheduler& scheduler, UserData&& data)
	{
		using Type = LoopContext<std::remove_reference_t<UserData>>;
		return Type(scheduler, UserData(std::forward<UserData>(data)));
	}

	auto make_loop_context(Scheduler& scheduler)
	{
		return LoopContext<void>(scheduler);
	}

	namespace detail
	{

		// Makes task to return what UserContext is
		template<typename UserContext>
		struct AsContextResultTask
		{
			using Result = expected<std::remove_reference_t<UserContext>, void>;

			template<typename T>
			Result operator()(LoopContext<UserContext>& context
				, T&& last_task, Status status) const
			{
				(void)last_task;
				(void)context;
				if (status == Status::Successful)
				{
					return Result(std::move(context.data()));
				}
				return MakeExpectedWithDefaultError<Result>();
			}
		};

		template<>
		struct AsContextResultTask<void>
		{
			using Result = expected<void, void>;

			template<typename T>
			Result operator()(LoopContext<void>& context, T&& last_task, Status status) const
			{
				(void)last_task;
				(void)context;
				if (status == Status::Successful)
				{
					return Result();
				}
				return MakeExpectedWithDefaultError<Result>();
			}
		};

		template<typename UserContext>
		struct AlwaysStartTask
		{
			bool operator()(LoopContext<UserContext>& context) const
			{
				(void)context;
				return true;
			}
		};

		template<typename UserContext>
		struct AlwaysContinueHandler
		{
			template<typename T>
			bool operator()(LoopContext<UserContext>& context, T&& task) const
			{
				(void)context;
				(void)task;
				return true;
			}
		};

		template<typename UserContext>
		struct NoopUserTask
		{
			Task<> operator()(LoopContext<UserContext>& context) const
			{
				return make_task(success, context.scheduler());
			}
		};

		template<
			typename UserContext
			, typename F
			, typename OnFinish
			, typename OnBeforeCreate
			, typename OnAllFinish>
		struct NN_EBO_CLASS ForLoopTask
			: EboStorage<F>
			, EboStorage<OnFinish>
			, EboStorage<OnBeforeCreate>
			, EboStorage<OnAllFinish>
		{
			using task_type = decltype(std::declval<F&>()(
				std::declval<LoopContext<UserContext>&>()/*context*/));
			static_assert(is_task<task_type>(), "");

			using expected_data = decltype(std::declval<OnAllFinish&>()(
				  std::declval<LoopContext<UserContext>&>()/*context*/
				, std::declval<task_type&>()/*task*/
				, Status()/*status*/));
			static_assert(is_expected<expected_data>(), "");

			using final_task_type = typename task_from_expected<expected_data>::type;

			explicit ForLoopTask(LoopContext<UserContext>&& context
				, F f
				, OnFinish on_task_finish
				, OnBeforeCreate on_before_create
				, OnAllFinish on_all_finish)
					: EboStorage<F>(std::move(f))
					, EboStorage<OnFinish>(std::move(on_task_finish))
					, EboStorage<OnBeforeCreate>(std::move(on_before_create))
					, EboStorage<OnAllFinish>(std::move(on_all_finish))
					, context_(std::move(context))
					, task_()
					, data_()
					, canceled_(false)
			{
			}

			Status tick(bool cancel_requested)
			{
				if (cancel_requested)
				{
					// If task was start - cancel and wait for finish
					if (task_.is_valid())
					{
						// In case inner `task_` ignores cancel() request
						// we will wait for the finish and then
						// terminate our execution
						canceled_ = true;
						task_.try_cancel();
						if (task_.is_finished())
						{
							// Ok, everything finished already, we are done
							return finish_all(Status::Canceled);
						}
						// Wait for inner `task_`
						return Status::InProgress;
					}
					return finish_all(Status::Canceled);
				}

				bool do_start = true;
				if (task_.is_valid())
				{
					switch (task_.status())
					{
						case nn::Status::Canceled:
						{
							return finish_all(Status::Canceled);
						}
						case nn::Status::Failed:
						{
							return finish_all(Status::Failed);
						}
						case nn::Status::Successful:
						{
							do_start = finish_current();
							break;
						}
						case nn::Status::InProgress:
						{
							return nn::Status::InProgress;
						}
					}
				}

				// Either was canceled before
				if (canceled_)
				{
					return finish_all(Status::Canceled);
				}
				// Or caller says not to start from finish callback
				// or caller does not want to start at all in start callback
				if (!do_start || !start_new())
				{
					return finish_all(Status::Successful);
				}
				return nn::Status::InProgress;
			}

			[[nodiscard]] Status finish_all(Status with_status)
			{
				assert(with_status != Status::InProgress);
				auto& all_finish = EboStorage<OnAllFinish>::get();
				// Called once. Move all and get the final data
				data_ = std::move(all_finish)(context_, /*as_const*/task_, with_status);
				assert(((with_status == Status::Successful) && (data_.has_value()))
					|| !data_.has_value());
				return with_status;
			}

			[[nodiscard]] bool finish_current()
			{
				auto& on_finish = EboStorage<OnFinish>::get();
				const bool start = on_finish(context_, /*as_const*/task_);
				context_.set_index(context_.index() + 1);
				return start;
			}

			[[nodiscard]] bool start_new()
			{
				auto& should_start = EboStorage<OnBeforeCreate>::get();
				if (should_start(context_))
				{
					auto& f = EboStorage<F>::get();
					assert(!task_.is_valid() || task_.is_finished());
					task_ = f(context_);
					return true;
				}
				return false;
			}

			expected_data& get()
			{
				return data_;
			}

		private:
			LoopContext<UserContext> context_;
			task_type task_;
			expected_data data_;
			bool canceled_;
		};

	} // namespace detail

	// #TODO: constrain callbacks
	template<
		  typename UserContext     = void
		, typename F               = detail::NoopUserTask<UserContext>
		, typename OnFinish        = detail::AlwaysContinueHandler<UserContext>
		, typename OnBeforeCreate  = detail::AlwaysStartTask<UserContext>
		, typename OnAllFinish     = detail::AsContextResultTask<UserContext>>
	auto make_for_loop_task(LoopContext<UserContext>&& context
		, F&& task_creator           = F()              // Task<...> (LoopContext<UserContext>&)
		, OnFinish&& task_finish     = OnFinish()       // bool /*continue*/ (LoopContext<UserContext>&, const Task<...>&)
		, OnBeforeCreate&& on_before = OnBeforeCreate() // bool /*start*/ (LoopContext<UserContext>&)
		, OnAllFinish&& all_finish   = OnAllFinish())   // expected<...> (LoopContext<UserContext>&, const Task<...>& last_task, Status)
	{
		using TaskImpl = detail::ForLoopTask<
			std::remove_reference_t<UserContext>
			, std::remove_reference_t<F>
			, std::remove_reference_t<OnFinish>
			, std::remove_reference_t<OnBeforeCreate>
			, std::remove_reference_t<OnAllFinish>>;
		using Task = typename TaskImpl::final_task_type;

		Scheduler& scheduler = context.scheduler();
		return Task::template make<TaskImpl>(scheduler
			, std::move(context)
			, std::forward<F>(task_creator)
			, std::forward<OnFinish>(task_finish)
			, std::forward<OnBeforeCreate>(on_before)
			, std::forward<OnAllFinish>(all_finish));
	}

	template<
		  typename F               = detail::NoopUserTask<void>
		, typename OnFinish        = detail::AlwaysContinueHandler<void>
		, typename OnBeforeCreate  = detail::AlwaysStartTask<void>
		, typename OnAllFinish     = detail::AsContextResultTask<void>>
	auto make_for_loop_task(LoopContext<void>&& context
		, F&& task_creator           = F()              // Task<...> (LoopContext<void>&)
		, OnFinish&& task_finish     = OnFinish()       // bool /*continue*/ (LoopContext<void>&, const Task<...>&)
		, OnBeforeCreate&& on_before = OnBeforeCreate() // bool /*start*/ (LoopContext<void>&)
		, OnAllFinish&& all_finish   = OnAllFinish())   // expected<...> (LoopContext<void>&, const Task<...>& last_task, Status)
	{
		return make_for_loop_task<void>(std::move(context)
			, std::forward<F>(task_creator)
			, std::forward<OnFinish>(task_finish)
			, std::forward<OnBeforeCreate>(on_before)
			, std::forward<OnAllFinish>(all_finish));
	}

	template<
		  typename UserContext  = void
		, typename F            = detail::NoopUserTask<UserContext>
		, typename OnFinish     = detail::AlwaysContinueHandler<UserContext>
		, typename OnAllFinish  = detail::AsContextResultTask<UserContext>>
	auto make_forever_loop_task(LoopContext<UserContext>&& context
		, F&& task_creator         = F()            // Task<...> (LoopContext<void>&)
		, OnFinish&& task_finish   = OnFinish()     // bool /*continue*/ (LoopContext<void>&, const Task<...>&)
		, OnAllFinish&& all_finish = OnAllFinish()) // expected<...> (LoopContext<void>, const Task<...>& last_task, Status)
	{
		return make_for_loop_task(std::move(context)
			, std::forward<F>(task_creator)
			, std::forward<OnFinish>(task_finish)
			, detail::AlwaysStartTask<UserContext>()
			, std::forward<OnAllFinish>(all_finish));
	}

	template<
		  typename F            = detail::NoopUserTask<void>
		, typename OnFinish     = detail::AlwaysContinueHandler<void>
		, typename OnAllFinish  = detail::AsContextResultTask<void>>
	auto make_forever_loop_task(LoopContext<void>&& context
		, F&& task_creator         = F()            // Task<...> (LoopContext<UserContext>&)
		, OnFinish&& task_finish   = OnFinish()     // bool /*continue*/ (LoopContext<UserContext>&, const Task<...>&)
		, OnAllFinish&& all_finish = OnAllFinish()) // expected<...> (LoopContext<UserContext>&, const Task<...>& last_task, Status)
	{
		return make_for_loop_task(std::move(context)
			, std::forward<F>(task_creator)
			, std::forward<OnFinish>(task_finish)
			, detail::AlwaysStartTask<void>()
			, std::forward<OnAllFinish>(all_finish));
	}

} // namespace nn

