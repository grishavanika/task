#pragma once
#include <rename_me/custom_task.h>
#include <rename_me/detail/ref_count_ptr.h>

#include <atomic>
#include <limits>

#include <cassert>

namespace nn
{

	class Scheduler;

	namespace detail
	{

		class ErasedTaskBase
		{
		public:
			virtual ~ErasedTaskBase() = default;

			virtual Status update() = 0;

		public:
			// Pointer interface implementation
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

		static_assert(sizeof(ErasedTaskBase) <= 2 * sizeof(void*)
			, "Expecting ErasedTask to be virtual pointer + reference count with "
			"alignment no more then 2 pointers");

		using ErasedTask = RefCountPtr<ErasedTaskBase>;

		template<typename D>
		class InternalTask : public ErasedTaskBase
		{
		public:
			virtual Scheduler& scheduler() = 0;
			virtual void cancel() = 0;
			virtual Status status() const = 0;
			virtual D& get_data() = 0;
		};

		template<typename D, typename CustomTask>
		class InternalCustomTask final
			: public InternalTask<D>
		{
			using Base = InternalTask<D>;
			static_assert(IsCustomTask<CustomTask, D>::value
				, "CustomTask should satisfy CustomTask<D> interface");
		public:
			template<typename... Args>
			explicit InternalCustomTask(Scheduler& scheduler, Args&&... args);

			CustomTask& task();

			virtual Scheduler& scheduler() override;
			virtual Status update() override;
			virtual void cancel() override;
			virtual Status status() const override;
			virtual D& get_data() override;

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

		template<typename D, typename CustomTask>
		template<typename... Args>
		/*explicit*/ InternalCustomTask<D, CustomTask>::InternalCustomTask(
			Scheduler& scheduler, Args&&... args)
			: scheduler_(scheduler)
			, task_(std::forward<Args>(args)...)
		{
			set_custom_status(HasInitialStatus<CustomTask>());
		}

		template<typename D, typename CustomTask>
		CustomTask& InternalCustomTask<D, CustomTask>::task()
		{
			return task_;
		}

		template<typename D, typename CustomTask>
		Scheduler& InternalCustomTask<D, CustomTask>::scheduler()
		{
			return scheduler_;
		}

		template<typename D, typename CustomTask>
		Status InternalCustomTask<D, CustomTask>::update()
		{
			assert(Base::last_run_ == Status::InProgress);
			const bool cancel_requested = Base::try_cancel_;
			const Status status = task().tick(ExecutionContext{scheduler_, cancel_requested});
#if !defined(NDEBUG)
			validate_data_state(status);
#endif
			Base::try_cancel_ = false;
			Base::last_run_ = status;
			return status;
		}

		template<typename D, typename CustomTask>
		Status InternalCustomTask<D, CustomTask>::status() const
		{
			return Base::last_run_;
		}

		template<typename D, typename CustomTask>
		void InternalCustomTask<D, CustomTask>::cancel()
		{
			Base::try_cancel_.store(true);
		}

		template<typename D, typename CustomTask>
		D& InternalCustomTask<D, CustomTask>::get_data()
		{
			assert(Base::last_run_ != Status::InProgress);
			return task().get();
		}

		template<typename D, typename CustomTask>
		void InternalCustomTask<D, CustomTask>::set_custom_status(std::true_type)
		{
			assert(Base::last_run_ == Status::InProgress);
			Base::last_run_ = task().initial_status();
		}

		template<typename D, typename CustomTask>
		void InternalCustomTask<D, CustomTask>::set_custom_status(std::false_type)
		{
			assert(Base::last_run_ == Status::InProgress);
		}

#if !defined(NDEBUG)
		template<typename D, typename CustomTask>
		void InternalCustomTask<D, CustomTask>::validate_data_state(Status status)
		{
			switch (status)
			{
			case Status::InProgress:
				break;
			case Status::Failed:
			case Status::Canceled:
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
