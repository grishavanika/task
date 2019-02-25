#pragma once
#include <rename_me/task.h>
#include <rename_me/detail/ebo_storage.h>
#include <rename_me/detail/lazy_storage.h>

#include <utility>
#include <type_traits>

namespace nn
{
	template<typename UserContext>
	struct Context
	{
		Context(Scheduler& scheduler)
			: scheduler_(scheduler)
			, data_()
		{
		}

		template<typename... Args>
		Context(Scheduler& scheduler, Args&&... args)
			: scheduler_(scheduler)
			, data_(std::forward<Args>(args)...)
		{
		}

		Scheduler& scheduler() { return scheduler_; }
		UserContext& data()    { return data_; }

	private:
		Scheduler& scheduler_;
		UserContext data_;
	};

	template<>
	struct Context<void>
	{
		Context(Scheduler& scheduler)
			: scheduler_(scheduler)
		{
		}

		Scheduler& scheduler() { return scheduler_; }

	private:
		Scheduler& scheduler_;
	};

	template<typename UserData>
	auto make_context(Scheduler& scheduler, UserData&& data)
	{
		using Type = Context<std::remove_reference_t<UserData>>;
		return Type(scheduler, UserData(std::forward<UserData>(data)));
	}

	auto make_context(Scheduler& scheduler)
	{
		return Context<void>(scheduler);
	}

	namespace detail
	{

		template<typename UserContext>
		struct NoopInplaceTask
		{
			Status operator()(Context<UserContext>& context, bool cancel) const
			{
				(void)context;
				return cancel ? Status::Canceled : Status::Successful;
			}

			expected<UserContext, void> operator()(Context<UserContext>& context) const
			{
				return expected<UserContext, void>(std::move(context.data()));
			}
		};

		template<>
		inline expected<void, void> NoopInplaceTask<void>::operator()
			(Context<void>& context) const
		{
			(void)context;
			return expected<void, void>();
		}

		struct TickTag {};
		struct FinishTag {};

		template<
			typename UserContext
			, typename Tick
			, typename Finish>
		struct NN_EBO_CLASS InplaceTask
			: EboStorage<Tick, TickTag>
			, EboStorage<Finish, FinishTag>
		{
			using expected_data = decltype(std::declval<Finish&>()(
				std::declval<Context<UserContext>&>()/*context*/));
			static_assert(is_expected<expected_data>(), "");
			
			using task_type = typename task_from_expected<expected_data>::type;

			using TickBase = EboStorage<Tick, TickTag>;
			using FinishBase = EboStorage<Finish, FinishTag>;

			explicit InplaceTask(Context<UserContext>&& context
				, Tick tick
				, Finish finish)
					: TickBase(std::move(tick))
					, FinishBase(std::move(finish))
					, context_(std::move(context))
					, data_()
			{
			}

			Status tick(const ExecutionContext& exec)
			{
				const Status status = TickBase::get()(context_, exec.cancel_requested);
				switch (status)
				{
				case Status::InProgress:
					// Continue, nothing to do
					break;
				case Status::Successful:
					data_.emplace_once(FinishBase::get()(context_));
					assert(get().has_value());
					break;
				case Status::Canceled:
					// [[fallthrough]]
				case Status::Failed:
					data_.emplace_once(FinishBase::get()(context_));
					assert(!get().has_value());
					break;
				}
				return status;
			}

			expected_data& get()
			{
				return data_.get();
			}

		private:
			// Looks like Context does not need Scheduler.
			// Remove and use EBO
			Context<UserContext> context_;
			// #TODO: EBO ?
			LazyStorage<expected_data> data_;
		};
	} // namespace detail

	template<
		  typename UserContext = void
		, typename Tick        = detail::NoopInplaceTask<UserContext>
		, typename Finish      = detail::NoopInplaceTask<UserContext>>
	auto make_in_place_task(Context<UserContext>&& context
		, Tick&& on_tick     = Tick()    // Status (Context<UserContext>&, const ExecutionContext&)
		, Finish&& on_finish = Finish()) // expected<...> (Context<UserContext>&)
	{
		using TaskImpl = detail::InplaceTask<
			  std::remove_reference_t<UserContext>
			, std::remove_reference_t<Tick>
			, std::remove_reference_t<Finish>>;
		using Task = typename TaskImpl::task_type;

		Scheduler& scheduler = context.scheduler();
		return Task::template make<TaskImpl>(scheduler
			, std::move(context)
			, std::forward<Tick>(on_tick)
			, std::forward<Finish>(on_finish));
	}

	template<
		  typename Tick        = detail::NoopInplaceTask<void>
		, typename Finish      = detail::NoopInplaceTask<void>>
	auto make_in_place_task(Context<void>&& context
		, Tick&& on_tick     = Tick()    // Status (Context<UserContext>&, const ExecutionContext&)
		, Finish&& on_finish = Finish()) // expected<...> (Context<UserContext>&)
	{
		return make_in_place_task<void>(std::move(context)
			, std::forward<Tick>(on_tick), std::forward<Finish>(on_finish));
	}

} // namespace nn
