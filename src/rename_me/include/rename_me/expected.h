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

	template<typename T>
	struct expected<T, void> : std::optional<T>
	{
		using Base = std::optional<T>;
		using Base::Base;
	};

	template<>
	struct expected<void, void>
	{
		bool has_value() const
		{
			return true;
		}
	};

	template<typename E>
	using unexpected = nonstd::unexpected_type<E>;

} // namespace nn
