#pragma once

namespace nn
{

	template<typename T>
	struct DataTraits;

	template<typename T>
	struct DataTraits
	{
		using value_type = T;
		using error_type = void;
		using type = T;

		static type make()
		{
			return type();
		}

		constexpr static value_type& get_value(type& data) noexcept
		{
			return data;
		}

		constexpr static error_type get_error(type& data) noexcept
		{
			(void)data;
		}

		constexpr static bool has_value(const type& data) noexcept
		{
			(void)data;
			return true;
		}

		constexpr static bool has_error(const type& data) noexcept
		{
			(void)data;
			return false;
		}

		constexpr static void set_cancel_error(type& data) noexcept
		{
			(void)data;
		}
	};

	template<>
	struct DataTraits<void>
	{
		using value_type = void;
		using error_type = void;
		using type = void;
	};

} // namespace nn
