#pragma once
#include <rename_me/detail/config.h>

namespace nn
{
	namespace detail
	{

		template<typename T>
		struct IsEboEnabled : std::integral_constant<bool
			, std::is_empty<T>::value
			&& !std::is_final<T>::value> { };

		template<bool IsEbo, typename T>
		struct NN_EBO_CLASS EboStorageImpl : private T
		{
			explicit EboStorageImpl(T&& f)
				: T(std::move(f)) { }

			T& get() { return *this; }
		};

		template<typename T>
		struct EboStorageImpl<false/*No EBO*/, T>
		{
			EboStorageImpl(T&& v)
				: value_(std::move(v)) { }

			T& get() { return value_; }

		private:
			T value_;
		};

		template<typename T>
		struct NN_EBO_CLASS EboStorage :
			EboStorageImpl<IsEboEnabled<T>::value, T>
		{
			static_assert(std::is_same_v<remove_cvref_t<T>, T>
				, "T should not be cv-reference");
			using Base = EboStorageImpl<IsEboEnabled<T>::value, T>;
			using Base::Base;
		};
	} // namespace detail
} // namespace nn
