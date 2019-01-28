#pragma once
#include <rename_me/custom_task.h>
#include <rename_me/detail/ref_count_ptr.h>

#include <atomic>

#include <cassert>

namespace nn
{

	class Scheduler;

	namespace detail
	{

		class TaskBase
		{
		public:
			virtual ~TaskBase() = default;

			virtual Status update() = 0;

		public:
			// Pointer interface
			bool remove_ref_count() noexcept
			{
				assert(ref_ != 0);
				return (ref_.fetch_sub(1) == 0);
			}

			void add_ref_count() noexcept
			{
				assert(ref_ != std::numeric_limits<std::uint16_t>::max());
				ref_.fetch_add(1);
			}

		protected:
			std::atomic<std::uint16_t> ref_ = 1;
			// Put there for better memory layout
			std::atomic<Status> last_run_ = Status::InProgress;
			std::atomic_bool try_cancel_ = false;
			// char alignment[4]; // For x64
		};

		static_assert(sizeof(TaskBase) <= 2 * sizeof(void*)
			, "Expecting TaskBase to be virtual pointer + reference count with "
			"alignment no more then 2 pointers");

		using ErasedTask = RefCountPtr<TaskBase>;

		template<typename T, typename E>
		class InternalTask : public TaskBase
		{
		public:
			virtual Scheduler& scheduler() = 0;
			virtual void cancel() = 0;
			virtual Status status() const = 0;
			virtual expected<T, E>& get_data() = 0;
		};

		template<typename T, typename E, typename CustomTask>
		class InternalCustomTask final
			: public InternalTask<T, E>
		{
			using Base = InternalTask<T, E>;
			static_assert(IsCustomTask<CustomTask, T, E>::value
				, "CustomTask should satisfy CustomTask<T, E> interface");
		public:
			template<typename... Args>
			explicit InternalCustomTask(Scheduler& scheduler, Args&&... args);

			CustomTask& task();

			virtual Scheduler& scheduler() override;
			virtual Status update() override;
			virtual void cancel() override;
			virtual Status status() const override;
			virtual expected<T, E>& get_data() override;

			void set_custom_status(std::true_type);
			void set_custom_status(std::false_type);

#if !defined(NDEBUG)
			void validate_data_state(Status status);
#endif
		private:
			Scheduler& scheduler_;
			CustomTask task_;
		};

	} // namespace detail
} // namespace nn


namespace nn
{
	namespace detail
	{

		template<typename T, typename E, typename CustomTask>
		template<typename... Args>
		/*explicit*/ InternalCustomTask<T, E, CustomTask>::InternalCustomTask(
			Scheduler& scheduler, Args&&... args)
			: scheduler_(scheduler)
			, task_(std::forward<Args>(args)...)
		{
			set_custom_status(HasInitialStatus<CustomTask>());
		}

		template<typename T, typename E, typename CustomTask>
		CustomTask& InternalCustomTask<T, E, CustomTask>::task()
		{
			return task_;
		}

		template<typename T, typename E, typename CustomTask>
		Scheduler& InternalCustomTask<T, E, CustomTask>::scheduler()
		{
			return scheduler_;
		}

		template<typename T, typename E, typename CustomTask>
		Status InternalCustomTask<T, E, CustomTask>::update()
		{
			assert(Base::last_run_ == Status::InProgress);
			const bool cancel_requested = Base::try_cancel_;
			const Status status = task().tick(cancel_requested);
#if !defined(NDEBUG)
			validate_data_state(status);
#endif
			Base::try_cancel_ = false;
			Base::last_run_ = status;
			return status;
		}

		template<typename T, typename E, typename CustomTask>
		Status InternalCustomTask<T, E, CustomTask>::status() const
		{
			return Base::last_run_;
		}

		template<typename T, typename E, typename CustomTask>
		void InternalCustomTask<T, E, CustomTask>::cancel()
		{
			Base::try_cancel_.store(true);
		}

		template<typename T, typename E, typename CustomTask>
		expected<T, E>& InternalCustomTask<T, E, CustomTask>::get_data()
		{
			assert(Base::last_run_ != Status::InProgress);
			return task().get();
		}

		template<typename T, typename E, typename CustomTask>
		void InternalCustomTask<T, E, CustomTask>::set_custom_status(std::true_type)
		{
			assert(Base::last_run_ == Status::InProgress);
			Base::last_run_ = task().initial_status();
		}

		template<typename T, typename E, typename CustomTask>
		void InternalCustomTask<T, E, CustomTask>::set_custom_status(std::false_type)
		{
			assert(Base::last_run_ == Status::InProgress);
		}

#if !defined(NDEBUG)
		template<typename T, typename E, typename CustomTask>
		void InternalCustomTask<T, E, CustomTask>::validate_data_state(Status status)
		{
			switch (status)
			{
			case Status::Canceled:
				// Allowed to leave expected<T, E> in unspecified state,
				// but expected<> should be valid/constructed object
				assert(task().get().has_value()
					|| !task().get().has_value());
				break;
			case Status::InProgress:
				break;
			case Status::Failed:
				// Error should be set
				assert(!task().get().has_value());
				break;
			case Status::Successful:
				// Value should be set
				assert(task().get().has_value());
				break;
			}
		}
#endif

	} // namespace detail
} // namespace nn
