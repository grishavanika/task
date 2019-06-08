#pragma once
#include <type_traits>

namespace nn
{
	namespace detail
	{

		template<typename T>
		struct remove_cvref
		{
			using type = typename std::remove_cv<
				typename std::remove_reference<T>::type>::type;
		};

		template<typename T>
		using remove_cvref_t = typename remove_cvref<T>::type;

	} // namespace detail
} // namespace nn
