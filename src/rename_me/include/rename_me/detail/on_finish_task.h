#pragma once
#include <rename_me/custom_task.h>
#include <rename_me/detail/internal_task.h>

#include <memory>
#include <cassert>

namespace nn
{

	template<typename T, typename E>
	class Task;

	namespace detail
	{
		
		template<typename RootTask, typename ResultTask, typename F>
		class OnFinishTask;

		template<typename RootT, typename RootE
			, typename ResultT, typename ResultE
			, typename F>
		class OnFinishTask<Task<RootT, RootE>, Task<ResultT, ResultE>, F>
			: public ICustomTask<ResultT, ResultE>
		{
		public:
			using RootTask = std::shared_ptr<InternalTask<RootT, RootE>>;
			using InvokedTask = std::shared_ptr<InternalTask<ResultT, ResultE>>;

			struct InvokeResult
			{
				// #TODO: variant<invoked, value>
				InvokedTask invoked_task = InvokedTask();
				ResultT value = ResultT();
				bool is_task = false;
				bool invoked = false;
			};

			using Invoker = void (*)(
				void* f/*erased F*/
				, void* root_task/*erased root task*/
				, InvokeResult* result);

		public:
			explicit OnFinishTask(RootTask root, F&& f, Invoker invoker)
				: root_((assert(root), std::move(root)))
				, f_(std::move(f))
				, status_(Status::InProgress)
				, invoker_(invoker)
				, result_()
			{
			}

			virtual void tick() override
			{
				if (root_->status() == Status::InProgress)
				{
					assert(status_ == Status::InProgress);
					return;
				}

				if (result_.invoked)
				{
					assert(status_ == Status::InProgress);
					assert(result_.is_task);
					assert(result_.invoked_task);
					status_ = result_.invoked_task->status();
					return;
				}

				invoker_(&f_, &root_, &result_);
				assert(result_.invoked);
				if (!result_.is_task)
				{
					status_ = Status::Successful;
				}
			}

			virtual Status status() const override
			{
				return status_;
			}

			virtual bool cancel() override
			{
				// #TODO: handle this
				return false;
			}

		private:
			RootTask root_;
			F f_;
			Status status_;
			Invoker invoker_;
			InvokeResult result_;
		};

	} // namespace detail
} // namespace nn
