#pragma once

namespace nn
{

	enum class Status
	{
		InProgress,
		Finished
	};

	template<typename T, typename E>
	class ICustomTask
	{
	public:
		virtual ~ICustomTask() = default;

		virtual void tick() = 0;
		virtual Status status() const = 0;
	};

} // namespace nn
