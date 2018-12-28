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

	template<typename T, typename E>
	class ICustomTask
	{
	public:
		virtual ~ICustomTask() = default;

		// Invoked on Scheduler's thread.
		// Implementation needs not care about cancel() and tick()
		// thread-safety: it's handled internally (e.g.,
		// if cancel() was requested by the client, cancel() will
		// be invoked right before tick(), from the same thread).
		virtual Status tick() = 0;
		virtual bool cancel() = 0;
		// Implementation needs to guaranty that returned value
		// is "some" valid value once tick() returns finished status.
		// To be thread-safe, get() value needs to be set before
		// finish status returned from tick().
		virtual expected<T, E>& get() = 0;
	};

} // namespace nn
