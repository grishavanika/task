#pragma once
#include <rename_me/storage.h>

namespace nn
{

	enum class Status
	{
		InProgress,
		Successful,
		Failed,
	};

	struct State
	{
		Status status;
		bool canceled;

		State(Status s, bool do_cancel = false)
			: status(s)
			, canceled(do_cancel)
		{
		}
	};

	template<typename T, typename E>
	class ICustomTask
	{
	public:
		virtual ~ICustomTask() = default;

		// Invoked on Scheduler's thread.
		virtual State tick(bool cancel_requested) = 0;
		// Implementation needs to guaranty that returned value
		// is "some" valid value once tick() returns finished status.
		// To be thread-safe, get() value needs to be set before
		// finish status returned from tick().
		virtual expected<T, E>& get() = 0;
	};

} // namespace nn
