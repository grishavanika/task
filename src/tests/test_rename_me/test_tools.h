#pragma once
#include <rename_me/custom_task.h>

#include <ostream>

namespace nn
{

	inline std::ostream& operator<<(std::ostream& o, Status s)
	{
		switch (s)
		{
		case Status::InProgress:
			o << "InProgress";
			break;
		case Status::Successful:
			o << "Successful";
			break;
		case Status::Failed:
			o << "Failed";
			break;
		case Status::Canceled:
			o << "Canceled";
			break;
		}
		return o;
	}

} // namespace nn
