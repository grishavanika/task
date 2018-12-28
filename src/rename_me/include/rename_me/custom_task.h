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

		virtual Status tick() = 0;
		virtual bool cancel() = 0;
		virtual expected<T, E>& get() = 0;
	};

} // namespace nn
