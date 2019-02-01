#pragma once
#include <nonstd/expected.hpp>
#include <optional>

namespace nn
{

	// #TODO: make consistent interfcace.
	// Behavior: expected<void, void> (or any combination of void and T)
	// should be valid with well-defined behavior.
	template<typename T, typename E>
	struct expected : nonstd::expected<T, E>
	{
		using Base = nonstd::expected<T, E>;
		using Base::Base;
	};

	template<typename E>
	using unexpected = nonstd::unexpected_type<E>;

	struct unexpected_void {};

	template<typename T>
	struct expected<T, void> : std::optional<T>
	{
		using Base = std::optional<T>;
		using Base::Base;
	};

	template<>
	struct expected<void, void>
	{
		explicit expected()
			: ok_(true)
		{
		}

		explicit expected(unexpected_void)
			: ok_(false)
		{
		}

		bool has_value() const
		{
			return ok_;
		}

	private:
		bool ok_;
	};

} // namespace nn
