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
		
		template<typename RootTask, typename ResultTask, typename F, typename IsTask>
		class OnFinishTask;

		template<typename RootT, typename RootE
			, typename ResultT, typename ResultE
			, typename F
			, typename IsTask>
		class OnFinishTask<Task<RootT, RootE>, Task<ResultT, ResultE>, F, IsTask> final
			: public ICustomTask<ResultT, ResultE>
		{
		public:
			using RootTask = std::shared_ptr<InternalTask<RootT, RootE>>;
			using InvokedTask = std::shared_ptr<InternalTask<ResultT, ResultE>>;

			struct InvokeResult
			{
				// #TODO: variant<invoked_task, value>
				InvokedTask invoked_task = InvokedTask();
				expected<ResultT, void> value;
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
				, invoker_(invoker)
				, result_()
			{
			}

			virtual Status tick() override
			{
				if (root_->status() == Status::InProgress)
				{
					return Status::InProgress;
				}

				if (result_.invoked)
				{
					assert(result_.invoked_task);
					return result_.invoked_task->status();
				}

				invoker_(&f_, &root_, &result_);
				assert(result_.invoked);
				return (IsTask() ? Status::InProgress : Status::Successful);
			}

			virtual bool cancel() override
			{
				// #TODO: handle this
				return false;
			}

			virtual expected<ResultT, ResultE>& get() override
			{
				if (IsTask())
				{
					return result_.invoked_task->get();
				}
				return result_.value;
			}

		private:
			RootTask root_;
			// #TODO: EBO, do better layout
			F f_;
			Invoker invoker_;
			InvokeResult result_;
		};

	} // namespace detail
} // namespace nn
